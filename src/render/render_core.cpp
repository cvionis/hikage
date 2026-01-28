#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

// @Note: Temporary and stupid
#define SCENE_MODELS_COUNT 256
#define SCENE_MATERIALS_COUNT 256

#define R_D3D12_FRAME_CBV_COUNT 1
#define R_D3D12_DRAW_CBV_COUNT  1
#define R_D3D12_CBV_COUNT       (R_D3D12_FRAME_CBV_COUNT + R_D3D12_DRAW_CBV_COUNT)
#define R_D3D12_TEXTURE_MAX     1024
#define R_D3D12_SRV_HEAP_SIZE   (R_D3D12_CBV_COUNT + R_D3D12_TEXTURE_MAX)

#define R_D3D12_FRAME_CBV_SLOT  0
#define R_D3D12_DRAW_CBV_SLOT   1
#define R_D3D12_TEXTURE_TABLE_BASE R_D3D12_CBV_COUNT

static void
r_wait_for_previous_frame(void)
{
  R_Context *ctx = &r_ctx;

  // Signal GPU to mark current work complete using this fence value.
  U64 fence_to_signal = ctx->fence_values[ctx->frame_idx];
  HRESULT hr = ctx->command_queue->Signal(ctx->fence, fence_to_signal);
  Assert(SUCCEEDED(hr));

  // Advance to the next back buffer index.
  ctx->frame_idx = ctx->swapchain->GetCurrentBackBufferIndex();

  // If the GPU hasn't finished processing this frame yet, wait for the fence event.
  if (ctx->fence->GetCompletedValue() < ctx->fence_values[ctx->frame_idx]) {
    hr = ctx->fence->SetEventOnCompletion(ctx->fence_values[ctx->frame_idx], ctx->fence_event);
    Assert(SUCCEEDED(hr));
    WaitForSingleObjectEx(ctx->fence_event, INFINITE, FALSE);
  }

  // Prepare fence value for the next frame.
  ctx->fence_values[ctx->frame_idx] = fence_to_signal + 1;
}

static IDXGIAdapter1 *
r_d3d12_get_hardware_adapter(IDXGIFactory1 *factory)
{
  IDXGIAdapter1 *adapter = 0;
  IDXGIFactory6 *factory6 = 0;

  if (SUCCEEDED(factory->QueryInterface(IID_PPV_ARGS(&factory6)))) {
    for (UINT adapter_idx = 0;
         SUCCEEDED(factory6->EnumAdapterByGpuPreference(adapter_idx, DXGI_GPU_PREFERENCE_UNSPECIFIED, IID_PPV_ARGS(&adapter)));
         adapter_idx += 1) {
      DXGI_ADAPTER_DESC1 desc;
      adapter->GetDesc1(&desc);

      if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
        adapter->Release();
        adapter = 0;
        continue;
      }

      if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), 0))) {
        break;
      }

      adapter->Release();
      adapter = 0;
    }
    factory6->Release();
  }

  if (adapter == 0) {
    for (UINT adapter_idx = 0; SUCCEEDED(factory->EnumAdapters1(adapter_idx, &adapter)); adapter_idx += 1) {
      DXGI_ADAPTER_DESC1 desc;
      adapter->GetDesc1(&desc);

      if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
        adapter->Release();
        adapter = 0;
        continue;
      }

      if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), 0))) {
        break;
      }

      adapter->Release();
      adapter = 0;
    }
  }

  return adapter;
}

struct R_FrameCB {
  Mat4x4 viewproj;
  V4F32  camera_ws;
};

struct R_DrawCB {
  Mat4x4 model;
  Mat4x4 normal;
};

static void
r_init(OS_Handle window)
{
  R_Context *ctx = &r_ctx;
  ctx->arena = arena_alloc_default();

  HWND hwnd = os_win32_window_from_handle(window)->hwnd;

  // @Todo: Temp
  ctx->width = 1280;
  ctx->height = 720;

  HRESULT hr;
  UINT dxgi_factory_flags = 0;

#if BUILD_DEBUG
  {
    ID3D12Debug *debug_controller = 0;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)))) {
      debug_controller->EnableDebugLayer();
      dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
      debug_controller->Release();
    }
  }
#endif

  IDXGIFactory4 *factory = 0;
  hr = CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&factory));
  Assert(SUCCEEDED(hr));

  IDXGIAdapter1 *hardware_adapter = r_d3d12_get_hardware_adapter(factory);
  hr = D3D12CreateDevice(hardware_adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&ctx->device));
  Assert(SUCCEEDED(hr));
  if (hardware_adapter) hardware_adapter->Release();

  D3D12_COMMAND_QUEUE_DESC queue_desc = {};
  queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  hr = ctx->device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&ctx->command_queue));
  Assert(SUCCEEDED(hr));

  DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
  swap_chain_desc.BufferCount = R_D3D12_FRAME_COUNT;
  swap_chain_desc.Width = ctx->width;
  swap_chain_desc.Height = ctx->height;
  swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  swap_chain_desc.SampleDesc.Count = 1;

  IDXGISwapChain1 *swap_chain = 0;
  hr = factory->CreateSwapChainForHwnd(ctx->command_queue, hwnd, &swap_chain_desc, 0, 0, &swap_chain);
  Assert(SUCCEEDED(hr));
  factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
  hr = swap_chain->QueryInterface(IID_PPV_ARGS(&ctx->swapchain));
  swap_chain->Release();
  ctx->frame_idx = ctx->swapchain->GetCurrentBackBufferIndex();

  // RTV heap
  {
    D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {};
    rtv_heap_desc.NumDescriptors = R_D3D12_FRAME_COUNT + 1;
    rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    hr = ctx->device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&ctx->rtv_heap));
    Assert(SUCCEEDED(hr));
    ctx->rtv_descriptor_size =
      ctx->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  }

  // DSV heap
  {
    D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc = {};
    dsv_heap_desc.NumDescriptors = 1;
    dsv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    hr = ctx->device->CreateDescriptorHeap(&dsv_heap_desc, IID_PPV_ARGS(&ctx->dsv_heap));
    Assert(SUCCEEDED(hr));
  }

  // Back buffers and command allocators
  {
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(ctx->rtv_heap->GetCPUDescriptorHandleForHeapStart());
    for (UINT n = 0; n < R_D3D12_FRAME_COUNT; n += 1) {
      hr = ctx->swapchain->GetBuffer(n, IID_PPV_ARGS(&ctx->render_targets[n]));
      Assert(SUCCEEDED(hr));
      ctx->device->CreateRenderTargetView(ctx->render_targets[n], 0, rtv_handle);
      rtv_handle.Offset(1, ctx->rtv_descriptor_size);

      hr = ctx->device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&ctx->command_allocators[n])
      );
      Assert(SUCCEEDED(hr));
    }
  }

  // Color buffer
  {
    D3D12_RESOURCE_DESC color_desc = {};
    color_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    color_desc.Alignment = 0;
    color_desc.Width = ctx->width;
    color_desc.Height = ctx->height;
    color_desc.DepthOrArraySize = 1;
    color_desc.MipLevels = 1;
    color_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    color_desc.SampleDesc.Count = 1;
    color_desc.SampleDesc.Quality = 0;
    color_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    color_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_CLEAR_VALUE clear_value = {};
    clear_value.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    clear_value.Color[0] = 0.95f;
    clear_value.Color[1] = 0.9f;
    clear_value.Color[2] = 0.9f;
    clear_value.Color[3] = 1.0f;

    CD3DX12_HEAP_PROPERTIES heap_props(D3D12_HEAP_TYPE_DEFAULT);
    hr = ctx->device->CreateCommittedResource(
      &heap_props,
      D3D12_HEAP_FLAG_NONE,
      &color_desc,
      D3D12_RESOURCE_STATE_RENDER_TARGET,
      &clear_value,
      IID_PPV_ARGS(&ctx->color_buffer)
    );
    Assert(SUCCEEDED(hr));

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(
      ctx->rtv_heap->GetCPUDescriptorHandleForHeapStart(),
      R_D3D12_FRAME_COUNT,
      ctx->rtv_descriptor_size
    );

    D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
    rtv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtv_desc.Texture2D.MipSlice = 0;
    rtv_desc.Texture2D.PlaneSlice = 0;

    ctx->device->CreateRenderTargetView(ctx->color_buffer, &rtv_desc, rtv);
  }

  // Depth buffer
  {
    CD3DX12_RESOURCE_DESC depth_desc =
      CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, ctx->width, ctx->height, 1, 1,
        1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

    D3D12_CLEAR_VALUE depth_clear = {};
    depth_clear.Format = DXGI_FORMAT_D32_FLOAT;
    depth_clear.DepthStencil.Depth = 1.0f;

    CD3DX12_HEAP_PROPERTIES heap_props(D3D12_HEAP_TYPE_DEFAULT);
    hr = ctx->device->CreateCommittedResource(
      &heap_props, D3D12_HEAP_FLAG_NONE, &depth_desc,
      D3D12_RESOURCE_STATE_DEPTH_WRITE, &depth_clear,
      IID_PPV_ARGS(&ctx->depth_buffer)
    );
    Assert(SUCCEEDED(hr));

    ctx->device->CreateDepthStencilView(
      ctx->depth_buffer, 0,
      ctx->dsv_heap->GetCPUDescriptorHandleForHeapStart()
    );
  }

  // Unified shader-visible heap: [frame CBVs] + [bindless textures]
  {
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heap_desc.NumDescriptors = R_D3D12_SRV_HEAP_SIZE;
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    heap_desc.NodeMask = 0;

    hr = ctx->device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&ctx->srv_heap));
    Assert(SUCCEEDED(hr));

    ctx->srv_descriptor_size =
      ctx->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    ctx->srv_next_idx = R_D3D12_TEXTURE_TABLE_BASE;
  }

  // Per-frame constant buffer (b0) stored in slot 0 of srv_heap
  {
    CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(256);

    hr = ctx->device->CreateCommittedResource(
      &heap, D3D12_HEAP_FLAG_NONE, &desc,
      D3D12_RESOURCE_STATE_GENERIC_READ, 0,
      IID_PPV_ARGS(&ctx->frame_cb)
    );
    Assert(SUCCEEDED(hr));

    hr = ctx->frame_cb->Map(0, 0, (void **)&ctx->frame_cb_mapped);
    Assert(SUCCEEDED(hr));

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbv = {};
    cbv.BufferLocation = ctx->frame_cb->GetGPUVirtualAddress();
    cbv.SizeInBytes = 256;

    CD3DX12_CPU_DESCRIPTOR_HANDLE h(
      ctx->srv_heap->GetCPUDescriptorHandleForHeapStart(),
      R_D3D12_FRAME_CBV_SLOT,
      ctx->srv_descriptor_size
    );
    ctx->device->CreateConstantBufferView(&cbv, h);
  }

  // Per-draw constant buffer (b1) stored in slot 1 of srv_heap
  {
    CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(256);

   hr = ctx->device->CreateCommittedResource(
      &heap,
      D3D12_HEAP_FLAG_NONE,
      &desc,
      D3D12_RESOURCE_STATE_GENERIC_READ,
      0,
      IID_PPV_ARGS(&ctx->draw_cb)
    );
    Assert(SUCCEEDED(hr));

    hr = ctx->draw_cb->Map(0, 0, (void **)&ctx->draw_cb_mapped);
    Assert(SUCCEEDED(hr));

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbv = {};
    cbv.BufferLocation = ctx->draw_cb->GetGPUVirtualAddress();
    cbv.SizeInBytes = 256;

    CD3DX12_CPU_DESCRIPTOR_HANDLE h(
      ctx->srv_heap->GetCPUDescriptorHandleForHeapStart(),
      R_D3D12_DRAW_CBV_SLOT,
      ctx->srv_descriptor_size
    );
    ctx->device->CreateConstantBufferView(&cbv, h);
  }

  // Root signature
  {
    CD3DX12_DESCRIPTOR_RANGE ranges[3];
    // b0: frame
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    // b1: per-draw
    ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
    // t0[]: textures
    ranges[2].Init(
      D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
      R_D3D12_TEXTURE_MAX,
      0,
      0,
      R_D3D12_TEXTURE_TABLE_BASE
    );

    CD3DX12_ROOT_PARAMETER params[3];
    params[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL);
    params[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_VERTEX);
    params[2].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_PIXEL);

    D3D12_STATIC_SAMPLER_DESC static_sampler = {};
    static_sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    static_sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    static_sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    static_sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    static_sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    static_sampler.ShaderRegister = 0; // s0
    static_sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    CD3DX12_ROOT_SIGNATURE_DESC root_sig_desc;
    root_sig_desc.Init(ArrayCount(params), params, 1, &static_sampler,
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ID3DBlob *sig_blob = 0;
    ID3DBlob *err_blob = 0;
    hr = D3D12SerializeRootSignature(
      &root_sig_desc,
      D3D_ROOT_SIGNATURE_VERSION_1,
      &sig_blob,
      &err_blob
    );
    Assert(SUCCEEDED(hr));

    hr = ctx->device->CreateRootSignature(
      0,
      sig_blob->GetBufferPointer(),
      sig_blob->GetBufferSize(),
      IID_PPV_ARGS(&ctx->root_signature)
    );
    Assert(SUCCEEDED(hr));

    sig_blob->Release();
    if (err_blob) err_blob->Release();
  }

  // Compile shaders
  ID3DBlob *vs_blob = 0;
  ID3DBlob *ps_blob = 0;
  ID3DBlob *err_blob = 0;

#if BUILD_DEBUG
  UINT compile_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
  UINT compile_flags = 0;
#endif

  hr = D3DCompileFromFile(L"../src/render/shaders/forward_basic.hlsl", 0, 0, "vs_main", "vs_5_0", compile_flags, 0, &vs_blob, &err_blob);
  if (FAILED(hr)) {
    if (err_blob) {
      OutputDebugStringA((char *)err_blob->GetBufferPointer());
      err_blob->Release();
    }
    Assert(SUCCEEDED(hr));
  }

  hr = D3DCompileFromFile(L"../src/render/shaders/forward_basic.hlsl", 0, 0, "ps_main", "ps_5_0", compile_flags, 0, &ps_blob, &err_blob);
  if (FAILED(hr)) {
    if (err_blob) {
      OutputDebugStringA((char *)err_blob->GetBufferPointer());
      err_blob->Release();
    }
    Assert(SUCCEEDED(hr));
  }

  // Pipeline state object
  U32 input_count = 4;
  ctx->input_desc[0] = {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};
  ctx->input_desc[1] = {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};
  ctx->input_desc[2] = {"TANGENT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};
  ctx->input_desc[3] = {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};

  D3D12_GRAPHICS_PIPELINE_STATE_DESC *pso_desc = &ctx->pso_desc;
  pso_desc->InputLayout = {ctx->input_desc, input_count};
  pso_desc->pRootSignature = ctx->root_signature;
  pso_desc->VS = CD3DX12_SHADER_BYTECODE(vs_blob);
  pso_desc->PS = CD3DX12_SHADER_BYTECODE(ps_blob);
  pso_desc->RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  pso_desc->BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  pso_desc->DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  pso_desc->SampleMask = UINT_MAX;
  pso_desc->PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pso_desc->NumRenderTargets = 1;
  pso_desc->RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
  pso_desc->DSVFormat = DXGI_FORMAT_D32_FLOAT;
  pso_desc->SampleDesc.Count = 1;

  D3D12_RASTERIZER_DESC rs = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  rs.FrontCounterClockwise = FALSE;
  rs.MultisampleEnable = TRUE;
  pso_desc->RasterizerState = rs;

  hr = ctx->device->CreateGraphicsPipelineState(pso_desc, IID_PPV_ARGS(&ctx->pipeline_state));
  Assert(SUCCEEDED(hr));
  vs_blob->Release();
  ps_blob->Release();

  // Main command list
  hr = ctx->device->CreateCommandList(
    0, D3D12_COMMAND_LIST_TYPE_DIRECT,
    ctx->command_allocators[ctx->frame_idx], ctx->pipeline_state,
    IID_PPV_ARGS(&ctx->command_list)
  );
  Assert(SUCCEEDED(hr));
  ctx->command_list->Close();

  // Create synchronization primitives (frame fence)
  ctx->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&ctx->fence));
  ctx->fence_event = CreateEvent(0, FALSE, FALSE, 0);
  for (S32 idx = 0; idx < R_D3D12_FRAME_COUNT; idx += 1) ctx->fence_values[idx] = 0;
  ctx->fence_values[ctx->frame_idx] = 1;

  factory->Release();

  // Resource copy command allocator
  hr = ctx->device->CreateCommandAllocator(
    D3D12_COMMAND_LIST_TYPE_DIRECT,
    IID_PPV_ARGS(&ctx->copy_cmd_allocator)
  );
  Assert(SUCCEEDED(hr));

  // Resource copy command list
  hr = ctx->device->CreateCommandList(
    0,
    D3D12_COMMAND_LIST_TYPE_DIRECT,
    ctx->copy_cmd_allocator,
    0,
    IID_PPV_ARGS(&ctx->copy_cmd_list)
  );
  Assert(SUCCEEDED(hr));
  ctx->copy_cmd_list->Close();

  // Resource upload/ready-tracking fence
  hr = ctx->device->CreateFence(
    0,
    D3D12_FENCE_FLAG_NONE,
    IID_PPV_ARGS(&ctx->copy_fence)
  );
  Assert(SUCCEEDED(hr));
  ctx->copy_fence_value = 0;
  ctx->copy_fence_event = CreateEventA(0, FALSE, FALSE, 0);
}

static void
r_render_forward(AssetContext *assets, Camera *camera, ModelInstance *models, S32 models_count)
{
  R_Context *ctx = &r_ctx;

  // Update per-frame CB (b0)
  {
    R_FrameCB cb = {
      .viewproj = camera->viewproj,
      .camera_ws = v4f32(camera->position.x, camera->position.y, camera->position.z, 0.f),
    };
    MemoryCopy(ctx->frame_cb_mapped, &cb, sizeof(cb));
  }

  // Command list setup
  CD3DX12_VIEWPORT viewport(0.f, 0.f, (F32)ctx->width, (F32)ctx->height);
  CD3DX12_RECT scissor_rect(0, 0, ctx->width, ctx->height);

  ctx->command_list->SetPipelineState(ctx->pipeline_state);
  ctx->command_list->SetGraphicsRootSignature(ctx->root_signature);
  ctx->command_list->RSSetViewports(1, &viewport);
  ctx->command_list->RSSetScissorRects(1, &scissor_rect);

  // Bind unified descriptor heap
  {
    ID3D12DescriptorHeap *heaps[] = { ctx->srv_heap };
    ctx->command_list->SetDescriptorHeaps(1, heaps);

    D3D12_GPU_DESCRIPTOR_HANDLE gpu_base =
      ctx->srv_heap->GetGPUDescriptorHandleForHeapStart();

    // Root param 0: per-frame constant buffer
    ctx->command_list->SetGraphicsRootDescriptorTable(0, gpu_base);

    // Root param 1: per-draw constant buffer
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_draw =
      CD3DX12_GPU_DESCRIPTOR_HANDLE(gpu_base, R_D3D12_DRAW_CBV_SLOT, ctx->srv_descriptor_size);
    ctx->command_list->SetGraphicsRootDescriptorTable(1, gpu_draw);

    // Root param 2: texture table
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_tex = gpu_base;
    gpu_tex.ptr +=
      (U64)R_D3D12_TEXTURE_TABLE_BASE * (U64)ctx->srv_descriptor_size;
    ctx->command_list->SetGraphicsRootDescriptorTable(2, gpu_tex);
  }

  // Prepare current framebuffer for writing by transitioning from presenting state
  {
    CD3DX12_RESOURCE_BARRIER barrier_to_rtv = CD3DX12_RESOURCE_BARRIER::Transition(
      ctx->render_targets[ctx->frame_idx],
      D3D12_RESOURCE_STATE_PRESENT,
      D3D12_RESOURCE_STATE_RENDER_TARGET
    );
    ctx->command_list->ResourceBarrier(1, &barrier_to_rtv);
  }

  CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(
    ctx->rtv_heap->GetCPUDescriptorHandleForHeapStart(),
    ctx->frame_idx,
    ctx->rtv_descriptor_size
  );
  CD3DX12_CPU_DESCRIPTOR_HANDLE dsv_handle(ctx->dsv_heap->GetCPUDescriptorHandleForHeapStart());
  ctx->command_list->OMSetRenderTargets(1, &rtv_handle, FALSE, &dsv_handle);

  F32 clear_color[] = { 0.95f, 0.9f, 0.9f, 1.0f };
  ctx->command_list->ClearRenderTargetView(rtv_handle, clear_color, 0, 0);
  ctx->command_list->ClearDepthStencilView(dsv_handle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, 0);

  // Input Assembler setup
  ctx->command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // Draw models
  for (S32 model_idx = 0; model_idx < models_count; model_idx += 1) {
    ModelInstance *m = &models[model_idx];
    Model *model = assets_get_model(assets, m->model);

    // @Todo: rotation
    Mat4x4 tr = translation_m4x4(m->position);
    Mat4x4 sc = scale_m4x4(m->scale);
    Mat4x4 mmat = m4x4_mul(sc, tr);

    Mat4x4 inv = m4x4_inverse(mmat);
    Mat4x4 normal = m4x4_transpose(inv);

    R_DrawCB draw_cb_data = {
      .model  = mmat,
      .normal = normal,
    };
    MemoryCopy(ctx->draw_cb_mapped, &draw_cb_data, sizeof(draw_cb_data)); // @Todo: Data is zeroed/NaN in shader (NEVER EVEN BIND CBV...)

    auto vertex_buffer_view = r_d3d12_vertex_buffer_view_from_buffer(model->vertex_buffer);
    auto index_buffer_view = r_d3d12_index_buffer_view_from_buffer(model->index_buffer);
    ctx->command_list->IASetVertexBuffers(0, 1, &vertex_buffer_view);
    ctx->command_list->IASetIndexBuffer(&index_buffer_view);

    for (S32 mesh_idx = 0; mesh_idx < model->meshes_count; mesh_idx += 1) {
      Mesh *mesh = &model->meshes[mesh_idx];

      S32 index_off = mesh->ib_off;
      S32 index_count = mesh->ib_count;

      ctx->command_list->DrawIndexedInstanced(index_count, 1, index_off, 0, 0);
    }
  }

  {
    CD3DX12_RESOURCE_BARRIER barrier_to_present = CD3DX12_RESOURCE_BARRIER::Transition(
      ctx->render_targets[ctx->frame_idx],
      D3D12_RESOURCE_STATE_RENDER_TARGET,
      D3D12_RESOURCE_STATE_PRESENT
    );
    ctx->command_list->ResourceBarrier(1, &barrier_to_present);
  }
}

static void
r_shutdown(void)
{
}

// @Note: Temporary. These don't belong in here.

// ----------------------------------------------------------------------------------------------------------------

static void
camera_update_position_aspect(Camera *camera, V3F32 delta, F32 aspect, F32 delta_time)
{
  F32 near_z = 0.1f; // @Todo: Don't hardcode this. Make it member of Camera.
  F32 far_z = 100.f;

  V3F32 up = v3f32(0,1,0);
  V3F32 right = v3f32_normalize(v3f32_cross(up, camera->direction));

  F32 tightness = 12.f;
  camera->position_target = v3f32_add(camera->position_target, v3f32_scale(camera->direction, delta.z));
  camera->position_target = v3f32_add(camera->position_target, v3f32_scale(right, delta.x));
  camera->position_target = v3f32_add(camera->position_target, v3f32_scale(up, delta.y));

  V3F32 dist = v3f32_sub(camera->position_target, camera->position);
  camera->position = v3f32_add(camera->position, v3f32_scale(dist, tightness * delta_time));

  V3F32 lookat_target = v3f32_add(camera->position, camera->direction);
  camera->view = lookat_m4x4(camera->position, lookat_target, up);
  camera->proj = perspective_m4x4(camera->fov, aspect, near_z, far_z);
  camera->viewproj = m4x4_mul(camera->proj, camera->view);
}

static void
camera_update_direction(Camera *camera, F32 yaw_delta, F32 pitch_delta, F32 delta_time)
{
  camera->yaw_target += yaw_delta;
  camera->pitch_target += pitch_delta;

  F32 tightness = 12.f;
  camera->yaw   += (camera->yaw_target   - camera->yaw)   * tightness * delta_time;
  camera->pitch += (camera->pitch_target - camera->pitch) * tightness * delta_time;

  V3F32 new_direction = {
    .x = cosf32(camera->pitch)*sinf32(camera->yaw),
    .y = sinf32(camera->pitch),
    .z = cosf32(camera->pitch)*cosf32(camera->yaw),
  };

  camera->direction = v3f32_normalize(new_direction);
}

// ----------------------------------------------------------------------------------------------------------------
