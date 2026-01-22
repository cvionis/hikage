//
// Helpers
//

#if 0
static DXGI_FORMAT dxgi_format_from_r_format(R_TextureFmt);

static void d3d_flags_from_r_usage(R_TextureUsage);
#endif

static S32
r_alloc_descriptor_idx(R_ResourceKind kind)
{
  // @Note: Implementation is backend-specific.
  // @Todo: Implement
  return 0;
}

// @Todo: Next step after initial sketch is to create the three main descriptor heaps you can allocate from
// @Todo: Make sure you store texture descriptors in such a way that you can expose to shader as a large table
// of textures that you can index into within the shader.

static void *
r_create_texture_impl(R_TextureData *init, R_TextureDesc desc)
{
  return 0;
}
