//
// D3D12 Resources
//


// @Todo: the "state" members for these (where applicable) are more like "initial state";
// kind of deceiving. Should get rid of this as I store current state in resource slot.

struct R_D3D12_Pipeline {
  // Runtime
  ID3D12PipelineState *pso;
  ID3D12RootSignature *root_sig;

  // Shader info
  // @Todo: String8, convert
  LPCWSTR vs_path;
  LPCWSTR ps_path;

  // Input layout
  D3D12_INPUT_ELEMENT_DESC input_layout[16];
  S32 input_layout_count;

  // Fixed state
  D3D12_RASTERIZER_DESC raster;
  D3D12_DEPTH_STENCIL_DESC depth_stencil;
  D3D12_BLEND_DESC blend;
  D3D12_PRIMITIVE_TOPOLOGY_TYPE topology_type;

  // Attachment formats
  DXGI_FORMAT rtv_formats[8];
  S32 rtv_count;
  DXGI_FORMAT dsv_format;
  DXGI_SAMPLE_DESC sample_desc;

  // Cached desc for rebuild
  D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc;
};

struct R_D3D12_Texture {
  // @Todo: Cache format
  ID3D12Resource *resource;
  D3D12_RESOURCE_STATES state;
};

struct R_D3D12_Buffer {
  ID3D12Resource *resource;
  D3D12_RESOURCE_STATES state;
  union {
    D3D12_VERTEX_BUFFER_VIEW vbv;
    D3D12_INDEX_BUFFER_VIEW  ibv;
  };
  S64 size;
};

static D3D12_RESOURCE_STATES
r_d3d12_state_from_r_state(R_ResourceState state)
{
  D3D12_RESOURCE_STATES result = D3D12_RESOURCE_STATE_COMMON;

  switch(state) {
    case R_ResourceState_Invalid:         { result = D3D12_RESOURCE_STATE_COMMON; } break;
    case R_ResourceState_Common:          { result = D3D12_RESOURCE_STATE_COMMON; } break;
    case R_ResourceState_RenderTarget:    { result = D3D12_RESOURCE_STATE_RENDER_TARGET; } break;
    case R_ResourceState_DepthWrite:      { result = D3D12_RESOURCE_STATE_DEPTH_WRITE; } break;
    case R_ResourceState_DepthRead:       { result = D3D12_RESOURCE_STATE_DEPTH_READ; } break;
    case R_ResourceState_ShaderRead:{
      result = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    } break;
    case R_ResourceState_ShaderReadWrite: { result = D3D12_RESOURCE_STATE_UNORDERED_ACCESS; } break;
    case R_ResourceState_CopySrc:         { result = D3D12_RESOURCE_STATE_COPY_SOURCE; } break;
    case R_ResourceState_CopyDst:         { result = D3D12_RESOURCE_STATE_COPY_DEST; } break;
    case R_ResourceState_Present:         { result = D3D12_RESOURCE_STATE_PRESENT; } break;
  }

  return result;
}

static R_ResourceState
r_state_from_d3d12_state(D3D12_RESOURCE_STATES state)
{
  R_ResourceState result = R_ResourceState_Invalid;

  if (state & D3D12_RESOURCE_STATE_PRESENT) {
    result = R_ResourceState_Present;
  }
  else if (state & D3D12_RESOURCE_STATE_RENDER_TARGET) {
    result = R_ResourceState_RenderTarget;
  }
  else if (state & D3D12_RESOURCE_STATE_DEPTH_WRITE) {
    result = R_ResourceState_DepthWrite;
  }
  else if (state & D3D12_RESOURCE_STATE_DEPTH_READ) {
    result = R_ResourceState_DepthRead;
  }
  else if (state & D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
    result = R_ResourceState_ShaderReadWrite;
  }
  else if (
    state & (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
      D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)) {
    result = R_ResourceState_ShaderRead;
  }
  else if (state & D3D12_RESOURCE_STATE_COPY_SOURCE) {
    result = R_ResourceState_CopySrc;
  }
  else if (state & D3D12_RESOURCE_STATE_COPY_DEST) {
    result = R_ResourceState_CopyDst;
  }
  else if (state & D3D12_RESOURCE_STATE_COMMON) {
    result = R_ResourceState_Common;
  }

  return result;
}

static ID3D12Resource *
r_d3d12_rsrc(R_Handle handle)
{
  ID3D12Resource *result = 0;

  R_ResourceSlot *slot = &r_resource_table.slots[handle.idx];
  switch (slot->kind) {
    case R_ResourceKind_Texture: {
      R_D3D12_Texture *rsrc = (R_D3D12_Texture *)slot->backend_rsrc;
      result = rsrc->resource;
    }break;
    case R_ResourceKind_Buffer: {
      R_D3D12_Buffer *rsrc = (R_D3D12_Buffer *)slot->backend_rsrc;
      result = rsrc->resource;
    }break;
  }

  return result;
}

static R_ResourceState
r_resource_state(R_Handle handle)
{
  R_ResourceState result = R_ResourceState_Invalid;
  R_ResourceSlot *slot = &r_resource_table.slots[handle.idx];
  result = slot->state;
  return result;
}

//
// Textures
//

static S32
r_alloc_texture_descriptor_idx_srv(void)
{
  R_D3D12_Backend *ctx = &r_ctx;

  // @Todo: Free list
  S32 idx = ctx->srv_next_idx;
  ctx->srv_next_idx += 1;

  return idx;
}

static S32
r_alloc_texture_descriptor_idx_rtv(void)
{
  R_D3D12_Backend *ctx = &r_ctx;

  // @Todo: Free list
  S32 idx = ctx->rtv_next_idx;
  ctx->rtv_next_idx += 1;

  return idx;
}

static S32
r_alloc_texture_descriptor_idx_dsv(void)
{
  R_D3D12_Backend *ctx = &r_ctx;

  // @Todo: Free list
  S32 idx = ctx->dsv_next_idx;
  ctx->dsv_next_idx += 1;

  return idx;
}

static D3D12_CPU_DESCRIPTOR_HANDLE
r_d3d12_srv_from_view(R_Handle handle)
{
  R_D3D12_Backend *backend = &r_ctx;

  R_View *view = &r_views.slots[handle.idx];
  S32 srv_idx = view->descriptor_idx;

  D3D12_CPU_DESCRIPTOR_HANDLE result = backend->srv_heap->GetCPUDescriptorHandleForHeapStart();
  result.ptr += (SIZE_T)srv_idx * (SIZE_T)backend->srv_descriptor_size;
  return result;
}

static D3D12_CPU_DESCRIPTOR_HANDLE
r_d3d12_rtv_from_view(R_Handle handle)
{
  R_D3D12_Backend *backend = &r_ctx;

  R_View *view = &r_views.slots[handle.idx];
  S32 rtv_idx = view->descriptor_idx;

  D3D12_CPU_DESCRIPTOR_HANDLE result = backend->rtv_heap->GetCPUDescriptorHandleForHeapStart();
  result.ptr += (SIZE_T)rtv_idx * (SIZE_T)backend->rtv_descriptor_size;
  return result;
}

static D3D12_CPU_DESCRIPTOR_HANDLE
r_d3d12_dsv_from_view(R_Handle handle)
{
  R_D3D12_Backend *backend = &r_ctx;

  R_View *view = &r_views.slots[handle.idx];
  S32 dsv_idx = view->descriptor_idx;

  D3D12_CPU_DESCRIPTOR_HANDLE result = backend->dsv_heap->GetCPUDescriptorHandleForHeapStart();
  result.ptr += (SIZE_T)dsv_idx * (SIZE_T)backend->dsv_descriptor_size;
  return result;
}

static DXGI_FORMAT
r_d3d12_fmt_from_r_fmt(R_Format fmt)
{
  DXGI_FORMAT result = DXGI_FORMAT_UNKNOWN;

  switch (fmt) {
    case R_Format_Invalid:                { result = DXGI_FORMAT_UNKNOWN; } break;
    case R_Format_R8_UNorm:               { result = DXGI_FORMAT_R8_UNORM; } break;
    case R_Format_R8G8_UNorm:             { result = DXGI_FORMAT_R8G8_UNORM; } break;
    case R_Format_R8G8B8A8_UNorm:         { result = DXGI_FORMAT_R8G8B8A8_UNORM; } break;
    case R_Format_R8G8B8A8_UNorm_Srgb:    { result = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; } break;
    case R_Format_R16_Float:              { result = DXGI_FORMAT_R16_FLOAT; } break;
    case R_Format_R16G16_Float:           { result = DXGI_FORMAT_R16G16_FLOAT; } break;
    case R_Format_R16G16B16A16_Float:     { result = DXGI_FORMAT_R16G16B16A16_FLOAT; } break;
    case R_Format_R32_Float:              { result = DXGI_FORMAT_R32_FLOAT; } break;
    case R_Format_R32G32_Float:           { result = DXGI_FORMAT_R32G32_FLOAT; } break;
    case R_Format_R32G32B32_Float:        { result = DXGI_FORMAT_R32G32B32_FLOAT; } break;
    case R_Format_R32G32B32A32_Float:     { result = DXGI_FORMAT_R32G32B32A32_FLOAT; } break;
    case R_Format_R11G11B10_Float:        { result = DXGI_FORMAT_R11G11B10_FLOAT; } break;
    case R_Format_R10G10B10A2_UNorm:      { result = DXGI_FORMAT_R10G10B10A2_UNORM; } break;
    case R_Format_BC1_UNorm:              { result = DXGI_FORMAT_BC1_UNORM; } break;
    case R_Format_BC1_UNorm_Srgb:         { result = DXGI_FORMAT_BC1_UNORM_SRGB; } break;
    case R_Format_BC3_UNorm:              { result = DXGI_FORMAT_BC3_UNORM; } break;
    case R_Format_BC3_UNorm_Srgb:         { result = DXGI_FORMAT_BC3_UNORM_SRGB; } break;
    case R_Format_BC4_UNorm:              { result = DXGI_FORMAT_BC4_UNORM; } break;
    case R_Format_BC5_UNorm:              { result = DXGI_FORMAT_BC5_UNORM; } break;
    case R_Format_BC7_UNorm:              { result = DXGI_FORMAT_BC7_UNORM; } break;
    case R_Format_BC7_UNorm_Srgb:         { result = DXGI_FORMAT_BC7_UNORM_SRGB; } break;
    case R_Format_D32_Float:              { result = DXGI_FORMAT_D32_FLOAT; } break;
    case R_Format_D24_UNorm_S8_UInt:      { result = DXGI_FORMAT_D24_UNORM_S8_UINT; } break;
  }

  return result;
}

static void
r_d3d12_write_srv(ID3D12Resource *resource, DXGI_FORMAT fmt, R_SubresourceRange range, S32 descriptor_idx)
{
  R_D3D12_Backend *ctx = &r_ctx;

  D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
  srv_desc.Format = fmt;
  srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

  if (range.slice_count > 1) {
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    srv_desc.Texture2DArray.MostDetailedMip     = range.mip_start;
    srv_desc.Texture2DArray.MipLevels           = range.mip_count;
    srv_desc.Texture2DArray.FirstArraySlice     = range.slice_start;
    srv_desc.Texture2DArray.ArraySize           = range.slice_count;
    srv_desc.Texture2DArray.PlaneSlice          = 0;
    srv_desc.Texture2DArray.ResourceMinLODClamp = 0.0f;
  }
  else {
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MostDetailedMip     = range.mip_start;
    srv_desc.Texture2D.MipLevels           = range.mip_count;
    srv_desc.Texture2D.PlaneSlice          = 0;
    srv_desc.Texture2D.ResourceMinLODClamp = 0.0f;
  }

  D3D12_CPU_DESCRIPTOR_HANDLE handle =
    ctx->srv_heap->GetCPUDescriptorHandleForHeapStart();
  handle.ptr += (SIZE_T)descriptor_idx * ctx->srv_descriptor_size;

  ctx->device->CreateShaderResourceView(resource, &srv_desc, handle);
}

static void
r_d3d12_write_rtv(ID3D12Resource *resource, DXGI_FORMAT fmt, R_SubresourceRange range, S32 descriptor_idx)
{
  R_D3D12_Backend *ctx = &r_ctx;

  D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
  rtv_desc.Format = fmt;

  if (range.slice_count > 1) {
    rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
    rtv_desc.Texture2DArray.MipSlice        = range.mip_start;
    rtv_desc.Texture2DArray.FirstArraySlice = range.slice_start;
    rtv_desc.Texture2DArray.ArraySize       = range.slice_count;
    rtv_desc.Texture2DArray.PlaneSlice      = 0;
  }
  else {
    rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtv_desc.Texture2D.MipSlice   = range.mip_start;
    rtv_desc.Texture2D.PlaneSlice= 0;
  }

  D3D12_CPU_DESCRIPTOR_HANDLE handle =
    ctx->rtv_heap->GetCPUDescriptorHandleForHeapStart();
  handle.ptr += (SIZE_T)descriptor_idx * ctx->rtv_descriptor_size;

  ctx->device->CreateRenderTargetView(resource, &rtv_desc, handle);
}

static void
r_d3d12_write_dsv(ID3D12Resource *resource, DXGI_FORMAT fmt, R_SubresourceRange range, S32 descriptor_idx)
{
  R_D3D12_Backend *ctx = &r_ctx;

  D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
  dsv_desc.Format = fmt;
  dsv_desc.Flags  = D3D12_DSV_FLAG_NONE;

  if (range.slice_count > 1) {
    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
    dsv_desc.Texture2DArray.MipSlice        = range.mip_start;
    dsv_desc.Texture2DArray.FirstArraySlice = range.slice_start;
    dsv_desc.Texture2DArray.ArraySize       = range.slice_count;
  }
  else {
    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsv_desc.Texture2D.MipSlice = range.mip_start;
  }

  D3D12_CPU_DESCRIPTOR_HANDLE handle =
    ctx->dsv_heap->GetCPUDescriptorHandleForHeapStart();
  handle.ptr += (SIZE_T)descriptor_idx * ctx->dsv_descriptor_size;

  ctx->device->CreateDepthStencilView(resource, &dsv_desc, handle);
}

static U64
r_d3d12_calc_upload_size(ID3D12Resource *dst, S32 subresource_count, D3D12_PLACED_SUBRESOURCE_FOOTPRINT *out_layouts, U64 *out_total_size)
{
  R_D3D12_Backend *ctx = &r_ctx;

  D3D12_RESOURCE_DESC desc = dst->GetDesc();
  ctx->device->GetCopyableFootprints(
    &desc,
    0,
    subresource_count,
    0,
    out_layouts,
    0,
    0,
    out_total_size
  );
  return *out_total_size;
}

static  B32
is_block_compressed(DXGI_FORMAT fmt)
{
  B32 result = 0;
  switch (fmt) {
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB: { result = 1; }break;
  }
  return result;
}

static S32
bc_bytes_per_block(DXGI_FORMAT fmt)
{
  S32 result = 16; // BC2/3/5/6/7
  switch (fmt) {
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM: { result = 8; }break;
  }
  return result;
}

static void
r_d3d12_upload_texture(R_D3D12_Texture *tex, DXGI_FORMAT fmt, R_TextureInitData *init, S32 init_count)
{
  R_D3D12_Backend *ctx = &r_ctx;

  D3D12_PLACED_SUBRESOURCE_FOOTPRINT layouts[16];
  U64 upload_size = 0;

  r_d3d12_calc_upload_size(
    tex->resource,
    init_count,
    layouts,
    &upload_size
  );

  ID3D12Resource *upload = 0;
  {
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = upload_size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    HRESULT hr = ctx->device->CreateCommittedResource(
      &heap,
      D3D12_HEAP_FLAG_NONE,
      &desc,
      D3D12_RESOURCE_STATE_GENERIC_READ,
      0,
      IID_PPV_ARGS(&upload)
    );
    Assert(SUCCEEDED(hr));
  }

  void *mapped = 0;
  upload->Map(0, 0, &mapped);
  {
    for (S32 i = 0; i < init_count; i += 1) {
      U8 *dst_base = (U8 *)mapped + layouts[i].Offset;
      U8 *src_base = (U8 *)init[i].data;

      U32 dst_row_pitch = layouts[i].Footprint.RowPitch;

      if (is_block_compressed(fmt)) {
        U32 bpb = bc_bytes_per_block(fmt);
        U32 dst_row_pitch = layouts[i].Footprint.RowPitch;

        U32 src_row_bytes = (U32)init[i].row_pitch;
        U32 rows          = (U32)(init[i].slice_pitch / init[i].row_pitch);
        Assert(src_row_bytes <= dst_row_pitch);

        for (U32 y = 0; y < rows; y += 1) {
          MemoryCopy(
            dst_base + (U64)y * dst_row_pitch,
            src_base + (U64)y * src_row_bytes,
            src_row_bytes
          );
        }
      }
      else {
        // @Note: Untested
        U32 rows = layouts[i].Footprint.Height;
        U32 src_row_bytes = (U32)init[i].row_pitch;

        Assert(src_row_bytes <= dst_row_pitch);

        for (U32 y = 0; y < rows; y += 1) {
          MemoryCopy(
            dst_base + (U64)y * dst_row_pitch,
            src_base + (U64)y * src_row_bytes,
            src_row_bytes
          );
        }
      }
    }
  }
  upload->Unmap(0, 0);

  // @Todo: Need to call upload->Release() when fence value is reached. You're leaking memory if you don't release it.

  ctx->copy_cmd_allocator->Reset();
  ctx->copy_cmd_list->Reset(ctx->copy_cmd_allocator, 0);

  for (S32 i = 0; i < init_count; i += 1) {
    D3D12_TEXTURE_COPY_LOCATION dst = {};
    dst.pResource = tex->resource;
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = i;

    D3D12_TEXTURE_COPY_LOCATION src = {};
    src.pResource = upload;
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint = layouts[i];

    ctx->copy_cmd_list->CopyTextureRegion(&dst, 0, 0, 0, &src, 0);
  }

  {
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = tex->resource;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    ctx->copy_cmd_list->ResourceBarrier(1, &barrier);
    tex->state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
  }

  ctx->copy_cmd_list->Close();

  ID3D12CommandList *lists[] = { ctx->copy_cmd_list };
  ctx->command_queue->ExecuteCommandLists(1, lists);

  ctx->copy_fence_value += 1;
  ctx->command_queue->Signal(
    ctx->copy_fence,
    ctx->copy_fence_value
  );
}

// @Todo: Test; error-checking and input validation.
// @Todo: Limited to 2D textures right now.
static R_CreateResource
r_create_texture_impl(R_TextureInitData *init, S32 init_count, R_TextureDesc desc)
{
  R_D3D12_Backend *ctx = &r_ctx;
  R_CreateResource result = {};

  DXGI_FORMAT dxgi_fmt = r_d3d12_fmt_from_r_fmt(desc.fmt);
  D3D12_RESOURCE_DESC rdesc = {};
  rdesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  rdesc.Width = desc.width;
  rdesc.Height = desc.height;
  rdesc.DepthOrArraySize = 1;
  rdesc.MipLevels = (U16)desc.mips_count;
  rdesc.Format = dxgi_fmt;
  rdesc.SampleDesc.Count = 1;
  rdesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

  D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
  if (desc.usage & R_TextureUsage_RenderTarget) {
    flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
  }
  if (desc.usage & R_TextureUsage_DepthStencil) {
    flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
  }
  if (desc.usage & R_TextureUsage_Unordered) {
    flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  }
  rdesc.Flags = flags;

  D3D12_CLEAR_VALUE clear_value = {};
  D3D12_CLEAR_VALUE *clear_ptr = 0;
  if (desc.has_clear_value) {
    clear_value.Format = dxgi_fmt;
    if (desc.usage & R_TextureUsage_RenderTarget) {
      clear_value.Color[0] = desc.clear_color.r;
      clear_value.Color[1] = desc.clear_color.g;
      clear_value.Color[2] = desc.clear_color.b;
      clear_value.Color[3] = desc.clear_color.a;
      clear_ptr = &clear_value;
    }
    else if (desc.usage & R_TextureUsage_DepthStencil) {
      clear_value.DepthStencil.Depth = desc.clear_ds.depth;
      clear_value.DepthStencil.Stencil = desc.clear_ds.stencil;
      clear_ptr = &clear_value;
    }
  }

  // @Todo: This is more like "initial state". Just stop storing this in the backend resource struct; I store it and update it
  // in resource slot now.
  R_D3D12_Texture *tex = ArenaPushStruct(ctx->arena, R_D3D12_Texture);
  tex->state = D3D12_RESOURCE_STATE_COMMON;
  switch (desc.init_state) {
    case R_TextureInitState_RenderTarget: { tex->state = D3D12_RESOURCE_STATE_RENDER_TARGET;          }break;
    case R_TextureInitState_DepthWrite:   { tex->state = D3D12_RESOURCE_STATE_DEPTH_WRITE;            }break;
    case R_TextureInitState_CopyDest:     { tex->state = D3D12_RESOURCE_STATE_COPY_DEST;              }break;
    case R_TextureInitState_ShaderRead:   { tex->state =  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE; }break;
  }
  result.state = r_state_from_d3d12_state(tex->state);

  D3D12_HEAP_PROPERTIES heap = {};
  heap.Type = D3D12_HEAP_TYPE_DEFAULT;
  HRESULT hr = ctx->device->CreateCommittedResource(
    &heap,
    D3D12_HEAP_FLAG_NONE,
    &rdesc,
    tex->state,
    clear_ptr,
    IID_PPV_ARGS(&tex->resource)
  );
  Assert(SUCCEEDED(hr));

  #if 0
  if (desc.usage & R_TextureUsage_Sampled) {
    S32 srv_idx = r_alloc_texture_descriptor_idx_srv();
    r_d3d12_write_srv(tex->resource, dxgi_fmt, desc.mips_count, srv_idx);
    result.srv_idx = srv_idx;
  }
  if (desc.usage & R_TextureUsage_RenderTarget) {
    S32 rtv_idx = r_alloc_texture_descriptor_idx_rtv();
    r_d3d12_write_rtv(tex->resource, dxgi_fmt, rtv_idx);
    result.rtv_idx = rtv_idx;
  }
  if (desc.usage & R_TextureUsage_DepthStencil) {
    S32 dsv_idx = r_alloc_texture_descriptor_idx_dsv();
    r_d3d12_write_dsv(tex->resource, dxgi_fmt, dsv_idx);
    result.dsv_idx = dsv_idx;
  }
  #endif

  if (init_count > 0) {
    r_d3d12_upload_texture(tex, dxgi_fmt, init, init_count);
    result.fence_value = ctx->copy_fence_value;
  } else {
    result.fence_value = 0;
  }

  result.backend = (void *)tex;
  return result;
}

//
// Buffers
//

static D3D12_VERTEX_BUFFER_VIEW
r_d3d12_vertex_buffer_view_from_buffer(R_Handle handle)
{
  D3D12_VERTEX_BUFFER_VIEW result = {};

  R_ResourceSlot *slot = &r_resource_table.slots[handle.idx];
  R_D3D12_Buffer *buff = (R_D3D12_Buffer *)slot->backend_rsrc;
  if (buff) {
    result = buff->vbv;
  }

  return result;
}

static D3D12_INDEX_BUFFER_VIEW
r_d3d12_index_buffer_view_from_buffer(R_Handle handle)
{
  D3D12_INDEX_BUFFER_VIEW result = {};

  R_ResourceSlot *slot = &r_resource_table.slots[handle.idx];
  R_D3D12_Buffer *buff = (R_D3D12_Buffer *)slot->backend_rsrc;
  if (buff) {
    result = buff->ibv;
  }

  return result;
}


// @Note: Incomplete
static void
r_d3d12_buffer_flags_state_from_desc(R_BufferDesc desc, D3D12_RESOURCE_FLAGS *out_flags, D3D12_RESOURCE_STATES *out_state)
{
  *out_flags = D3D12_RESOURCE_FLAG_NONE;

  switch (desc.memory) {
    case R_BufferMemory_Default: {
      *out_state = D3D12_RESOURCE_STATE_COPY_DEST;
    }break;

    case R_BufferMemory_Upload: {
      *out_state = D3D12_RESOURCE_STATE_GENERIC_READ;
    }break;

    case R_BufferMemory_Readback: {
      *out_state = D3D12_RESOURCE_STATE_COPY_DEST;
    }break;
  }
}

// @Todo: Test; error-checking and input validation.
static R_CreateResource
r_create_buffer_impl(R_BufferInitData init, R_BufferDesc desc)
{
  R_D3D12_Backend *ctx = &r_ctx;
  R_CreateResource result = {};

  R_D3D12_Buffer *buf = ArenaPushStruct(ctx->arena, R_D3D12_Buffer);
  buf->size = desc.size;

  // ---------------------------------------------------------------------------
  // Resource description
  // ---------------------------------------------------------------------------

  D3D12_RESOURCE_DESC res_desc = {};
  res_desc.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
  res_desc.Width            = desc.size;
  res_desc.Height           = 1;
  res_desc.DepthOrArraySize = 1;
  res_desc.MipLevels        = 1;
  res_desc.SampleDesc.Count = 1;
  res_desc.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

  D3D12_RESOURCE_FLAGS flags;
  D3D12_RESOURCE_STATES initial_state;
  r_d3d12_buffer_flags_state_from_desc(desc, &flags, &initial_state);
  res_desc.Flags = flags;

  result.state = r_state_from_d3d12_state(initial_state);

  // ---------------------------------------------------------------------------
  // Heap type selection
  // ---------------------------------------------------------------------------

  D3D12_HEAP_PROPERTIES heap = {};
  switch (desc.memory) {
    case R_BufferMemory_Default:  { heap.Type = D3D12_HEAP_TYPE_DEFAULT;  }break;
    case R_BufferMemory_Upload:   { heap.Type = D3D12_HEAP_TYPE_UPLOAD;   }break;
    case R_BufferMemory_Readback: { heap.Type = D3D12_HEAP_TYPE_READBACK; }break;
  }

  // ---------------------------------------------------------------------------
  // Create GPU resource
  // ---------------------------------------------------------------------------

  HRESULT hr = ctx->device->CreateCommittedResource(
    &heap,
    D3D12_HEAP_FLAG_NONE,
    &res_desc,
    initial_state,
    0,
    IID_PPV_ARGS(&buf->resource)
  );
  Assert(SUCCEEDED(hr));
  buf->state = initial_state;

  // ---------------------------------------------------------------------------
  // Upload path (only if init.data is provided)
  // ---------------------------------------------------------------------------

  if (init.data) {
    if (desc.memory == R_BufferMemory_Default) {
      // Create upload buffer
      ID3D12Resource *upload = 0;

      D3D12_HEAP_PROPERTIES upload_heap = {};
      upload_heap.Type = D3D12_HEAP_TYPE_UPLOAD;

      hr = ctx->device->CreateCommittedResource(
        &upload_heap,
        D3D12_HEAP_FLAG_NONE,
        &res_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        0,
        IID_PPV_ARGS(&upload)
      );
      Assert(SUCCEEDED(hr));

      // Map and copy
      void *mapped = 0;
      upload->Map(0, 0, &mapped);
      MemoryCopy(mapped, init.data, desc.size);
      upload->Unmap(0, 0);

      ctx->copy_cmd_allocator->Reset();
      ctx->copy_cmd_list->Reset(ctx->copy_cmd_allocator, 0);

      // Copy into default buffer
      ctx->copy_cmd_list->CopyBufferRegion(
        buf->resource, 0,
        upload, 0,
        desc.size
      );

      // Transition to final state
      D3D12_RESOURCE_STATES final_state = {};
      switch (desc.usage) {
        case R_BufferUsage_Vertex: { final_state = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER; }break;
        case R_BufferUsage_Index:  { final_state = D3D12_RESOURCE_STATE_INDEX_BUFFER;               }break;
      }

      D3D12_RESOURCE_BARRIER barrier = {};
      barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      barrier.Transition.pResource = buf->resource;
      barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
      barrier.Transition.StateAfter  = final_state;
      barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

      ctx->copy_cmd_list->ResourceBarrier(1, &barrier);
      buf->state = final_state;

      ctx->copy_cmd_list->Close();
      ID3D12CommandList *lists[] = { ctx->copy_cmd_list };
      ctx->command_queue->ExecuteCommandLists(1, lists);

      ctx->copy_fence_value += 1;
      ctx->command_queue->Signal(ctx->copy_fence, ctx->copy_fence_value);

      result.fence_value = ctx->copy_fence_value;

      //upload->Release(); // @Todo: Only safe if you guarantee the upload resource is not destroyed until the fence passes.
    }
    else {
      // Upload heap buffer: direct map
      void *mapped = 0;
      buf->resource->Map(0, 0, &mapped);
      MemoryCopy(mapped, init.data, desc.size);
      buf->resource->Unmap(0, 0);

      result.fence_value = 0; // @Note: This doesn't need a fence value; need to treat 0 specially when checking fence values.
    }
  }

  switch (desc.usage) {
    case R_BufferUsage_Vertex: {
      D3D12_VERTEX_BUFFER_VIEW vbv = {};
      vbv.BufferLocation = buf->resource->GetGPUVirtualAddress();
      vbv.SizeInBytes = (UINT)buf->size;
      vbv.StrideInBytes = desc.stride_bytes;
      buf->vbv = vbv;
    }break;
    case R_BufferUsage_Index: {
      D3D12_INDEX_BUFFER_VIEW ibv = {};
      ibv.BufferLocation = buf->resource->GetGPUVirtualAddress();
      ibv.SizeInBytes = (UINT)buf->size;
      ibv.Format = ((desc.index_kind == R_IndexKind_U16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT);
      buf->ibv = ibv;
    }break;
  }

  result.backend = (void *)buf;
  return result;
}

//
// Pipelines
//

static D3D12_PRIMITIVE_TOPOLOGY_TYPE
r_d3d12_topology_kind_from_r(R_TopologyKind kind)
{
  D3D12_PRIMITIVE_TOPOLOGY_TYPE result = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

  switch (kind) {
    case R_TopologyKind_Triangle: { result = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; }break;
    case R_TopologyKind_Line:     { result = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE; }break;
    case R_TopologyKind_Point:    { result = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT; }break;
  }

  return result;
}

static D3D12_PRIMITIVE_TOPOLOGY
r_d3d12_topology_from_r(R_Topology topology)
{
  D3D12_PRIMITIVE_TOPOLOGY result = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

  switch (topology) {
    case R_Topology_TriangleList:  { result = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST; }break;
    case R_Topology_TriangleStrip: { result = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; }break;
    case R_Topology_LineList:      { result = D3D_PRIMITIVE_TOPOLOGY_LINELIST; }break;
    case R_Topology_LineStrip:     { result = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP; }break;
    case R_Topology_PointList:     { result = D3D_PRIMITIVE_TOPOLOGY_POINTLIST; }break;
  }

  return result;
}

static D3D12_FILL_MODE
r_d3d12_fill_mode_from_r(R_FillMode m)
{
  D3D12_FILL_MODE result = D3D12_FILL_MODE_SOLID;

  switch(m) {
    case R_FillMode_Solid:     { result = D3D12_FILL_MODE_SOLID; }break;
    case R_FillMode_Wireframe: { result = D3D12_FILL_MODE_WIREFRAME; }break;
  }

  return result;
}

static D3D12_CULL_MODE
r_d3d12_cull_mode_from_r(R_CullMode m)
{
  D3D12_CULL_MODE result = D3D12_CULL_MODE_BACK;

  switch(m) {
    case R_CullMode_None:  { result = D3D12_CULL_MODE_NONE; }break;
    case R_CullMode_Front: { result = D3D12_CULL_MODE_FRONT; }break;
    case R_CullMode_Back:  { result = D3D12_CULL_MODE_BACK; }break;
  }

  return result;
}

static D3D12_COMPARISON_FUNC
r_d3d12_compare_func_from_r(R_CompareOp op)
{
  D3D12_COMPARISON_FUNC result = D3D12_COMPARISON_FUNC_LESS_EQUAL;

  switch(op) {
    case R_CompareOp_Never:        { result = D3D12_COMPARISON_FUNC_NEVER; }break;
    case R_CompareOp_Less:         { result = D3D12_COMPARISON_FUNC_LESS; }break;
    case R_CompareOp_Equal:        { result = D3D12_COMPARISON_FUNC_EQUAL; }break;
    case R_CompareOp_LessEqual:    { result = D3D12_COMPARISON_FUNC_LESS_EQUAL; }break;
    case R_CompareOp_Greater:      { result = D3D12_COMPARISON_FUNC_GREATER; }break;
    case R_CompareOp_NotEqual:     { result = D3D12_COMPARISON_FUNC_NOT_EQUAL; }break;
    case R_CompareOp_GreaterEqual: { result = D3D12_COMPARISON_FUNC_GREATER_EQUAL; }break;
    case R_CompareOp_Always:       { result = D3D12_COMPARISON_FUNC_ALWAYS; }break;
  }

  return result;
}

static D3D12_STENCIL_OP
r_d3d12_stencil_op_from_r(R_StencilOp op)
{
  D3D12_STENCIL_OP result = D3D12_STENCIL_OP_KEEP;

  switch(op) {
    case R_StencilOp_Keep:     { result = D3D12_STENCIL_OP_KEEP; }break;
    case R_StencilOp_Zero:     { result = D3D12_STENCIL_OP_ZERO; }break;
    case R_StencilOp_Replace:  { result = D3D12_STENCIL_OP_REPLACE; }break;
    case R_StencilOp_IncClamp: { result = D3D12_STENCIL_OP_INCR_SAT; }break;
    case R_StencilOp_DecClamp: { result = D3D12_STENCIL_OP_DECR_SAT; }break;
    case R_StencilOp_Invert:   { result = D3D12_STENCIL_OP_INVERT; }break;
    case R_StencilOp_IncWrap:  { result = D3D12_STENCIL_OP_INCR; }break;
    case R_StencilOp_DecWrap:  { result = D3D12_STENCIL_OP_DECR; }break;
  }

  return result;
}

static D3D12_BLEND
r_d3d12_blend_from_r(R_BlendFactor f)
{
  D3D12_BLEND result = D3D12_BLEND_ONE;

  switch(f) {
    case R_BlendFactor_Zero:         { result = D3D12_BLEND_ZERO; }break;
    case R_BlendFactor_One:          { result = D3D12_BLEND_ONE; }break;
    case R_BlendFactor_SrcColor:     { result = D3D12_BLEND_SRC_COLOR; }break;
    case R_BlendFactor_InvSrcColor:  { result = D3D12_BLEND_INV_SRC_COLOR; }break;
    case R_BlendFactor_SrcAlpha:     { result = D3D12_BLEND_SRC_ALPHA; }break;
    case R_BlendFactor_InvSrcAlpha:  { result = D3D12_BLEND_INV_SRC_ALPHA; }break;
    case R_BlendFactor_DestAlpha:    { result = D3D12_BLEND_DEST_ALPHA; }break;
    case R_BlendFactor_InvDestAlpha: { result = D3D12_BLEND_INV_DEST_ALPHA; }break;
    case R_BlendFactor_DestColor:    { result = D3D12_BLEND_DEST_COLOR; }break;
    case R_BlendFactor_InvDestColor: { result = D3D12_BLEND_INV_DEST_COLOR; }break;
  }

  return result;
}

static D3D12_BLEND_OP
r_d3d12_blend_op_from_r(R_BlendOp op)
{
  D3D12_BLEND_OP result = D3D12_BLEND_OP_ADD;

  switch(op) {
    case R_BlendOp_Add:        { result = D3D12_BLEND_OP_ADD; }break;
    case R_BlendOp_Subtract:   { result = D3D12_BLEND_OP_SUBTRACT; }break;
    case R_BlendOp_RevSubtract:{ result = D3D12_BLEND_OP_REV_SUBTRACT; }break;
    case R_BlendOp_Min:        { result = D3D12_BLEND_OP_MIN; }break;
    case R_BlendOp_Max:        { result = D3D12_BLEND_OP_MAX; }break;
  }

  return result;
}

static void
r_d3d12_input_layout_from_r(R_Layout *layout, D3D12_INPUT_ELEMENT_DESC *out, S32 *out_count)
{
  S32 n = (S32)layout->elements_count;
  n = Min(16, n);

  for (S32 i = 0; i < n; i += 1) {
    R_InputElement *e = &layout->elements[i];

    D3D12_INPUT_ELEMENT_DESC d = {};
    d.SemanticName         = chr_from_str8(e->semantic_name);
    d.SemanticIndex        = (UINT)e->semantic_index;
    d.Format               =  r_d3d12_fmt_from_r_fmt(e->format);
    d.InputSlot            = (UINT)e->input_slot;
    d.AlignedByteOffset    = (UINT)e->byte_offset;

    if (e->input_class == R_VertexInputClass_PerInstance) {
      d.InputSlotClass         = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
      d.InstanceDataStepRate   = (UINT)e->instance_step_rate;
    } else {
      d.InputSlotClass         = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
      d.InstanceDataStepRate   = 0;
    }

    out[i] = d;
  }

  *out_count = n;
}

static D3D12_RASTERIZER_DESC
r_d3d12_raster_from_r(R_RasterizerState *s)
{
  D3D12_RASTERIZER_DESC r = {};

  r.FillMode              = r_d3d12_fill_mode_from_r(s->fill_mode);
  r.CullMode              = r_d3d12_cull_mode_from_r(s->cull_mode);
  r.FrontCounterClockwise = s->front_ccw ? TRUE : FALSE;

  r.DepthBias             = (INT)s->depth_bias;
  r.DepthBiasClamp        = s->depth_bias_clamp;
  r.SlopeScaledDepthBias  = s->slope_scaled_depth_bias;

  r.DepthClipEnable       = s->depth_clip_enable ? TRUE : FALSE;
  r.MultisampleEnable     = s->multisample_enable ? TRUE : FALSE;
  r.AntialiasedLineEnable = FALSE;
  r.ForcedSampleCount     = 0;
  r.ConservativeRaster    = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

  return r;
}

static D3D12_DEPTH_STENCILOP_DESC
r_d3d12_stencil_face_from_r(R_StencilFaceState *s)
{
  D3D12_DEPTH_STENCILOP_DESC d = {};

  d.StencilFailOp      = r_d3d12_stencil_op_from_r(s->fail_op);
  d.StencilDepthFailOp = r_d3d12_stencil_op_from_r(s->depth_fail_op);
  d.StencilPassOp      = r_d3d12_stencil_op_from_r(s->pass_op);
  d.StencilFunc        = r_d3d12_compare_func_from_r(s->compare_op);

  return d;
}

static D3D12_DEPTH_STENCIL_DESC
r_d3d12_depthstencil_from_r(R_DepthStencilState *s)
{
  D3D12_DEPTH_STENCIL_DESC d = {};

  d.DepthEnable      = s->depth_enable ? TRUE : FALSE;
  d.DepthWriteMask   = s->depth_write_enable ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
  d.DepthFunc        = r_d3d12_compare_func_from_r(s->depth_compare);

  d.StencilEnable    = s->stencil_enable ? TRUE : FALSE;
  d.StencilReadMask  = s->stencil_read_mask;
  d.StencilWriteMask = s->stencil_write_mask;

  d.FrontFace = r_d3d12_stencil_face_from_r(&s->front_face);
  d.BackFace  = r_d3d12_stencil_face_from_r(&s->back_face);

  return d;
}

static D3D12_BLEND_DESC
r_d3d12_blend_from_r(R_BlendState *s)
{
  D3D12_BLEND_DESC b = {};

  b.AlphaToCoverageEnable  = s->alpha_to_coverage_enable ? TRUE : FALSE;
  b.IndependentBlendEnable = s->independent_blend_enable ? TRUE : FALSE;

  for (S32 i = 0; i < 8; i += 1) {
    R_RenderTargetBlendState *rt = &s->targets[i];

    D3D12_RENDER_TARGET_BLEND_DESC out = {};
    out.BlendEnable           = rt->blend_enable ? TRUE : FALSE;
    out.LogicOpEnable         = FALSE;

    out.SrcBlend              = r_d3d12_blend_from_r(rt->src_color);
    out.DestBlend             = r_d3d12_blend_from_r(rt->dst_color);
    out.BlendOp               = r_d3d12_blend_op_from_r(rt->color_op);

    out.SrcBlendAlpha         = r_d3d12_blend_from_r(rt->src_alpha);
    out.DestBlendAlpha        = r_d3d12_blend_from_r(rt->dst_alpha);
    out.BlendOpAlpha          = r_d3d12_blend_op_from_r(rt->alpha_op);

    out.LogicOp               = D3D12_LOGIC_OP_NOOP;
    out.RenderTargetWriteMask = rt->write_mask;

    b.RenderTarget[i] = out;
  }

  return b;
}

// @Todo: Use String8, convert to LPCWSTR
ID3DBlob *
r_d3d12_compile_hlsl(LPCWSTR path, char *entry, char *version)
{
  // Compile shaders
  ID3DBlob *shader_blob = 0;
  ID3DBlob *err_blob = 0;

#if BUILD_DEBUG
  UINT compile_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
  UINT compile_flags = 0;
#endif

  HRESULT hr = D3DCompileFromFile(path, 0, 0, entry, version, compile_flags, 0, &shader_blob, &err_blob);
  if (FAILED(hr)) {
    if (err_blob) {
      OutputDebugStringA((char *)err_blob->GetBufferPointer());
      err_blob->Release();
    }
    Assert(SUCCEEDED(hr));
  }

  return shader_blob;
}

static R_CreateResource
r_create_pipeline_impl(R_PipelineDesc desc)
{
  R_D3D12_Backend *ctx = &r_ctx;
  R_CreateResource result = {};

  R_D3D12_Pipeline *pipe = ArenaPushStruct(ctx->arena, R_D3D12_Pipeline);

  pipe->vs_path = desc.vs_path;
  pipe->ps_path = desc.ps_path;
  pipe->root_sig = ctx->root_signature; // @Note: Using a single shared root signature that all pipelines will agree upon.

  // @Todo: Should probably free later
  ID3DBlob *vs_blob = r_d3d12_compile_hlsl(desc.vs_path, "vs_main", "vs_5_1");
  ID3DBlob *ps_blob = 0;
  if (desc.ps_path != 0) {
    ps_blob = r_d3d12_compile_hlsl(desc.ps_path, "ps_main", "ps_5_1");
  }
  Assert(vs_blob != 0);

  if (desc.input_layout) {
    r_d3d12_input_layout_from_r(desc.input_layout, pipe->input_layout, &pipe->input_layout_count);
  }

  pipe->raster        = r_d3d12_raster_from_r(&desc.raster);
  pipe->depth_stencil = r_d3d12_depthstencil_from_r(&desc.depth_stencil);
  pipe->blend         = r_d3d12_blend_from_r(&desc.blend);
  pipe->topology_type = r_d3d12_topology_kind_from_r(desc.topology);

  pipe->rtv_count = desc.rt_count;

  for (S32 i = 0; i < pipe->rtv_count; i += 1) {
    pipe->rtv_formats[i] = r_d3d12_fmt_from_r_fmt(desc.rt_formats[i]);
  }
  pipe->dsv_format = r_d3d12_fmt_from_r_fmt(desc.depth_format);
  pipe->sample_desc.Count = (UINT)desc.sample_count;
  pipe->sample_desc.Quality = 0;

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
  pso.pRootSignature = pipe->root_sig;

  pso.VS = CD3DX12_SHADER_BYTECODE(vs_blob);
  pso.PS = CD3DX12_SHADER_BYTECODE(ps_blob);

  pso.BlendState        = pipe->blend;
  pso.SampleMask        = UINT_MAX;
  pso.RasterizerState   = pipe->raster;
  pso.DepthStencilState = pipe->depth_stencil;

  if (desc.input_layout && pipe->input_layout_count != 0) {
    pso.InputLayout.pInputElementDescs = pipe->input_layout;
    pso.InputLayout.NumElements = (UINT)pipe->input_layout_count;
  }
  else {
    pso.InputLayout.pInputElementDescs = 0;
    pso.InputLayout.NumElements = 0;
  }

  pso.PrimitiveTopologyType = pipe->topology_type;
  pso.NumRenderTargets = (UINT)pipe->rtv_count;

  for (S32 i = 0; i < 8; i += 1) {
    if (i < pipe->rtv_count) {
      pso.RTVFormats[i] = pipe->rtv_formats[i];
    } else {
      pso.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
    }
  }

  pso.DSVFormat  = pipe->dsv_format;
  pso.SampleDesc = pipe->sample_desc;

  pso.IBStripCutValue                 = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
  pso.NodeMask                        = 0;
  pso.CachedPSO.pCachedBlob           = 0;
  pso.CachedPSO.CachedBlobSizeInBytes = 0;
  pso.Flags                           = D3D12_PIPELINE_STATE_FLAG_NONE;

  pipe->pso_desc = pso;

  HRESULT hr = ctx->device->CreateGraphicsPipelineState(&pipe->pso_desc, IID_PPV_ARGS(&pipe->pso));
  Assert(SUCCEEDED(hr));

  result.fence_value = 0;
  result.backend = (void *)pipe;
  return result;
}
