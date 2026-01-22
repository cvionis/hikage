static S32
r_alloc_resource_slot(void)
{
  S32 slot = r_resource_table.count;
  r_resource_table.count += 1;
  return slot;
  // @Todo: Free list
}

static R_Handle
r_create_texture(R_TextureData *init, R_TextureDesc desc)
{
  S32 slot_idx = r_alloc_resource_slot();
  R_ResourceSlot *slot = &r_resource_table.slots[slot_idx];

  slot->gen += 1;
  slot->kind = R_ResourceKind_Texture;
  slot->descriptor_idx = r_alloc_descriptor_idx(slot->kind);

  // @Note: Temporary; will eventually push a create_texture() command to a resource work queue.
  // Will probably just have to pass this pointer to the job, and it will fill it out when the work is completed,
  // rather than returning this from a function.
  slot->backend_rsrc = r_create_texture_impl(init, desc);

  R_Handle result = {
    .idx = slot_idx,
    .gen = slot->gen,
  };
  return result;
}
