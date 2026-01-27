static S32
r_alloc_resource_slot(void)
{
  S32 slot = r_resource_table.count;
  r_resource_table.count += 1;
  return slot;
  // @Todo: Free list
}

static R_Handle
r_create_texture(R_TextureInitData *init, S32 init_count, R_TextureDesc desc)
{
  S32 slot_idx = r_alloc_resource_slot();
  R_ResourceSlot *slot = &r_resource_table.slots[slot_idx];

  slot->gen += 1;
  slot->kind = R_ResourceKind_Texture;
  slot->descriptor_idx = r_alloc_texture_descriptor_idx();

  R_CreateResource create = r_create_texture_impl(init, init_count, desc, slot->descriptor_idx);
  slot->backend_rsrc = create.backend;

  R_Handle result = {
    .idx = slot_idx,
    .gen = slot->gen,
  };
  return result;
}
