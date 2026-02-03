//
// D3D12 Resources
//

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

//
// Textures
//

static S32
r_alloc_texture_descriptor_idx(void)
{
  R_Context *ctx = &r_ctx;

  // @Todo: Free list
  S32 idx = ctx->srv_next_idx;
  ctx->srv_next_idx += 1;

  return idx;
}

static DXGI_FORMAT
r_d3d12_fmt_from_texture_fmt(R_TextureFmt fmt)
{
  DXGI_FORMAT result = DXGI_FORMAT_UNKNOWN;

  switch (fmt) {
    case R_TextureFmt_RGBA8_UNORM:  { result = DXGI_FORMAT_R8G8B8A8_UNORM;     } break;
    case R_TextureFmt_RGBA16_FLOAT: { result = DXGI_FORMAT_R16G16B16A16_FLOAT; } break;
    case R_TextureFmt_BC1_UNORM:    { result = DXGI_FORMAT_BC1_UNORM;          } break;
    case R_TextureFmt_BC3_UNORM:    { result = DXGI_FORMAT_BC3_UNORM;          } break;
    case R_TextureFmt_BC4_UNORM:    { result = DXGI_FORMAT_BC4_UNORM;          } break;
    case R_TextureFmt_BC5_UNORM:    { result = DXGI_FORMAT_BC5_UNORM;          } break;
    case R_TextureFmt_BC7_UNORM:    { result = DXGI_FORMAT_BC7_UNORM;          } break;
  }

  return result;
}

static void
r_d3d12_write_srv(ID3D12Resource *resource, DXGI_FORMAT fmt, S32 mips_count, S32 descriptor_idx)
{
  R_Context *ctx = &r_ctx;

  D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
  srv_desc.Format = fmt;
  srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srv_desc.Texture2D.MipLevels = mips_count;

  D3D12_CPU_DESCRIPTOR_HANDLE handle =
    ctx->srv_heap->GetCPUDescriptorHandleForHeapStart();
  handle.ptr += (SIZE_T)descriptor_idx * ctx->srv_descriptor_size;

  ctx->device->CreateShaderResourceView(resource, &srv_desc, handle);
}

static U64
r_d3d12_calc_upload_size(ID3D12Resource *dst, S32 subresource_count, D3D12_PLACED_SUBRESOURCE_FOOTPRINT *out_layouts, U64 *out_total_size)
{
  R_Context *ctx = &r_ctx;

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
  R_Context *ctx = &r_ctx;

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

        #if 0
        U32 w = layouts[i].Footprint.Width;
        U32 h = layouts[i].Footprint.Height;

        U32 blocks_x = (w + 3) / 4; if (blocks_x == 0) blocks_x = 1;
        U32 blocks_y = (h + 3) / 4; if (blocks_y == 0) blocks_y = 1;

        U32 src_row_bytes = blocks_x * bpb;
        Assert(src_row_bytes <= dst_row_pitch);

        for (U32 y = 0; y < blocks_y; y += 1) {
          MemoryCopy(
            dst_base + (U64)y * dst_row_pitch,
            src_base + (U64)y * src_row_bytes,
            // src_base + (U64)y * init[i].row_pitch,
            src_row_bytes
          );
        }
        #endif
        U32 dst_row_pitch = layouts[i].Footprint.RowPitch;

        // Source layout must come from your init data (DirectXTex)
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
// @Todo: Limited to 2D sampled textures right now.
static R_CreateResource
r_create_texture_impl(R_TextureInitData *init, S32 init_count, R_TextureDesc desc, S32 descriptor_idx)
{
  R_Context *ctx = &r_ctx;
  R_CreateResource result = {};

  DXGI_FORMAT dxgi_fmt = r_d3d12_fmt_from_texture_fmt(desc.fmt);
  D3D12_RESOURCE_DESC rdesc = {};
  rdesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  rdesc.Width = desc.width;
  rdesc.Height = desc.height;
  rdesc.DepthOrArraySize = 1;
  rdesc.MipLevels = (U16)init_count; //(U16)desc.mips_count;
  rdesc.Format = dxgi_fmt;
  rdesc.SampleDesc.Count = 1;
  rdesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  rdesc.Flags = D3D12_RESOURCE_FLAG_NONE;

  D3D12_HEAP_PROPERTIES heap = {};
  heap.Type = D3D12_HEAP_TYPE_DEFAULT;

  R_D3D12_Texture *tex = ArenaPushStruct(ctx->arena, R_D3D12_Texture);

  HRESULT hr = ctx->device->CreateCommittedResource(
    &heap,
    D3D12_HEAP_FLAG_NONE,
    &rdesc,
    D3D12_RESOURCE_STATE_COPY_DEST,
    0,
    IID_PPV_ARGS(&tex->resource)
  );
  Assert(SUCCEEDED(hr));
  tex->state = D3D12_RESOURCE_STATE_COPY_DEST;

  r_d3d12_write_srv(tex->resource, dxgi_fmt, init_count, descriptor_idx);

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
  R_Context *ctx = &r_ctx;
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
r_d3d12_topology_from_r(R_TopologyKind kind)
{
  D3D12_PRIMITIVE_TOPOLOGY_TYPE result = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

  switch(kind) {
    case R_TopologyKind_Triangle: { result = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; }break;
    case R_TopologyKind_Line:     { result = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE; }break;
    case R_TopologyKind_Point:    { result = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT; }break;
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
  if (n > 16) {
    n = 16;
  }

  for (S32 i = 0; i < n; i += 1) {
    R_InputElement *e = &layout->elements[i];

    D3D12_INPUT_ELEMENT_DESC d = {};
    d.SemanticName         = chr_from_str8(e->semantic_name);
    d.SemanticIndex        = (UINT)e->semantic_index;
    d.Format               =  r_d3d12_fmt_from_texture_fmt(e->format);
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
  R_Context *ctx = &r_ctx;
  R_CreateResource result = {};

  R_D3D12_Pipeline *pipe = ArenaPushStruct(ctx->arena, R_D3D12_Pipeline);
  MemoryZeroStruct(pipe);

  pipe->vs_path = desc.vs_path;
  pipe->ps_path = desc.ps_path;
  pipe->root_sig = ctx->root_signature; // @Note: Using a single shared root signature that all pipelines will agree upon.

  // @Todo: Should probably free later
  ID3DBlob *vs_blob = r_d3d12_compile_hlsl(desc.vs_path, "vs_main", "vs_6_6");
  ID3DBlob *ps_blob = 0;
  if (desc.ps_path != 0) {
    ps_blob = r_d3d12_compile_hlsl(desc.ps_path, "ps_main", "ps_6_6");
  }
  Assert(vs_blob != 0);

  r_d3d12_input_layout_from_r(&desc.input_layout, pipe->input_layout, &pipe->input_layout_count);

  pipe->raster        = r_d3d12_raster_from_r(&desc.raster);
  pipe->depth_stencil = r_d3d12_depthstencil_from_r(&desc.depth_stencil);
  pipe->blend         = r_d3d12_blend_from_r(&desc.blend);
  pipe->topology_type = r_d3d12_topology_from_r(desc.topology);

  pipe->rtv_count = desc.rt_count;

  for (S32 i = 0; i < pipe->rtv_count; i += 1) {
    pipe->rtv_formats[i] = r_d3d12_fmt_from_texture_fmt(desc.rt_formats[i]);
  }
  pipe->dsv_format = r_d3d12_fmt_from_texture_fmt(desc.depth_format);
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

  if (pipe->input_layout_count != 0) {
    pso.InputLayout.pInputElementDescs = pipe->input_layout;
    pso.InputLayout.NumElements        = (UINT)pipe->input_layout_count;
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
