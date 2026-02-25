
static R_Format
r_texture_get_fmt(R_Handle handle)
{
  R_ResourceSlot *slot = &r_resource_table.slots[handle.idx];
  R_Format fmt = slot->fmt;
  return fmt;
}

static S32
r_descriptor_idx_from_view(R_Handle handle)
{
  R_View *view = &r_views.slots[handle.idx];
  return view->descriptor_idx;
}

static B32
r_resource_valid(R_Handle handle)
{
  B32 valid = 0;
  // @Todo: Make sure you're setting `alive` wherever you need to.
  if (handle.idx > 0) {
    R_ResourceSlot *slot = &r_resource_table.slots[handle.idx];
    valid = slot->alive;
  }
  return valid;
}

static S32
r_alloc_resource_slot(void)
{
  S32 slot = r_resource_table.count;
  r_resource_table.count += 1;
  return slot;
  // @Todo: Free list
}

// @Todo: Implementation is backend-dependent. Put in _d3d12_resource.cpp
static S32
r_alloc_descriptor_for_view(R_Handle texture, R_ViewDesc desc)
{
  S32 idx = -1;

  R_ResourceSlot *slot = &r_resource_table.slots[texture.idx];
  R_D3D12_Texture *tex = (R_D3D12_Texture *)slot->backend_rsrc;
  DXGI_FORMAT dxgi_fmt = r_d3d12_fmt_from_r_fmt(desc.fmt);

  switch (desc.kind) {
    case R_ViewKind_UnorderedAccess: {
      S32 uav_idx = r_alloc_texture_descriptor_idx_uav(desc.range);
      r_d3d12_write_uav(tex->resource, dxgi_fmt, desc.range, uav_idx);
      idx = uav_idx;
    }break;
    case R_ViewKind_ShaderResource: {
      S32 srv_idx = r_alloc_texture_descriptor_idx_srv(desc.range);
      r_d3d12_write_srv(tex->resource, dxgi_fmt, desc.range, srv_idx);
      idx = srv_idx;
    }break;
    case R_ViewKind_RenderTarget: {
      S32 rtv_idx = r_alloc_texture_descriptor_idx_rtv();
      r_d3d12_write_rtv(tex->resource, dxgi_fmt, desc.range, rtv_idx);
      idx = rtv_idx;
    }break;
    case R_ViewKind_DepthStencil: {
      S32 dsv_idx = r_alloc_texture_descriptor_idx_dsv();
      r_d3d12_write_dsv(tex->resource, dxgi_fmt, desc.range, dsv_idx);
      idx = dsv_idx;
    }break;
  }

  return idx;
}

// Returns a handle to an entry in the view cache (R_View) containing descriptor heap idx if it exists in cache, otherwise allocates
// a descriptor from the appropriate descriptor heap.
static R_Handle
r_view_from_texture(R_Handle texture, R_ViewDesc desc)
{
  R_Handle result = {};

  S32 views_count = r_views.count;
  S32 match_idx = 0;
  R_View *match = 0;
  // @Note: Temporary
  for (; match_idx < views_count; match_idx += 1) {
    R_View *view = &r_views.slots[match_idx];
    if ((view->kind == desc.kind) &&
        (view->resource == texture.idx) &&
        (view->range.slice_start == desc.range.slice_start) &&
        (view->range.slice_count == desc.range.slice_count)) {
      match = view;
      break;
    }
  }

  if (match) {
    result.idx = match_idx;
    result.gen = -1;
  }
  else {
    R_View *new_view = &r_views.slots[views_count];
    new_view->resource = texture.idx;
    new_view->range = desc.range;
    new_view->kind = desc.kind;
    new_view->descriptor_idx = r_alloc_descriptor_for_view(texture, desc);

    result.idx = views_count;
    result.gen = -1;

    r_views.count += 1;
  }

  return result;
}

// @Todo: Reject BCn, depth, sRGB formats for textures created with UnorderedAccess.
static R_Handle
r_create_texture(R_TextureInitData *init, S32 init_count, R_TextureDesc desc)
{
  S32 slot_idx = r_alloc_resource_slot();
  R_ResourceSlot *slot = &r_resource_table.slots[slot_idx];

  slot->gen += 1;
  slot->alive = 1;
  slot->kind = R_ResourceKind_Texture;

  R_CreateResource create = r_create_texture_impl(init, init_count, desc);
  slot->state = create.state;
  slot->fence_value = create.fence_value;
  slot->backend_rsrc = create.backend;
  slot->fmt = desc.fmt;

  R_Handle result = {
    .idx = slot_idx,
    .gen = slot->gen,
  };
  return result;
}

static R_Handle
r_create_buffer(R_BufferInitData init, R_BufferDesc desc)
{
  S32 slot_idx = r_alloc_resource_slot();
  R_ResourceSlot *slot = &r_resource_table.slots[slot_idx];

  // @Todo: texture view indices should be -1... why are they 0 for buffers?
  slot->gen += 1;
  slot->alive = 1;
  slot->kind = R_ResourceKind_Buffer;

  R_CreateResource create = r_create_buffer_impl(init, desc);
  slot->state = create.state;
  slot->fence_value = create.fence_value;
  slot->backend_rsrc = create.backend;

  R_Handle result = {
    .idx = slot_idx,
    .gen = slot->gen,
  };
  return result;
}

static R_Handle
r_create_graphics_pipeline(R_GraphicsPipelineDesc desc)
{
  R_Handle result = {};

  // @Note: Temporary type for vshader path
  if (desc.vs_path != 0) {
    S32 slot_idx = r_alloc_resource_slot();
    R_ResourceSlot *slot = &r_resource_table.slots[slot_idx];

    slot->gen += 1;
    slot->alive = 1;
    slot->kind = R_ResourceKind_GraphicsPipeline;

    R_CreateResource create = r_create_graphics_pipeline_impl(desc);
    slot->fence_value  = create.fence_value;
    slot->backend_rsrc = create.backend;

    result.idx = slot_idx;
    result.gen = slot->gen;
  }

  return result;
}

static R_Handle
r_create_compute_pipeline(R_ComputePipelineDesc desc)
{
  R_Handle result = {};

  // @Note: Temporary type for vshader path
  if (desc.cs_path != 0) {
    S32 slot_idx = r_alloc_resource_slot();
    R_ResourceSlot *slot = &r_resource_table.slots[slot_idx];

    slot->gen += 1;
    slot->alive = 1;
    slot->kind = R_ResourceKind_ComputePipeline;

    R_CreateResource create = r_create_compute_pipeline_impl(desc);
    slot->fence_value  = create.fence_value;
    slot->backend_rsrc = create.backend;

    result.idx = slot_idx;
    result.gen = slot->gen;
  }

  return result;
}
