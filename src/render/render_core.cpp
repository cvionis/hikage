#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

// @Note: Temporary and stupid
#define SCENE_MODELS_COUNT 256
#define SCENE_MATERIALS_COUNT 256

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

static void
r_init(OS_Handle window)
{
  R_Context *ctx = &r_ctx;
  HWND hwnd = os_win32_window_from_handle(window)->hwnd;

  // @Todo: Temp
  ctx->width = 1280;
  ctx->height = 720;

  HRESULT hr;
  UINT dxgi_factory_flags = 0;

#if BUILD_DEBUG
  ID3D12Debug *debug_controller = 0;
  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)))) {
    debug_controller->EnableDebugLayer();
    dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
    debug_controller->Release();
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
  D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {};
  rtv_heap_desc.NumDescriptors = R_D3D12_FRAME_COUNT + 1; // +1 for MSAA RT
  rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  hr = ctx->device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&ctx->rtv_heap));
  Assert(SUCCEEDED(hr));
  ctx->rtv_descriptor_size = ctx->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  // DSV heap
  D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc = {};
  dsv_heap_desc.NumDescriptors = 1;
  dsv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
  hr = ctx->device->CreateDescriptorHeap(&dsv_heap_desc, IID_PPV_ARGS(&ctx->dsv_heap));
  Assert(SUCCEEDED(hr));

  // Uniform descriptor heap: camera + models + materials
  ctx->cbv_count =  1 + SCENE_MODELS_COUNT + SCENE_MATERIALS_COUNT;
  D3D12_DESCRIPTOR_HEAP_DESC cbv_heap_desc = {};
  cbv_heap_desc.NumDescriptors = ctx->cbv_count;
  cbv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  cbv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // @Todo: Should probably specify in name whether or not heap is GPU visible.
  hr = ctx->device->CreateDescriptorHeap(&cbv_heap_desc, IID_PPV_ARGS(&ctx->cbv_heap));
  Assert(SUCCEEDED(hr));
  ctx->cbv_descriptor_size = ctx->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  // Back buffers and command allocators
  CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(ctx->rtv_heap->GetCPUDescriptorHandleForHeapStart());
  for (UINT n = 0; n < R_D3D12_FRAME_COUNT; n += 1) {
    hr = ctx->swapchain->GetBuffer(n, IID_PPV_ARGS(&ctx->render_targets[n]));
    ctx->device->CreateRenderTargetView(ctx->render_targets[n], 0, rtv_handle);
    rtv_handle.Offset(1, ctx->rtv_descriptor_size);
    hr = ctx->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&ctx->command_allocators[n]));
  }

  // Color buffer (no MSAA)
  {
    D3D12_RESOURCE_DESC color_desc = {};
    color_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    color_desc.Alignment = 0;
    color_desc.Width = ctx->width;
    color_desc.Height = ctx->height;
    color_desc.DepthOrArraySize = 1;
    color_desc.MipLevels = 1;
    color_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    // Non-MSAA
    color_desc.SampleDesc.Count = 1;
    color_desc.SampleDesc.Quality = 0;

    color_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    color_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_CLEAR_VALUE clear_value = {};
    clear_value.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    clear_value.Color[0] = 0.0f;
    clear_value.Color[1] = 0.2f;
    clear_value.Color[2] = 0.4f;
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
    ctx->rtv_handle = rtv;
  }


  // Depth buffer
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
                                            IID_PPV_ARGS(&ctx->depth_buffer));
  Assert(SUCCEEDED(hr));
  ctx->device->CreateDepthStencilView(ctx->depth_buffer, 0,
                                      ctx->dsv_heap->GetCPUDescriptorHandleForHeapStart());

  // Root signature with three descriptor tables
  // table0: b0 (CameraCB)
  // table1: b1 (ModelCB)
  // table2: b2 (MaterialCB)
  CD3DX12_DESCRIPTOR_RANGE ranges[5];
  ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0);
  ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1, 0);
  ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 2, 0);

  CD3DX12_ROOT_PARAMETER params[5];
  params[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL);
  params[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_VERTEX);
  params[2].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_PIXEL);

  D3D12_STATIC_SAMPLER_DESC static_sampler = {};
  static_sampler.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
  static_sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  static_sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  static_sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  static_sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
  static_sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
  static_sampler.ShaderRegister = 0; // s0
  static_sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

  CD3DX12_ROOT_SIGNATURE_DESC root_sig_desc;
  root_sig_desc.Init(ArrayCount(params), params, 1, &static_sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

  ID3DBlob *sig_blob = 0;
  ID3DBlob *err_blob = 0;
  D3D12SerializeRootSignature(&root_sig_desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig_blob, &err_blob);
  ctx->device->CreateRootSignature(0, sig_blob->GetBufferPointer(), sig_blob->GetBufferSize(),
                                   IID_PPV_ARGS(&ctx->root_signature));
  sig_blob->Release();
  if (err_blob) {
    err_blob->Release();
  }

  // Compile shaders

  ID3DBlob *vs_blob = 0;
  ID3DBlob *ps_blob = 0;

#if BUILD_DEBUG
  UINT compile_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
  UINT compile_flags = 0;
#endif

  D3DCompileFromFile(L"render/shaders/main.hlsl", 0, 0, "vs_main", "vs_5_0", compile_flags, 0, &vs_blob, 0);
  D3DCompileFromFile(L"render/shaders/main.hlsl", 0, 0, "ps_main", "ps_5_0", compile_flags, 0, &ps_blob, 0);

  // Pipeline state object
  // @Todo: Tangent, UV; remove color.
  U32 input_count = 3;
  ctx->input_desc[0] = {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};
  ctx->input_desc[1] = {"COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};
  ctx->input_desc[2] = {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};

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

  ctx->device->CreateGraphicsPipelineState(pso_desc, IID_PPV_ARGS(&ctx->pipeline_state));
  vs_blob->Release();
  ps_blob->Release();

  ctx->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                 ctx->command_allocators[ctx->frame_idx], ctx->pipeline_state,
                                 IID_PPV_ARGS(&ctx->command_list));
  ctx->command_list->Close();

  // Cube geometry
  struct Vertex { V3F32 pos; V4F32 color; V3F32 normal; };

  Vertex cube_vertices[] = {
    // -Z face
    {{-0.5f,-0.5f,-0.5f},{1,0,0,1},{ 0, 0,-1}},
    {{-0.5f, 0.5f,-0.5f},{0,1,0,1},{ 0, 0,-1}},
    {{ 0.5f, 0.5f,-0.5f},{0,0,1,1},{ 0, 0,-1}},
    {{ 0.5f,-0.5f,-0.5f},{1,1,0,1},{ 0, 0,-1}},

    // +Z
    {{-0.5f,-0.5f, 0.5f},{1,0,1,1},{ 0, 0, 1}},
    {{-0.5f, 0.5f, 0.5f},{0,1,1,1},{ 0, 0, 1}},
    {{ 0.5f, 0.5f, 0.5f},{1,1,1,1},{ 0, 0, 1}},
    {{ 0.5f,-0.5f, 0.5f},{0.2f,0.2f,0.2f},{ 0, 0, 1}},

    // -X
    {{-0.5f,-0.5f,-0.5f},{1,0,0,1},{-1, 0, 0}},
    {{-0.5f,-0.5f, 0.5f},{1,0,1,1},{-1, 0, 0}},
    {{-0.5f, 0.5f, 0.5f},{0,1,1,1},{-1, 0, 0}},
    {{-0.5f, 0.5f,-0.5f},{0,1,0,1},{-1, 0, 0}},

    // +X
    {{ 0.5f,-0.5f,-0.5f},{1,1,0,1},{ 1, 0, 0}},
    {{ 0.5f,-0.5f, 0.5f},{0.2f,0.2f,0.2f},{ 1, 0, 0}},
    {{ 0.5f, 0.5f, 0.5f},{1,1,1,1},{ 1, 0, 0}},
    {{ 0.5f, 0.5f,-0.5f},{0,0,1,1},{ 1, 0, 0}},

    // -Y
    {{-0.5f,-0.5f,-0.5f},{1,0,0,1},{ 0,-1, 0}},
    {{ 0.5f,-0.5f,-0.5f},{1,1,0,1},{ 0,-1, 0}},
    {{ 0.5f,-0.5f, 0.5f},{0.2f,0.2f,0.2f},{ 0,-1, 0}},
    {{-0.5f,-0.5f, 0.5f},{1,0,1,1},{ 0,-1, 0}},

    // +Y
    {{-0.5f, 0.5f,-0.5f},{0,1,0,1},{ 0, 1, 0}},
    {{ 0.5f, 0.5f,-0.5f},{0,0,1,1},{ 0, 1, 0}},
    {{ 0.5f, 0.5f, 0.5f},{1,1,1,1},{ 0, 1, 0}},
    {{-0.5f, 0.5f, 0.5f},{0,1,1,1},{ 0, 1, 0}},
  };

  U16 cube_indices[] = {
    0,1,2,0,2,3,
    4,6,5,4,7,6,
    8,9,10,8,10,11,
    12,14,13,12,15,14,
    16,17,18,16,18,19,
    20,22,21,20,23,22
  };

  UINT vb_size = sizeof(cube_vertices);
  UINT ib_size = sizeof(cube_indices);

  // Vertex buffer
  CD3DX12_HEAP_PROPERTIES vb_heap(D3D12_HEAP_TYPE_UPLOAD);
  CD3DX12_RESOURCE_DESC vb_desc = CD3DX12_RESOURCE_DESC::Buffer(vb_size);
  ctx->device->CreateCommittedResource(&vb_heap, D3D12_HEAP_FLAG_NONE, &vb_desc,
                                       D3D12_RESOURCE_STATE_GENERIC_READ, 0,
                                       IID_PPV_ARGS(&ctx->vertex_buffer));
  void *vtx_data;
  ctx->vertex_buffer->Map(0, 0, &vtx_data);
  MemoryCopy(vtx_data, cube_vertices, vb_size);
  ctx->vertex_buffer->Unmap(0, 0);
  ctx->vertex_buffer_view.BufferLocation = ctx->vertex_buffer->GetGPUVirtualAddress();
  ctx->vertex_buffer_view.SizeInBytes   = vb_size;
  ctx->vertex_buffer_view.StrideInBytes = sizeof(Vertex);

  // Index buffer
  CD3DX12_HEAP_PROPERTIES ib_heap(D3D12_HEAP_TYPE_UPLOAD);
  CD3DX12_RESOURCE_DESC ib_desc = CD3DX12_RESOURCE_DESC::Buffer(ib_size);
  ctx->device->CreateCommittedResource(&ib_heap, D3D12_HEAP_FLAG_NONE, &ib_desc,
                                       D3D12_RESOURCE_STATE_GENERIC_READ, 0,
                                       IID_PPV_ARGS(&ctx->index_buffer));
  void *idx_data;
  ctx->index_buffer->Map(0, 0, &idx_data);
  MemoryCopy(idx_data, cube_indices, ib_size);
  ctx->index_buffer->Unmap(0, 0);
  ctx->index_buffer_view.BufferLocation = ctx->index_buffer->GetGPUVirtualAddress();
  ctx->index_buffer_view.Format = DXGI_FORMAT_R16_UINT;
  ctx->index_buffer_view.SizeInBytes = ib_size;

  // Constant buffers: separate resources
  ctx->model_cb_stride = 256;
  ctx->material_cb_stride = 256;

  // Uniform data backing buffer: camera
  {
    CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(256);
    ctx->device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                                         D3D12_RESOURCE_STATE_GENERIC_READ, 0,
                                         IID_PPV_ARGS(&ctx->camera_cb));
    ctx->camera_cb->Map(0, 0, (void**)(&ctx->camera_cb_mapped));
  }
  // Uniform data backing buffer: model transforms
  {
    U64 bytes = (U64)ctx->model_cb_stride * (U64)SCENE_MODELS_COUNT;
    CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(bytes);
    ctx->device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                                         D3D12_RESOURCE_STATE_GENERIC_READ, 0,
                                         IID_PPV_ARGS(&ctx->model_cb));
    ctx->model_cb->Map(0, 0, (void**)(&ctx->model_cb_mapped));
  }
  // Uniform data backing buffer: materials
  {
    U64 bytes = (U64)ctx->material_cb_stride * (U64)SCENE_MATERIALS_COUNT;
    CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(bytes);
    ctx->device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                                         D3D12_RESOURCE_STATE_GENERIC_READ, 0,
                                         IID_PPV_ARGS(&ctx->material_cb));
    ctx->material_cb->Map(0, 0, (void**)(&ctx->material_cb_mapped));
  }

  // Allocate uniform descriptors from a single heap: [0]=camera, [1..N]=models, [base..]=materials
  {
    ctx->model_cbv_base = 1;
    ctx->material_cbv_base = 1 + SCENE_MODELS_COUNT;

    CD3DX12_CPU_DESCRIPTOR_HANDLE h(ctx->cbv_heap->GetCPUDescriptorHandleForHeapStart());

    // Camera
    {
      D3D12_CONSTANT_BUFFER_VIEW_DESC d = {};
      d.BufferLocation = ctx->camera_cb->GetGPUVirtualAddress();
      d.SizeInBytes = 256;
      ctx->device->CreateConstantBufferView(&d, h);
      h.Offset(1, ctx->cbv_descriptor_size);
    }
    // Model transform slices
    for (S32 idx = 0; idx < SCENE_MODELS_COUNT; idx += 1) {
      D3D12_CONSTANT_BUFFER_VIEW_DESC d = {};
      d.BufferLocation = ctx->model_cb->GetGPUVirtualAddress() + (U64)ctx->model_cb_stride * (U64)idx;
      d.SizeInBytes = ctx->model_cb_stride;
      ctx->device->CreateConstantBufferView(&d, h);
      h.Offset(1, ctx->cbv_descriptor_size);
    }
    // Material slices
    for (S32 idx = 0; idx < SCENE_MATERIALS_COUNT; idx += 1) {
      D3D12_CONSTANT_BUFFER_VIEW_DESC d = {};
      d.BufferLocation = ctx->material_cb->GetGPUVirtualAddress() + (U64)ctx->material_cb_stride * (U64)idx;
      d.SizeInBytes = ctx->material_cb_stride;
      ctx->device->CreateConstantBufferView(&d, h);
      h.Offset(1, ctx->cbv_descriptor_size);
    }
  }

  // Create synchronization primitives
  ctx->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&ctx->fence));
  ctx->fence_event = CreateEvent(0, FALSE, FALSE, 0);
  for (S32 idx = 0; idx < R_D3D12_FRAME_COUNT; idx += 1) ctx->fence_values[idx] = 0;
  ctx->fence_values[ctx->frame_idx] = 1;
  factory->Release();
}

static void
r_d3d12_wait_for_previous_frame(void)
{
  R_Context *ctx = &r_ctx;

  // Signal GPU to mark current work complete using this fence value.
  U64 fence_to_signal = ctx->fence_values[ctx->frame_idx];
  HRESULT hr = ctx->command_queue->Signal(ctx->fence, fence_to_signal);
  Assert(SUCCEEDED(hr));

  // Advance to the next back buffer index.
  ctx->frame_idx = ctx->swapchain->GetCurrentBackBufferIndex();

  // If the GPU hasnâ€™t finished processing this frame yet, wait for the fence event.
  if (ctx->fence->GetCompletedValue() < ctx->fence_values[ctx->frame_idx]) {
    hr = ctx->fence->SetEventOnCompletion(ctx->fence_values[ctx->frame_idx], ctx->fence_event);
    Assert(SUCCEEDED(hr));
    WaitForSingleObjectEx(ctx->fence_event, INFINITE, FALSE);
  }

  // Prepare fence value for the next frame.
  ctx->fence_values[ctx->frame_idx] = fence_to_signal + 1;
}
