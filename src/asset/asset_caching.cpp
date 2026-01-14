static AC_Builder
ac_make(void)
{
  AC_Builder result = {};
  result.arena = arena_alloc_default();
  return result;
}

static void
ac_release(AC_Builder *builder)
{
  if (builder) {
    arena_release(builder->arena);
    MemoryZeroStruct(builder);
  }
}

static void
ac_set_align(AC_Builder *builder, U64 align)
{
  if (builder) {
    arena_set_align(builder->arena, align);
  }
}

static AC_Blob
ac_blob_from_gltf(AC_Builder *builder, String8 gltf_path)
{
}
