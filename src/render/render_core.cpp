// @Todo: Doesn't belong in here.
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

// @Note: Temporary and stupid
// @Todo: Deprecate
#define SCENE_MODELS_COUNT 256
#define SCENE_MATERIALS_COUNT 256

static R_LinearAllocator
r_alloc_make(U64 size)
{
  R_LinearAllocator result = {};
  R_D3D12_Backend *backend = &r_ctx;

  U64 size_aligned = AlignPow2(size, 256);

  CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
  CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(size_aligned);
  ID3D12Resource *rsrc = 0;
  HRESULT hr = backend->device->CreateCommittedResource(
    &heap, D3D12_HEAP_FLAG_NONE, &desc,
    D3D12_RESOURCE_STATE_GENERIC_READ, 0,
    IID_PPV_ARGS(&rsrc)
  );
  Assert(SUCCEEDED(hr));

  hr = rsrc->Map(0, 0, (void **)&result.cpu_base);
  Assert(SUCCEEDED(hr));

  result.backend = (void *)rsrc;
  result.gpu_base = (U64)rsrc->GetGPUVirtualAddress();
  result.pos = 0;
  result.capacity = size_aligned;

  return result;
}

static void
r_alloc_release(R_LinearAllocator *alloc)
{
  ID3D12Resource *rsrc = (ID3D12Resource *)alloc->backend;
  if (rsrc) {
    rsrc->Unmap(0,0);
    rsrc->Release();
    MemoryZeroStruct(alloc);
  }
}

static void
r_alloc_reset(R_LinearAllocator *alloc)
{
  alloc->pos = 0;
}

// @Todo: basic checks and enforcing capacity
// @Todo: How do I detect overflow?
static R_Alloc
r_alloc_push(R_LinearAllocator *alloc, U64 size)
{
  U64 pos_aligned = AlignPow2(alloc->pos, 256);
  U64 size_aligned = AlignPow2(size, 256);
  Assert(pos_aligned + size_aligned <= alloc->capacity);

  U8 *cpu_curr = alloc->cpu_base + pos_aligned;
  U64 gpu_curr = alloc->gpu_base + pos_aligned;

  alloc->pos = pos_aligned + size_aligned;

  R_Alloc result = {
    .cpu = (void *)cpu_curr,
    .gpu = gpu_curr,
  };
  return result;
}

// @Todo: Move this stuff

static S32
r_get_current_base_texture_idx(void)
{
  R_D3D12_Backend *backend = &r_ctx;
  S32 current_base = backend->srv_next_idx - R_D3D12_TEXTURE_TABLE_BASE;
  return current_base;
}

static S32
r_get_current_base_material_idx(void)
{
  R_D3D12_Backend *backend = &r_ctx;
  S32 current_base = backend->mtl_next_idx;
  return current_base;
}

// @Todo: Put in render_resource.h, render_resource_d3d12.cpp (or put in render_core.h and use r_create_buffer_impl()
// if you flesh it out for structured buffs)
static void
r_upload_materials(R_MaterialGPU *materials, S32 materials_count)
{
  R_D3D12_Backend *ctx = &r_ctx;
  ID3D12Resource *upload;

  ctx->copy_cmd_allocator->Reset();
  ctx->copy_cmd_list->Reset(ctx->copy_cmd_allocator, 0);

  U64 buffer_size = sizeof(R_MaterialGPU) * materials_count;
  CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
  CD3DX12_RESOURCE_DESC buf = CD3DX12_RESOURCE_DESC::Buffer(buffer_size);

  ctx->device->CreateCommittedResource(
    &heap,
    D3D12_HEAP_FLAG_NONE,
    &buf,
    D3D12_RESOURCE_STATE_GENERIC_READ,
    0,
    IID_PPV_ARGS(&upload)
  );

  void *mapped;
  upload->Map(0, 0, &mapped);
  {
    MemoryCopy(mapped, materials, buffer_size);
  }
  upload->Unmap(0, 0);
  ctx->copy_cmd_list->CopyBufferRegion(ctx->material_buffer, 0, upload, 0, buffer_size);

  CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
    ctx->material_buffer,
    D3D12_RESOURCE_STATE_COPY_DEST,
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
  );
  ctx->copy_cmd_list->ResourceBarrier(1, &barrier);
  ctx->copy_cmd_list->Close();

  ID3D12CommandList *lists[] = { ctx->copy_cmd_list };
  ctx->command_queue->ExecuteCommandLists(1, lists);

  ctx->copy_fence_value += 1;
  ctx->command_queue->Signal(ctx->copy_fence, ctx->copy_fence_value);

  // @Todo: Release upload buffer
   //ctx->command_queue->Wait(ctx->copy_fence, ctx->copy_fence_value);
}

static void
r_d3d12_wait_for_previous_frame(void)
{
  R_D3D12_Backend *ctx = &r_ctx;

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

static void
r_init(OS_Handle window)
{
  R_D3D12_Backend *ctx = &r_ctx;
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
    ctx->dsv_descriptor_size =
      ctx->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
  }

  // Create a backbuffer texture for each frame, and render target views for them.
  // (@Todo: Should do this using texture creation and view creation API)
  for (S32 frame_idx = 0; frame_idx < R_FRAME_COUNT; frame_idx += 1) {
    ctx->swapchain->GetBuffer(frame_idx, IID_PPV_ARGS(&ctx->back_buffers[frame_idx]));

    {
      R_D3D12_Texture *tex = ArenaPushStruct(ctx->arena, R_D3D12_Texture);
      tex->resource = ctx->back_buffers[frame_idx];

      R_ResourceSlot *slot = &r_resource_table.slots[frame_idx];
      r_resource_table.count += 1;

      slot->kind = R_ResourceKind_Texture;
      slot->alive = 1;
      slot->state = R_ResourceState_Present;
      slot->backend_rsrc = (void *)tex;
    }

    {
      S32 descriptor_idx = r_alloc_texture_descriptor_idx_rtv();
      D3D12_CPU_DESCRIPTOR_HANDLE handle =
        ctx->rtv_heap->GetCPUDescriptorHandleForHeapStart();
      handle.ptr += (SIZE_T)descriptor_idx * ctx->rtv_descriptor_size;
      ctx->device->CreateRenderTargetView(ctx->back_buffers[frame_idx], 0, handle);

      R_View *view = &r_views.slots[r_views.count];
      r_views.count += 1;

      view->resource = frame_idx;
      view->kind = R_ViewKind_RenderTarget;
      view->descriptor_idx = descriptor_idx;
    }

  }

  // Create a command allocator for each frame
  for (S32 frame_idx = 0; frame_idx < R_FRAME_COUNT; frame_idx += 1) {
    hr = ctx->device->CreateCommandAllocator(
      D3D12_COMMAND_LIST_TYPE_DIRECT,
      IID_PPV_ARGS(&ctx->command_allocators[frame_idx])
    );
    Assert(SUCCEEDED(hr));
  }

  // Unified shader-visible heap: [root CBVs] + [bindless textures] + [material buffer]
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

  // Material buffer (StructuredBuffer) (t0, space1) stored in slot 3 of srv_heap
  {
    ctx->material_capacity = R_D3D12_MATERIAL_MAX;
    U64 buffer_size = sizeof(R_MaterialGPU) * ctx->material_capacity;
    CD3DX12_HEAP_PROPERTIES heap_props(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(buffer_size);
    hr = ctx->device->CreateCommittedResource(
      &heap_props,
      D3D12_HEAP_FLAG_NONE,
      &desc,
      D3D12_RESOURCE_STATE_COPY_DEST,
      0,
      IID_PPV_ARGS(&ctx->material_buffer)
    );
    Assert(SUCCEEDED(hr));

    D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
    srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srv.Format = DXGI_FORMAT_UNKNOWN;
    srv.Buffer.FirstElement = 0;
    srv.Buffer.NumElements = ctx->material_capacity;
    srv.Buffer.StructureByteStride = sizeof(R_MaterialGPU);
    srv.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    ctx->material_srv_idx = R_D3D12_MATERIAL_BUFFER_BASE;

    CD3DX12_CPU_DESCRIPTOR_HANDLE h(
      ctx->srv_heap->GetCPUDescriptorHandleForHeapStart(),
      ctx->material_srv_idx,
      ctx->srv_descriptor_size
    );
    ctx->device->CreateShaderResourceView(ctx->material_buffer, &srv, h);
  }

  // Root signature
  {
    CD3DX12_DESCRIPTOR_RANGE ranges[2];
    // t0[] space0: texture table
    ranges[0].Init(
      D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
      R_D3D12_TEXTURE_MAX,
      0, // baseShaderRegister t0
      0  // registerSpace 0
    );

    // t0 space1: material buffer
    ranges[1].Init(
      D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
      1,
      0, // baseShaderRegister t0
      1  // registerSpace 1
    );

    CD3DX12_ROOT_PARAMETER params[4];
    // b0: frame/pass constants (root CBV)
    params[0].InitAsConstantBufferView(
      0, // shaderRegister b0
      0, // registerSpace
      D3D12_SHADER_VISIBILITY_ALL
    );
    // b1: per-draw constants (root CBV)
    params[1].InitAsConstantBufferView(
      1, // shaderRegister b1
      0, // registerSpace
      D3D12_SHADER_VISIBILITY_ALL
    );
    // SRV descriptor table for textures
    params[2].InitAsDescriptorTable(
      1, &ranges[0],
      D3D12_SHADER_VISIBILITY_PIXEL
    );
    // SRV descriptor table for materials
    params[3].InitAsDescriptorTable(
      1, &ranges[1],
      D3D12_SHADER_VISIBILITY_PIXEL
    );

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

  // Main command list
  hr = ctx->device->CreateCommandList(
    0, D3D12_COMMAND_LIST_TYPE_DIRECT,
    ctx->command_allocators[ctx->frame_idx], 0,
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
