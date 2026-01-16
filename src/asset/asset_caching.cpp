// @Todo: Replace ChatGPT's U32's with S32's.

static AC_Builder
ac_make(void)
{
  AC_Builder result = {};
  result.arena = arena_alloc_default();
  result.data = (U8 *)result.arena + ARENA_HEADER_SIZE;
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

static cgltf_data *
ac_parse_gltf(String8 gltf_path)
{
  cgltf_data *data = 0;

  cgltf_result cgltf_res = {};
  cgltf_options options = {};
  cgltf_res = cgltf_parse_file(&options, chr_from_str8(gltf_path), &data);
  cgltf_res = cgltf_load_buffers(&options, data, chr_from_str8(gltf_path));
  cgltf_res = cgltf_validate(data);

  return data;
}

static void
ac_free_gltf(cgltf_data *data)
{
  cgltf_free(data);
}

static Mat4x4
ac_node_local_matrix(cgltf_node *node)
{
  Mat4x4 res = {};

  if (node->has_matrix) {
    // @Todo: Make sure this casting produces the expected results. Maybe use static_cast?
    res = m4x4_transpose(*(Mat4x4 *)&node->matrix);
  }
  // @Todo: Test this path for correctness
  else {
    V3F32 t = node->has_translation
      ? v3f32(node->translation[0], node->translation[1], node->translation[2])
      : v3f32(0,0,0);

    Quat r = node->has_rotation
      ? quat_from_f32(node->rotation[0], node->rotation[1],
            node->rotation[2], node->rotation[3])
      : quat_identity();

    V3F32 s = node->has_scale
      ? v3f32(node->scale[0], node->scale[1], node->scale[2])
      : v3f32(1,1,1);

    res = m4x4_mul(
            translation_m4x4(t),
            m4x4_mul(
              m4x4_from_quat(r),
              scale_m4x4(s)
            )
         );
    }

  return res;
}

static U32
ac_vertex_count_from_primitive(cgltf_primitive *prim)
{
  U32 result = 0;
  for (U32 i = 0; i < prim->attributes_count; ++i) {
    if (prim->attributes[i].type == cgltf_attribute_type_position) {
      result = (U32)prim->attributes[i].data->count;
      break;;
    }
  }
  return result;
}

static U32
ac_index_count_from_primitive(cgltf_primitive *prim)
{
  return prim->indices ? (U32)prim->indices->count : 0;
}

static void
ac_collect_primitives_from_node(Arena *arena, cgltf_node *node, Mat4x4 parent_world, AC_PrimitiveArray *out)
{
  Mat4x4 local = ac_node_local_matrix(node);
  Mat4x4 world = m4x4_mul(parent_world, local);

  if (node->mesh) {
    cgltf_mesh *mesh = node->mesh;
    for (U32 p = 0; p < mesh->primitives_count; ++p) {
      AC_Primitive *dst = &out->v[out->count++];
      *dst = {
        .gltf_primitive = &mesh->primitives[p],
        .gltf_material  = mesh->primitives[p].material,
        .world_transform = world,
        .vertex_count   = ac_vertex_count_from_primitive(&mesh->primitives[p]),
        .index_count    = ac_index_count_from_primitive(&mesh->primitives[p]),
      };
    }
  }
  for (U32 i = 0; i < node->children_count; ++i) {
    ac_collect_primitives_from_node(arena, node->children[i], world, out);
  }
}

// @Todo: ChatGPT: i -> idx; remove ternaries; ++ -> += 1;
// @Todo: Test with more files.
static AC_PrimitiveArray
ac_flatten_gltf(Arena *arena, cgltf_data *gltf)
{
  AC_PrimitiveArray result = {};

  // Conservative pre-pass: count primitives to size the array once
  S32 primitive_count = 0;
  for (U32 i = 0; i < gltf->meshes_count; ++i) {
    primitive_count += (S32)gltf->meshes[i].primitives_count;
  }
  result.v = ArenaPushArray(arena, AC_Primitive, primitive_count);
  result.count = 0;

  Mat4x4 identity = m4x4_identity();

  cgltf_scene *scene =
    gltf->scene ? gltf->scene :
    (gltf->scenes_count ? &gltf->scenes[0] : 0);

  if (scene) {
    for (U32 i = 0; i < scene->nodes_count; ++i) {
      ac_collect_primitives_from_node(arena, scene->nodes[i], identity, &result);
    }
  } else {
    // No scene: traverse all root nodes
    for (U32 i = 0; i < gltf->nodes_count; ++i) {
      if (!gltf->nodes[i].parent) {
        ac_collect_primitives_from_node(arena, &gltf->nodes[i], identity, &result);
      }
    }
  }

  return result;
}

static void *
ac_push(AC_Builder *builder, U64 size, U64 align)
{
  void *result = 0;

  if (builder) {
    Arena *arena = builder->arena;
    arena_set_align(arena, align);
    result = arena_push(arena, size);
    arena_set_align(arena, 0);

    builder->size += size;
  }

  return result;
}

static AC_MeshEntry *
ac_build_mesh_table(AC_Builder *builder, AC_PrimitiveArray prims, cgltf_data *gltf)
{
  AC_MeshEntry *mesh_table = (AC_MeshEntry *)ac_push(
    builder,
    sizeof(AC_MeshEntry) * prims.count,
    1
  );

  for (S32 i = 0; i < prims.count; ++i) {
    AC_Primitive *p = &prims.v[i];
    AC_MeshEntry *m = &mesh_table[i];

    *m = {
      .vertex_offset_bytes = 0, // filled during geometry build
      .vertex_count        = p->vertex_count,
      .vertex_stride       = AC_VERTEX_STRIDE,
      .index_offset_bytes  = 0, // filled during geometry build
      .index_count         = p->index_count,
      .index_kind          = (p->vertex_count <= 65535)
                               ? AC_IndexKind_U16
                               : AC_IndexKind_U32,
      .material_index      = (U32)(p->gltf_primitive->material - gltf->materials), // @Todo: Make sure material isn't null first.
    };
  }
  return mesh_table;
}

static V3F32
ac_transform_point(Mat4x4 m, V3F32 p)
{
  V4F32 v = { p.x, p.y, p.z, 1.0f };
  V4F32 r = v4f32_transform(m, v);
  return { r.x, r.y, r.z };
}

static V3F32
ac_transform_vector(Mat4x4 m, V3F32 v)
{
  V4F32 vv = { v.x, v.y, v.z, 0.0f };
  V4F32 r  = v4f32_transform(m, vv);
  return { r.x, r.y, r.z };
}

static void
ac_emit_vertices(AC_Vertex *dst, AC_Primitive *prim)
{
  cgltf_primitive *p = prim->gltf_primitive;

  //---------------------------------------------------------------------------
  // Locate attribute accessors
  //---------------------------------------------------------------------------

  cgltf_accessor *acc_pos = 0;
  cgltf_accessor *acc_nrm = 0;
  cgltf_accessor *acc_tan = 0;
  cgltf_accessor *acc_uv  = 0;

  for (U32 i = 0; i < p->attributes_count; ++i) {
    cgltf_attribute *a = &p->attributes[i];
    switch (a->type) {
      case cgltf_attribute_type_position:
        acc_pos = a->data;
        break;
      case cgltf_attribute_type_normal:
        acc_nrm = a->data;
        break;
      case cgltf_attribute_type_tangent:
        acc_tan = a->data;
        break;
      case cgltf_attribute_type_texcoord:
        if (a->index == 0) acc_uv = a->data;
        break;
    }
  }

  Assert(acc_pos != 0);
  U32 count = (U32)acc_pos->count;

  //---------------------------------------------------------------------------
  // Precompute normal matrix: inverse-transpose of world
  //---------------------------------------------------------------------------

  Mat4x4 world_mat = prim->world_transform;
  Mat4x4 normal_mat = m4x4_transpose(m4x4_inverse(world_mat));

  //---------------------------------------------------------------------------
  // Emit vertices
  //---------------------------------------------------------------------------

  for (U32 i = 0; i < count; ++i) {
    AC_Vertex *v = &dst[i];

    // --- position ---
    {
      F32 p3[3];
      cgltf_accessor_read_float(acc_pos, i, p3, 3);
      V3F32 p0 = { p3[0], p3[1], p3[2] };
      v->position = ac_transform_point(world_mat, p0);
    }

    // --- normal ---
    if (acc_nrm) {
      F32 n3[3];
      cgltf_accessor_read_float(acc_nrm, i, n3, 3);
      V3F32 n0 = { n3[0], n3[1], n3[2] };
      v->normal = v3f32_normalize(ac_transform_vector(normal_mat, n0));
    } else {
      v->normal = {0, 0, 1};
    }

    // --- tangent ---
    if (acc_tan) {
      F32 t4[4];
      cgltf_accessor_read_float(acc_tan, i, t4, 4);
      V3F32 t0 = { t4[0], t4[1], t4[2] };
      V3F32 t1 = v3f32_normalize(ac_transform_vector(normal_mat, t0));
      v->tangent = { t1.x, t1.y, t1.z, t4[3] };
    } else {
      v->tangent = {1, 0, 0, 1};
    }

    // --- UV ---
    if (acc_uv) {
      F32 uv2[2];
      cgltf_accessor_read_float(acc_uv, i, uv2, 2);
      v->uv = { uv2[0], uv2[1] };
    } else {
      v->uv = {0, 0};
    }
  }
}

static B32
ac_should_flip_winding(Mat4x4 world)
{
  // determinant of upper-left 3×3
  F32 det =
    world.e[0][0] * (world.e[1][1]*world.e[2][2] - world.e[1][2]*world.e[2][1]) -
    world.e[0][1] * (world.e[1][0]*world.e[2][2] - world.e[1][2]*world.e[2][0]) +
    world.e[0][2] * (world.e[1][0]*world.e[2][1] - world.e[1][1]*world.e[2][0]);

  return det < 0.0f;
}

static void
ac_emit_indices_u32_impl(U32 *dst, cgltf_accessor *acc, U32 index_count, B32 flip_winding)
{
  if (acc) {
    // Indexed primitive
    for (U32 i = 0; i < index_count; i += 3) {
      U32 i0 = (U32)cgltf_accessor_read_index(acc, i + 0);
      U32 i1 = (U32)cgltf_accessor_read_index(acc, i + 1);
      U32 i2 = (U32)cgltf_accessor_read_index(acc, i + 2);

      if (flip_winding) {
        dst[i + 0] = i0;
        dst[i + 1] = i2;
        dst[i + 2] = i1;
      } else {
        dst[i + 0] = i0;
        dst[i + 1] = i1;
        dst[i + 2] = i2;
      }
    }
  } else {
    // Non-indexed primitive: implicit 0..N-1
    for (U32 i = 0; i < index_count; i += 3) {
      U32 i0 = i + 0;
      U32 i1 = i + 1;
      U32 i2 = i + 2;

      if (flip_winding) {
        dst[i + 0] = i0;
        dst[i + 1] = i2;
        dst[i + 2] = i1;
      } else {
        dst[i + 0] = i0;
        dst[i + 1] = i1;
        dst[i + 2] = i2;
      }
    }
  }
}

static void
ac_emit_indices_u16(U16 *dst, AC_Primitive *prim)
{
  cgltf_primitive *p = prim->gltf_primitive;
  cgltf_accessor *acc = p->indices;

  U32 index_count = acc ? (U32)acc->count : prim->vertex_count;
  B32 flip = ac_should_flip_winding(prim->world_transform);

  // Emit into temporary U32 buffer, then narrow
  for (U32 i = 0; i < index_count; i += 3) {
    U32 i0, i1, i2;

    if (acc) {
      i0 = (U32)cgltf_accessor_read_index(acc, i + 0);
      i1 = (U32)cgltf_accessor_read_index(acc, i + 1);
      i2 = (U32)cgltf_accessor_read_index(acc, i + 2);
    } else {
      i0 = i + 0;
      i1 = i + 1;
      i2 = i + 2;
    }

    if (flip) {
      dst[i + 0] = (U16)i0;
      dst[i + 1] = (U16)i2;
      dst[i + 2] = (U16)i1;
    } else {
      dst[i + 0] = (U16)i0;
      dst[i + 1] = (U16)i1;
      dst[i + 2] = (U16)i2;
    }
  }
}

static void
ac_emit_indices_u32(U32 *dst, AC_Primitive *prim)
{
  cgltf_primitive *p = prim->gltf_primitive;
  cgltf_accessor *acc = p->indices;

  U32 index_count = acc ? (U32)acc->count : prim->vertex_count;
  B32 flip = ac_should_flip_winding(prim->world_transform);

  ac_emit_indices_u32_impl(dst, acc, index_count, flip);
}

static void
ac_build_geometry_payload(AC_Builder *builder, AC_PrimitiveArray prims, AC_MeshEntry *mesh_table)
{
  for (S32 i = 0; i < prims.count; ++i) {
    AC_Primitive *p = &prims.v[i];
    AC_MeshEntry *m = &mesh_table[i];

    // Vertices
    m->vertex_offset_bytes = (U32)builder->size;
    AC_Vertex *vertices = (AC_Vertex *)ac_push(
      builder,
      sizeof(AC_Vertex) * p->vertex_count,
      16
    );
    ac_emit_vertices(vertices, p);

    // Indices
    m->index_offset_bytes = (U32)builder->size;
    if (m->index_kind == AC_IndexKind_U16) {
      U16 *indices = (U16 *)ac_push(
        builder,
        sizeof(U16) * p->index_count,
        16
      );
      ac_emit_indices_u16(indices, p);
    }
    else {
      U32 *indices = (U32 *)ac_push(
        builder,
        sizeof(U32) * p->index_count,
        16
      );
       ac_emit_indices_u32(indices, p);
    }
  }
}

static U32
ac_texture_index_from_view(cgltf_data *gltf, cgltf_texture_view *view)
{
  if (!view || !view->texture) {
    return AC_TEXTURE_NONE;
  }

  return (U32)(view->texture - gltf->textures);
}

static AC_MaterialEntry *
ac_build_material_table(AC_Builder *builder, cgltf_data *gltf)
{
  AC_MaterialEntry *mtl_table = (AC_MaterialEntry *)ac_push(
    builder,
    sizeof(AC_MaterialEntry) * gltf->materials_count,
    1
  );

  for (U32 mat_idx = 0; mat_idx < gltf->materials_count; ++mat_idx) {
    cgltf_material *gltf_mat = &gltf->materials[mat_idx];
    AC_MaterialEntry *dst    = &mtl_table[mat_idx];

    // ---- Defaults ----
    dst->flags = AC_MaterialFlag_None;

    dst->base_color.x = 1.0f;
    dst->base_color.y = 1.0f;
    dst->base_color.z = 1.0f;
    dst->base_color.w = 1.0f;

    dst->emissive.x = 0.0f;
    dst->emissive.y = 0.0f;
    dst->emissive.z = 0.0f;

    dst->metallic  = 1.0f;
    dst->roughness = 1.0f;

    dst->base_color_tex         = AC_TEXTURE_NONE;
    dst->normal_tex             = AC_TEXTURE_NONE;
    dst->metallic_roughness_tex = AC_TEXTURE_NONE;
    dst->occlusion_tex          = AC_TEXTURE_NONE;
    dst->emissive_tex           = AC_TEXTURE_NONE;

    // ---- PBR metallic-roughness ----
    if (gltf_mat->has_pbr_metallic_roughness) {
      cgltf_pbr_metallic_roughness *pbr =
        &gltf_mat->pbr_metallic_roughness;

      dst->base_color.x = pbr->base_color_factor[0];
      dst->base_color.y = pbr->base_color_factor[1];
      dst->base_color.z = pbr->base_color_factor[2];
      dst->base_color.w = pbr->base_color_factor[3];

      dst->metallic  = pbr->metallic_factor;
      dst->roughness = pbr->roughness_factor;

      if (pbr->base_color_texture.texture) {
        dst->base_color_tex =
          ac_texture_index_from_view(gltf,
            &pbr->base_color_texture);
        dst->flags |= AC_MaterialFlag_BaseColor;
      }

      if (pbr->metallic_roughness_texture.texture) {
        dst->metallic_roughness_tex =
          ac_texture_index_from_view(gltf,
            &pbr->metallic_roughness_texture);
        dst->flags |= AC_MaterialFlag_MetalRough;
      }
    }

    // ---- Normal map ----
    if (gltf_mat->normal_texture.texture) {
      dst->normal_tex =
        ac_texture_index_from_view(gltf,
          &gltf_mat->normal_texture);
      dst->flags |= AC_MaterialFlag_Normal;
    }

    // ---- Occlusion map ----
    if (gltf_mat->occlusion_texture.texture) {
      dst->occlusion_tex =
        ac_texture_index_from_view(gltf,
          &gltf_mat->occlusion_texture);
      dst->flags |= AC_MaterialFlag_Occlusion;
    }

    // ---- Emissive ----
    dst->emissive.x = gltf_mat->emissive_factor[0];
    dst->emissive.y = gltf_mat->emissive_factor[1];
    dst->emissive.z = gltf_mat->emissive_factor[2];

    if (gltf_mat->emissive_texture.texture) {
      dst->emissive_tex =
        ac_texture_index_from_view(gltf,
          &gltf_mat->emissive_texture);
      dst->flags |= AC_MaterialFlag_Emissive;
    }
  }

  return mtl_table;
}

static U32
ac_image_index_from_image(cgltf_data *gltf, cgltf_image *image)
{
  if (!image) {
    return (U32)-1; // @Note: Temp
  }

  return (U32)(image - gltf->images);
}

static void
ac_build_texture_table(AC_Builder *builder, cgltf_data *gltf)
{
  AC_TextureEntry *tex_table = (AC_TextureEntry *)ac_push(
    builder,
    sizeof(AC_TextureEntry) * gltf->textures_count,
    1
  );

  for (S32 tex_idx = 0; tex_idx < gltf->textures_count; tex_idx += 1) {
    tex_table[tex_idx].img_index = ac_image_index_from_image(gltf, gltf->textures[tex_idx].image);
  }
}

static void
mark_image_usage_using_mtl_texture(cgltf_data *gltf, U32 *image_flags, U32 mtl_flags, U32 tex_idx)
{
  if (tex_idx != AC_TEXTURE_NONE) {
    cgltf_texture *tex = &gltf->textures[tex_idx];
    U32 img_idx = ac_image_index_from_image(gltf, tex->image);
    if (img_idx != AC_TEXTURE_NONE) { // @Todo: no
      image_flags[img_idx] |= mtl_flags;
    }
  }
}

static void
ac_image_usage_from_materials(U32 *image_flags, cgltf_data *gltf, AC_MaterialEntry *mtl_table)
{
  for (U32 i = 0; i < gltf->images_count; ++i) {
    image_flags[i] = AC_MaterialFlag_None;
  }

  for (U32 mtl_idx = 0; mtl_idx < gltf->materials_count; mtl_idx += 1) {
    AC_MaterialEntry *m = &mtl_table[mtl_idx];

    mark_image_usage_using_mtl_texture(gltf, image_flags, m->flags, m->base_color_tex);
    mark_image_usage_using_mtl_texture(gltf, image_flags, m->flags, m->normal_tex);
    mark_image_usage_using_mtl_texture(gltf, image_flags, m->flags, m->metallic_roughness_tex);
    mark_image_usage_using_mtl_texture(gltf, image_flags, m->flags, m->occlusion_tex);
    mark_image_usage_using_mtl_texture(gltf, image_flags, m->flags, m->emissive_tex);
  }
}

struct AC_CompressedImageHeader {
  AC_ImageFormat fmt;
  U32 width;
  U32 height;
  U32 mip_count;

  U32 data_offset;
  U32 data_size;
};

static AC_ImageFormat
ac_image_fmt_from_usage(U32 usage)
{
  // From low to high priority

  // Default: base color / albedo
  AC_ImageFormat res = AC_ImageFormat_BC7;

  // Emissive often benefits from alpha (and is usually LDR)
  if (usage & AC_MaterialFlag_Emissive) {
    res = AC_ImageFormat_BC3;
  }

  // Packed scalar data (R, or RG masks)
  if (usage & (AC_MaterialFlag_MetalRough |
               AC_MaterialFlag_Occlusion)) {
    res = AC_ImageFormat_BC4;
  }

  if (usage & AC_MaterialFlag_Normal) {
    res = AC_ImageFormat_BC5;
  }

  return res;
}

struct AC_ImageLoad {
  U8 *data;
  U64 size;
};

static AC_ImageLoad
ac_img_load(Arena *arena, cgltf_image *img)
{
  AC_ImageLoad result = {};

  U8 *out_data = 0;
  U64 out_size = 0;

  if (img->buffer_view) {
    cgltf_buffer_view *view = img->buffer_view;
    cgltf_buffer *buffer = view->buffer;

    out_data = ArenaPushArray(arena, U8, view->size);
    MemoryCopy(out_data, buffer, view->size);
    out_size = (U64)view->size;
  }

  else if (img->uri) {
    TempArena tmp = arena_temp_begin(arena);
    {
      String8 img_path = str8((U8 *)img->uri, 256); // @Note: Length isn't even used in file_read so who cares.
      // @Todo: Need to save full path when loading model and build uri path here.
      String8 file_read = os_file_read(tmp.arena, S8("../assets/models/BoxTextured/CesiumLogoFlat.png"));
      if (file_read.count > 0) {
        out_data = ArenaPushArray(arena, U8, file_read.count);
        MemoryCopy(out_data, file_read.data, file_read.count);
        out_size = file_read.count;
      }
    }
    arena_temp_end(tmp);
  }

  result.data = out_data;
  result.size = out_size;

  return result;
}

static void
ac_build_image_table_and_payload(AC_Builder *builder, cgltf_data *gltf, AC_MaterialEntry *mtl_table)
{
  AC_ImageEntry *img_table = (AC_ImageEntry *)ac_push(
    builder,
    sizeof(AC_ImageEntry) * gltf->images_count,
    1
  );

  // * Decide compression format for each image
  // * Use that to create a set of compressed images, and an array of image metadata
  //   - For each gltf image: load into memory, decode into raw pixel data using stbi,
  //                          gen mipmaps, feed result into a compressor, return compressed data
  //

  Arena *scratch = arena_get_scratch(0,0);

  // Preliminary work: Prepare compressed image metadata

  U32 *img_usage_flags = ArenaPushArray(scratch, U32, gltf->images_count);
  ac_image_usage_from_materials(img_usage_flags, gltf, mtl_table);

  AC_CompressedImageHeader *img_headers = ArenaPushArray(scratch, AC_CompressedImageHeader, gltf->images_count);
  for (U32 img_idx = 0; img_idx < gltf->images_count; img_idx += 1) {
    img_headers[img_idx].fmt = ac_image_fmt_from_usage(img_usage_flags[img_idx]);
  }

  // Load, decode, build mips, compress image data

  for (U32 img_idx = 0; img_idx < gltf->images_count; img_idx += 1) {
    cgltf_image *image = &gltf->images[img_idx];
    AC_ImageLoad img = ac_img_load(scratch, image);

    S32 img_width = 0, img_height = 0, img_channels = 0;
    U8 *img_data_decoded = stbi_load_from_memory(img.data, (S32)img.size, &img_width, &img_height, &img_channels, 4);
    if (img_data_decoded) {
      int x;
      (void)x;

      #if 0
      S32 mip_count = 0;
      U32 fmt = img_headers[img_idx].fmt;
      AC_ImageCompress comp = ac_img_compress_and_push(builder->arena, img_data_decoded, fmt);

      AC_ImageEntry *img_entry = &img_table[img_idx];
      img_entry->fmt = fmt;
      img_entry->width = img_width; // @Todo: Not sure whether to put compressed or uncompressed sizes.
      img_entry->height = img_height;
      img_entry->mip_count = mip_count;
      img_entry->data_offset_bytes = comp.off;
      img_entry->data_offset_bytes = comp.size;
      #endif
    }
  }
}

// @Todo: Test with gltf files with several materials, textures, images
static AC_Blob
ac_blob_from_gltf(AC_Builder *builder, String8 gltf_path)
{
  AC_Blob res = {};

  cgltf_data *gltf = ac_parse_gltf(gltf_path);
  if (gltf) {
    TempArena scratch = arena_scratch_begin(0,0);
    AC_PrimitiveArray primitives = ac_flatten_gltf(scratch.arena, gltf);

    // @Todo: Build header placeholder, then fill out within each statics (pass header to them).

    // @Todo: Probably don't even need this shitty "AC_Builder" abstraction. Just incrementally push to array, pass that pointer
    // to each build static. E.g. ac_build_material_table should write out to a passed AC_MaterialEntry array allocated from the builder
    // arena.

    AC_MeshEntry *mesh_table = ac_build_mesh_table(builder, primitives, gltf);
    ac_build_geometry_payload(builder, primitives, mesh_table);

    AC_MaterialEntry *mtl_table = ac_build_material_table(builder, gltf);
    // @Todo: Copy image indices from gltf using ptr arithmetic; worry about copy sampling state later.
    ac_build_texture_table(builder, gltf);
    // @Todo: Compress images, storing each result's metadeta in one array of metadata structs and pushing the actual image data onto
    // a separate arena. Then use the former to populate image table entries; finally, copy compressed image data onto builder arena.

    ac_build_image_table_and_payload(builder, gltf, mtl_table);

    arena_scratch_end(scratch);
    ac_free_gltf(gltf);

    res = {
      .data = builder->data,
      .size = builder->size,
    };
  }

  return res;
}
