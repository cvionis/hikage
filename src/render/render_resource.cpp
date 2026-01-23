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

  // @!!Todo: Maybe instead of just not giving this slot a backend_rsrc pointer, you should still give it such a pointer,
  // just to a texture without any initial data. After all, if you don't pass initial data, you DO still want to create a texture in
  // the backend -- just without data. Also, you want to create a descriptor for it in the descriptor table at position `slot->descriptor_idx`!
  // That means not (checking init_count && init) in here, but in r_create_texture_impl.
  slot->backend_rsrc = r_create_texture_impl(init, init_count, desc, slot->descriptor_idx);

  R_Handle result = {
    .idx = slot_idx,
    .gen = slot->gen,
  };
  return result;
}
