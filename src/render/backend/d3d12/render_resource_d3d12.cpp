static S32
r_alloc_texture_descriptor_idx(void)
{
  R_Context *ctx = &r_ctx;

  // @Todo: Free list
  S32 idx = ctx->srv_next_idx;
  ctx->srv_next_idx += 1;

  return idx;
}

struct R_D3D12_Texture {
  ID3D12Resource *resource;
  D3D12_RESOURCE_STATES state;
};

static DXGI_FORMAT
r_d3d12_fmt_from_texture_fmt(R_TextureFmt fmt)
{
  DXGI_FORMAT result = DXGI_FORMAT_UNKNOWN;

  switch (fmt) {
    case R_TextureFmt_RGBA8_UNORM:  { result = DXGI_FORMAT_R8G8B8A8_UNORM;     } break;
    case R_TextureFmt_RGBA16_FLOAT: { result = DXGI_FORMAT_R16G16B16A16_FLOAT; } break;
    case R_TextureFmt_BC1_UNORM:    { result = DXGI_FORMAT_BC1_UNORM;          } break;
    case R_TextureFmt_BC3_UNORM:    { result = DXGI_FORMAT_BC3_UNORM;          } break;
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

static void
r_d3d12_upload_texture(R_D3D12_Texture *tex, R_TextureInitData *init, S32 init_count)
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

    ctx->device->CreateCommittedResource(
      &heap,
      D3D12_HEAP_FLAG_NONE,
      &desc,
      D3D12_RESOURCE_STATE_GENERIC_READ,
      0,
      IID_PPV_ARGS(&upload)
    );
  }

  void *mapped = 0;
  upload->Map(0, 0, &mapped);
  {
    for (S32 i = 0; i < init_count; i += 1) {
      U8 *dst = (U8 *)mapped + layouts[i].Offset;
      U8 *src = (U8 *)init[i].data;

      for (U32 y = 0; y < layouts[i].Footprint.Height; y += 1) {
        MemoryCopy(
          dst + y * layouts[i].Footprint.RowPitch,
          src + y * init[i].row_pitch,
          init[i].row_pitch
        );
      }
    }
  }
  upload->Unmap(0, 0);

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

static R_CreateResource r_create_texture_impl(R_TextureInitData *init, S32 init_count, R_TextureDesc desc, S32 descriptor_idx)
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

  ctx->device->CreateCommittedResource(
    &heap,
    D3D12_HEAP_FLAG_NONE,
    &rdesc,
    D3D12_RESOURCE_STATE_COPY_DEST,
    0,
    IID_PPV_ARGS(&tex->resource)
  );
  tex->state = D3D12_RESOURCE_STATE_COPY_DEST;

  r_d3d12_write_srv(tex->resource, dxgi_fmt, init_count, descriptor_idx);

  if (init_count > 0) {
    r_d3d12_upload_texture(tex, init, init_count);
    result.fence_value = ctx->copy_fence_value;
  } else {
    result.fence_value = 0;
  }

  result.backend = (void *)tex;
  return result;
}
