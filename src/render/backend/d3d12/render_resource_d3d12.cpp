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
      U8 *dst_base = (U8 *)mapped + layouts[i].Offset;
      U8 *src_base = (U8 *)init[i].data;

      U32 dst_row_pitch = layouts[i].Footprint.RowPitch;

      if (is_block_compressed(fmt)) {
        U32 bpb = bc_bytes_per_block(fmt);

        U32 w = layouts[i].Footprint.Width;
        U32 h = layouts[i].Footprint.Height;

        U32 blocks_x = (w + 3) / 4; if (blocks_x == 0) blocks_x = 1;
        U32 blocks_y = (h + 3) / 4; if (blocks_y == 0) blocks_y = 1;

        U32 src_row_bytes = blocks_x * bpb;

        Assert(init[i].row_pitch >= (S32)src_row_bytes);
        Assert(src_row_bytes <= dst_row_pitch);

        for (U32 y = 0; y < blocks_y; y += 1) {
          MemoryCopy(
            dst_base + (U64)y * dst_row_pitch,
            src_base + (U64)y * src_row_bytes,
            src_row_bytes
          );
        }
      }
      else {
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
    r_d3d12_upload_texture(tex, dxgi_fmt, init, init_count);
    result.fence_value = ctx->copy_fence_value;
  } else {
    result.fence_value = 0;
  }

  result.backend = (void *)tex;
  return result;
}
