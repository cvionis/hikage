//
// Helpers
//

#if 0
static DXGI_FORMAT dxgi_format_from_r_format(R_TextureFmt);

static void d3d_flags_from_r_usage(R_TextureUsage);
#endif

static S32
r_alloc_texture_descriptor_idx(void)
{
  // @Note: Implementation is backend-specific.
  // @Todo: Implement
  return 0;
}

static void *
r_create_texture_impl(R_TextureInitData *init, S32 init_count, R_TextureDesc desc, S32 descriptor_idx)
{
  return 0;
}
