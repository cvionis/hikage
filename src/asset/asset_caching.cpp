// @Todo: deez nuts!
static String8
str8_cat(Arena *arena, String8 a, String8 b)
{
  String8 result = {};

  if (a.data && b.data) {
    U64 count = a.count + b.count;
    U8 *data = ArenaPushArray(arena, U8, count + 1);

    MemoryCopy(data, a.data, a.count);
    MemoryCopy(data + a.count, b.data, b.count);

    data[count] = 0;

    result.data = data;
    result.count = count;
  }

  return result;
}

// @Todo: Deez nuts!
static String8
dir_from_path(String8 path)
{
  String8 result = {};

  if (path.count > 0) {
    U64 dir_count = path.count;
    for (U64 idx = path.count-1; idx > 0; idx -= 1) {
      if (path.data[idx] == '/' || path.data[idx] == '\\') {
        break;
      }
      dir_count -= 1;
    }

    result.data = path.data;
    result.count = dir_count;
  }

  return result;
}

// @Todo: Yo mama!
static String8
filename_from_path(String8 path)
{
  String8 result = {};

  if (path.count > 0) {
    U64 filename_start = 0;

    for (U64 idx = path.count-1; idx > 0; idx -= 1) {
      if (path.data[idx] == '/' || path.data[idx] == '\\') {
        filename_start = idx + 1;
        break;
      }
    }
    result.data = path.data + filename_start;
    result.count = path.count - filename_start;
  }

  return result;
}

static String8
remove_extension_from_path(String8 path)
{
  String8 result = {};

  if (path.count > 0) {
    U64 new_count = path.count;
    for (U64 idx = path.count-1; idx > 0; idx -= 1) {
      new_count -= 1;
      if (path.data[idx] == '.') {
        break;
      }
    }

    result.data = path.data;
    result.count = new_count;
  }

  return result;
}

// @Todo: Replace ChatGPT's U32's with S32's.

static AC_Builder
ac_make(void)
{
  AC_Builder result = {};
  result.arena = arena_alloc(MiB(128));
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
  if (cgltf_res == cgltf_result_success) {
    cgltf_res = cgltf_load_buffers(&options, data, chr_from_str8(gltf_path));
    if (cgltf_res == cgltf_result_success) {
      cgltf_res = cgltf_validate(data);
    }
  }

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
A_Vertex_count_from_primitive(cgltf_primitive *prim)
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
        .vertex_count   = A_Vertex_count_from_primitive(&mesh->primitives[p]),
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

    U64 pos_prev = arena->pos;

    arena_set_align(arena, align);
    result = arena_push(arena, size);
    arena_set_align(arena, 1);

    U64 pos_curr = arena->pos;

    builder->size += (pos_curr - pos_prev);
  }

  return result;
}

static AC_BuildResult
ac_build_mesh_table(AC_Builder *builder, AC_PrimitiveArray prims, cgltf_data *gltf)
{
  U32 mesh_count = prims.count;
  U32 mesh_table_size = sizeof(AC_MeshEntry) * mesh_count;
  U32 mesh_table_offset = (U32)builder->size;
  AC_MeshEntry *mesh_table = (AC_MeshEntry *)ac_push(
    builder,
    mesh_table_size,
    1
  );

  for (U32 i = 0; i < mesh_count; ++i) {
    AC_Primitive *p = &prims.v[i];
    AC_MeshEntry *m = &mesh_table[i];

    *m = {
      .vertex_offset_bytes = 0, // filled during geometry build
      .vertex_count        = p->vertex_count,
      .vertex_stride       = A_VERTEX_STRIDE,
      .index_offset_bytes  = 0, // filled during geometry build
      .index_count         = p->index_count,
      .index_kind          = (p->vertex_count <= 65535) ? R_IndexKind_U16 : R_IndexKind_U32,
      .material_index      = (U32)(p->gltf_primitive->material - gltf->materials), // @Todo: Make sure material isn't null first.
    };
  }

  AC_BuildResult result = {
    .data = (void *)mesh_table,
    .offset = mesh_table_offset,
    .size = mesh_table_size,
    .count = mesh_count,
  };

  return result;
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
ac_emit_vertices(A_Vertex *dst, AC_Primitive *prim)
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
    A_Vertex *v = &dst[i];

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

static AC_BuildResult
ac_build_geometry_vertices(AC_Builder *builder, AC_PrimitiveArray prims, AC_MeshEntry *mesh_table)
{
  U32 section_offset = (U32)builder->size;
  section_offset = AlignPow2(section_offset, 256);
  ac_push(builder, section_offset - builder->size, 1);

  U32 section_size = 0;
  U32 entry_offset = 0;

  for (S32 i = 0; i < prims.count; ++i) {
    AC_Primitive *p = &prims.v[i];
    AC_MeshEntry *m = &mesh_table[i];

    m->vertex_offset_bytes = entry_offset;

    U32 vertices_size = sizeof(A_Vertex) * p->vertex_count;
    A_Vertex *vertices = (A_Vertex *)ac_push(
      builder,
      vertices_size,
      16
    );
    ac_emit_vertices(vertices, p);

    section_size += vertices_size;
    entry_offset += vertices_size;
  }

  AC_BuildResult result = {
    .data = 0,
    .offset = section_offset,
    .size = section_size,
    .count = 0,
  };

  return result;
}

static AC_BuildResult
ac_build_geometry_indices(AC_Builder *builder, AC_PrimitiveArray prims, AC_MeshEntry *mesh_table)
{
  U32 section_offset = (U32)builder->size;
  section_offset = AlignPow2(section_offset, 256);
  ac_push(builder, section_offset - builder->size, 1);

  U32 section_size = 0;
  U32 entry_offset = 0;

  for (S32 i = 0; i < prims.count; ++i) {
    AC_Primitive *p = &prims.v[i];
    AC_MeshEntry *m = &mesh_table[i];

    m->index_offset_bytes = entry_offset;

    U32 indices_size = 0;

    if (m->index_kind == R_IndexKind_U16) {
      indices_size = sizeof(U16) * p->index_count;
      U16 *indices = (U16 *)ac_push(
        builder,
        indices_size,
        16
      );
      ac_emit_indices_u16(indices, p);

      int x = 0;
      (void) x;
    }
    else {
      indices_size = sizeof(U32) * p->index_count;
      U32 *indices = (U32 *)ac_push(
        builder,
        indices_size,
        16
      );
      ac_emit_indices_u32(indices, p);
    }

    section_size += indices_size;
    entry_offset += indices_size;
  }

  AC_BuildResult result = {
    .data = 0,
    .offset = section_offset,
    .size = section_size,
    .count = 0,
  };

  return result;
}

static U32
ac_texture_index_from_view(cgltf_data *gltf, cgltf_texture_view *view)
{
  if (!view || !view->texture) {
    return AC_TEXTURE_NONE;
  }

  return (U32)(view->texture - gltf->textures);
}

static U32
ac_image_index_from_image(cgltf_data *gltf, cgltf_image *image)
{
  if (!image) {
    return (U32)-1; // @Note: Temp
  }

  return (U32)(image - gltf->images);
}

static AC_BuildResult
ac_build_material_table(AC_Builder *builder, cgltf_data *gltf)
{
  U32 mtl_count = (U32)gltf->materials_count;
  U32 mtl_table_size = sizeof(AC_MaterialEntry) * mtl_count;
  U32 mtl_table_offset = (U32)builder->size;
  AC_MaterialEntry *mtl_table = (AC_MaterialEntry *)ac_push(
    builder,
    mtl_table_size,
    1
  );

  for (U32 mat_idx = 0; mat_idx < mtl_count; mat_idx += 1) {
    cgltf_material *gltf_mat = &gltf->materials[mat_idx];
    AC_MaterialEntry *dst    = &mtl_table[mat_idx];

    // ---- Defaults ----
    dst->flags = A_MaterialFlag_None;

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
        U32 tex = ac_texture_index_from_view(gltf, &pbr->base_color_texture);
        dst->base_color_tex = tex;
        dst->flags |= A_MaterialFlag_BaseColor;

        U32 img = ac_image_index_from_image(
          gltf, pbr->base_color_texture.texture->image);
        builder->img_usages[img] = AC_ImageUsage_BaseColor;
      }

      if (pbr->metallic_roughness_texture.texture) {
        U32 tex = ac_texture_index_from_view(gltf, &pbr->metallic_roughness_texture);
        dst->metallic_roughness_tex = tex;
        dst->flags |= A_MaterialFlag_MetalRough;

        U32 img = ac_image_index_from_image(
          gltf, pbr->metallic_roughness_texture.texture->image);
        builder->img_usages[img] = AC_ImageUsage_MetalRough;
      }
    }

    // ---- Normal map ----
    if (gltf_mat->normal_texture.texture) {
      U32 tex = ac_texture_index_from_view(gltf, &gltf_mat->normal_texture);
      dst->normal_tex = tex;
      dst->flags |= A_MaterialFlag_Normal;

      U32 img = ac_image_index_from_image(
        gltf, gltf_mat->normal_texture.texture->image);
      builder->img_usages[img] = AC_ImageUsage_Normal;
    }

    // ---- Occlusion map ----
    if (gltf_mat->occlusion_texture.texture) {
      U32 tex = ac_texture_index_from_view(gltf, &gltf_mat->occlusion_texture);
      dst->occlusion_tex = tex;
      dst->flags |= A_MaterialFlag_Occlusion;

      U32 img = ac_image_index_from_image(
        gltf, gltf_mat->occlusion_texture.texture->image);
      builder->img_usages[img] = AC_ImageUsage_Occlusion;
    }

    // ---- Emissive ----
    dst->emissive.x = gltf_mat->emissive_factor[0];
    dst->emissive.y = gltf_mat->emissive_factor[1];
    dst->emissive.z = gltf_mat->emissive_factor[2];

    if (gltf_mat->emissive_texture.texture) {
      U32 tex = ac_texture_index_from_view(gltf, &gltf_mat->emissive_texture);
      dst->emissive_tex = tex;
      dst->flags |= A_MaterialFlag_Emissive;

      U32 img = ac_image_index_from_image(
        gltf, gltf_mat->emissive_texture.texture->image);
      builder->img_usages[img] = AC_ImageUsage_Emissive;
    }
  }

  AC_BuildResult result = {
    .data = (void *)mtl_table,
    .offset = mtl_table_offset,
    .size = mtl_table_size,
    .count = mtl_count,
  };

  return result;
}

static AC_BuildResult
ac_build_texture_table(AC_Builder *builder, cgltf_data *gltf)
{
  U32 tex_count = (U32)gltf->textures_count;
  U32 tex_table_size = sizeof(AC_TextureEntry) * tex_count;
  U32 tex_table_offset = (U32)builder->size;
  AC_TextureEntry *tex_table = (AC_TextureEntry *)ac_push(
    builder,
    tex_table_size,
    1
  );

  for (U32 tex_idx = 0; tex_idx < tex_count; tex_idx += 1) {
    tex_table[tex_idx].img_index = ac_image_index_from_image(gltf, gltf->textures[tex_idx].image);
  }

  AC_BuildResult result = {
    .data = (void *)tex_table,
    .offset = tex_table_offset,
    .size = tex_table_size,
    .count = tex_count,
  };

  return result;
}

struct AC_ImageMetadata {
  R_TextureFmt fmt;
  U32 width;
  U32 height;

  U32 mips_begin;
  U32 mip_count;

  U32 data_offset;
  U32 data_size;
};

static R_TextureFmt
ac_image_fmt_from_usage(U32 usage)
{
  // @Note: From low to high priority

  //A_ImageFormat result = A_ImageFormat_BC7;
  R_TextureFmt result = R_TextureFmt_BC3_UNORM;
  switch (usage) {
    case AC_ImageUsage_Emissive:   { result = R_TextureFmt_BC3_UNORM; }break;
    case AC_ImageUsage_MetalRough: { result = R_TextureFmt_BC4_UNORM; }break;
    case AC_ImageUsage_Occlusion:  { result = R_TextureFmt_BC4_UNORM; }break;
    case AC_ImageUsage_Normal:     { result = R_TextureFmt_BC5_UNORM; }break;
  }

  return result;
}

struct AC_ImageLoad {
  U8 *data;
  U64 size;
};

static AC_ImageLoad
ac_img_load(AC_Builder *builder, Arena *arena, cgltf_image *img)
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
    // @Todo: Need to save full path when loading model and build uri path here.
    String8 img_dir = dir_from_path(builder->model_path);
    String8 img_filename = str8((U8 *)img->uri, cstr_count(img->uri));
    String8 img_path = str8_cat(arena, img_dir, img_filename);
    String8 file_read = os_file_read(arena, img_path);
    if (file_read.count > 0) {
      out_data = ArenaPushArray(arena, U8, file_read.count);
      MemoryCopy(out_data, file_read.data, file_read.count);
      out_size = file_read.count;
    }
  }

  result.data = out_data;
  result.size = out_size;

  return result;
}

// @Note: This doesn't fill out any fields in the table's entries.
static AC_BuildResult
ac_build_image_table(AC_Builder *builder, cgltf_data *gltf)
{
  U32 img_count = (U32)gltf->images_count;
  U32 img_table_size = sizeof(AC_ImageEntry) * img_count;
  U32 img_table_offset = (U32)builder->size;
  AC_ImageEntry *img_table = (AC_ImageEntry *)ac_push(
    builder,
    img_table_size,
    1
  );

  AC_BuildResult result = {
    .data = (void *)img_table,
    .offset = img_table_offset,
    .size = img_table_size,
    .count = img_count,
  };

  return result;
}

static AC_BuildResult
ac_build_mip_table(AC_Builder *builder)
{
  // @Note: Have to populate most of the mip table inside ac_build_images() as that's the only place you have access to mip counts!

  U32 mip_table_offset = (U32)builder->size;
  AC_MipEntry *mip_table = (AC_MipEntry *)(builder->data + builder->size);

  AC_BuildResult result = {
    .data = mip_table, // @Note: Populated in ac_build_images()
    .offset = mip_table_offset,
    .size = 0, // @Note: Not used
    .count = 0, // @Note: Not used
  };

  return result;
}

static AC_BuildResult
ac_build_images(AC_Builder *builder, AC_ImageEntry *img_table, cgltf_data *gltf)
{
  U32 img_data_size = 0;

  // @Note: Temporary
  Arena *img_staging_arena = arena_alloc(MiB(128));
  arena_set_align(img_staging_arena, 256);

  // Preliminary work: Prepare compressed image metadata
  Arena *scratch = arena_get_scratch(0,0);

  // @Todo: This is retarded. Just fill img_table entries as you go thru each image and compress it...
  AC_ImageMetadata *img_metadata = ArenaPushArray(scratch, AC_ImageMetadata, gltf->images_count);
  for (U32 img_idx = 0; img_idx < gltf->images_count; img_idx += 1) {
    AC_ImageUsage img_usage = builder->img_usages[img_idx];
    img_metadata[img_idx].fmt = ac_image_fmt_from_usage(img_usage);
  }

  // Load, decode, build mips, compress image data

  U32 compressed_data_offset = 0; // From start of compressed image data, not the file.
  U32 running_mip_count = 0;

  // @Todo: Make sure to handle sRGB data correctly.
  for (U32 img_idx = 0; img_idx < gltf->images_count; img_idx += 1) {
    TempArena tmp = arena_temp_begin(scratch);

    cgltf_image *image = &gltf->images[img_idx];
    AC_ImageLoad img = ac_img_load(builder, tmp.arena, image);

    S32 img_width = 0, img_height = 0, img_channels = 0;
    U8 *img_data_decoded = stbi_load_from_memory(img.data, (S32)img.size, &img_width, &img_height, &img_channels, 4);
    if (img_data_decoded) {
      DirectX::Image src = {
        .width      = (size_t)img_width,
        .height     = (size_t)img_height,
        .format     = DXGI_FORMAT_R8G8B8A8_UNORM,
        .rowPitch   = (size_t)img_width * 4,
        .slicePitch = src.rowPitch * img_height,
        .pixels     = img_data_decoded,
      };

      // Generate a mip chain for the decoded image
      DirectX::ScratchImage mip_chain;
      HRESULT hr = DirectX::GenerateMipMaps(
        src,
        DirectX::TEX_FILTER_DEFAULT,
        0,
        mip_chain
      );

      stbi_image_free(img_data_decoded);

      if (SUCCEEDED(hr)) {
        // Compress zeh mips
        R_TextureFmt fmt = img_metadata[img_idx].fmt;
        DXGI_FORMAT dxgi_fmt = r_d3d12_fmt_from_texture_fmt(fmt);
        if (dxgi_fmt != DXGI_FORMAT_UNKNOWN) {
          DirectX::ScratchImage compressed;
          hr = DirectX::Compress(
            mip_chain.GetImages(),
            mip_chain.GetImageCount(),
            mip_chain.GetMetadata(),
            dxgi_fmt,
            DirectX::TEX_COMPRESS_DEFAULT | DirectX::TEX_COMPRESS_PARALLEL,
            1.0f,
            compressed
          );

          if (SUCCEEDED(hr)) {
            // Save image metadata
            const DirectX::TexMetadata &meta = compressed.GetMetadata();
            const DirectX::Image *images = compressed.GetImages();

            img_metadata[img_idx].mips_begin = running_mip_count;
            img_metadata[img_idx].mip_count  = (U32)meta.mipLevels;
            img_metadata[img_idx].width      = (U32)meta.width;
            img_metadata[img_idx].height     = (U32)meta.height;

            U32 compressed_size = 0;
            for (U32 mip_idx = 0; mip_idx < meta.mipLevels; mip_idx += 1) {
              compressed_size += (U32)images[mip_idx].slicePitch;
            }

            // Fill mip table entries for this image
            U32 mips_count = (U32)meta.mipLevels;
            U32 mip_img_offset = 0;
            running_mip_count += mips_count;

            for (U32 mip_idx = 0; mip_idx < mips_count; mip_idx += 1) {
              AC_MipEntry *mip = (AC_MipEntry *)ac_push(builder, sizeof(AC_MipEntry), 1);

              mip->width = (U32)images[mip_idx].width;
              mip->height = (U32)images[mip_idx].height;
              mip->row_pitch = (U32)images[mip_idx].rowPitch;
              mip->slice_pitch = (U32)images[mip_idx].slicePitch;
              mip->image_offset_bytes = mip_img_offset;

              mip_img_offset += (U32)images[mip_idx].slicePitch;
            }

            // Calculate aligned offset in output
            compressed_data_offset = AlignPow2(compressed_data_offset, 256);
            img_metadata[img_idx].data_offset = compressed_data_offset;
            img_metadata[img_idx].data_size = compressed_size;
            compressed_data_offset += compressed_size;
            img_data_size += compressed_size;

            // Copy compressed image data to output
            // @Note: Temporary
            U8 *compressed_mips = (U8 *)arena_push(img_staging_arena, compressed_size);

            U32 pos = 0;
            U32 image_count = (U32)compressed.GetImageCount();
            for (U32 mip_idx = 0; mip_idx < image_count; mip_idx += 1) {
              U32 copy_size = (U32)images[mip_idx].slicePitch;
              MemoryCopy(compressed_mips + pos, images[mip_idx].pixels, copy_size);
              pos += copy_size;
            }
          }
        }
      }
    }

    arena_temp_end(tmp);
  }

  U32 img_data_offset = AlignPow2(builder->size, 256);
  U8 *src = (U8 *)img_staging_arena + ARENA_HEADER_SIZE;
  U8 *dst = builder->data + img_data_offset;
  ac_push(builder, img_data_size, 256);
  MemoryCopy(dst, src, img_data_size);

  arena_release(img_staging_arena);

  // Fill img table entries (@Todo: Get rid of this stupid array and just directly copy inside the fucking loop...)
  for (U32 img_idx = 0; img_idx < gltf->images_count; img_idx += 1) {
    AC_ImageMetadata *metadata = &img_metadata[img_idx];
    img_table[img_idx].format = metadata->fmt;
    img_table[img_idx].width = metadata->width;
    img_table[img_idx].height = metadata->height;
    img_table[img_idx].mips_begin = metadata->mips_begin;
    img_table[img_idx].mip_count = metadata->mip_count;
    img_table[img_idx].data_offset_bytes = metadata->data_offset;
    img_table[img_idx].data_size_bytes = metadata->data_size;
  }

  AC_BuildResult result = {
    .data = 0,
    .offset = img_data_offset,
    .size = img_data_size,
    .count = running_mip_count, // @Note: Gross temporary solution
  };
  return result;
}

// @Note: This would eventually just take the name of the model and form
// the full path of the cached file.
static AC_Blob
ac_load_model_blob_cached(Arena *arena, String8 name)
{
  AC_Blob result = {};

  TempArena scratch = arena_scratch_begin(&arena, 1);
  {
    String8 cached_model_dir = S8("R:/KageEngine/assets/cache/models/"); // @Note: Temporary
    String8 cache_path;
    cache_path = str8_cat(scratch.arena, cached_model_dir, name);
    cache_path = str8_cat(scratch.arena, cache_path, S8(".mb"));

    String8 file_read = os_file_read(scratch.arena, cache_path);
    if (file_read.count > 0) {
      U8 *data = ArenaPushArray(arena, U8, file_read.count);
      MemoryCopy(data, file_read.data, file_read.count);

      result.data = data;
      result.size = file_read.count;
    }
  }
  arena_scratch_end(scratch);
  return result;
}

// @Todo: Test with broken or unconvential gltf files
static AC_Blob
ac_load_model_blob_gltf(Arena *arena, AC_Builder *builder, String8 gltf_path)
{
  builder->model_path = gltf_path;

  AC_Blob res = {};

  cgltf_data *gltf = ac_parse_gltf(gltf_path);
  if (gltf) {
    Arena *scratch = arena_get_scratch(0,0);

    AC_Header *hdr = (AC_Header *)ac_push(builder, sizeof(AC_Header), 1);
    char magic[] = "DEEZ";
    MemoryCopy(&hdr->magic, magic, sizeof(U32));
    hdr->version = AC_VERSION;

    AC_BuildResult build;
    AC_MeshEntry *mesh_table;
    AC_MaterialEntry *mtl_table;
    AC_ImageEntry *img_table;
    AC_MipEntry *mip_table;

    AC_PrimitiveArray primitives = ac_flatten_gltf(scratch, gltf);
    build = ac_build_mesh_table(builder, primitives, gltf);
    hdr->mesh_count = build.count;
    hdr->mesh_table_off = build.offset;
    mesh_table = (AC_MeshEntry *)build.data;

    build = ac_build_geometry_vertices(builder, primitives, mesh_table);
    hdr->vb_bytes_off = build.offset;
    hdr->vb_bytes_size = build.size;

    build = ac_build_geometry_indices(builder, primitives, mesh_table);
    hdr->ib_bytes_off = build.offset;
    hdr->ib_bytes_size = build.size;

    build = ac_build_material_table(builder, gltf);
    hdr->material_count = build.count;
    hdr->material_table_off = build.offset;
    mtl_table = (AC_MaterialEntry *)build.data;

    build = ac_build_texture_table(builder, gltf);
    hdr->texture_count = build.count;
    hdr->texture_table_off = build.offset;

    build = ac_build_image_table(builder, gltf);
    hdr->image_count = build.count;
    hdr->image_table_off = build.offset;
    img_table = (AC_ImageEntry *)build.data;

    build = ac_build_mip_table(builder);
    hdr->mip_table_off = build.offset;
    mip_table = (AC_MipEntry *)build.data;

    build = ac_build_images(builder, img_table, gltf);
    hdr->image_bytes_off = build.offset;
    hdr->image_bytes_size = build.size;
    hdr->mip_count = build.count; // @Note: Temporary gross solution.

    ac_free_gltf(gltf);

    // @Note: Temporary
    U64 blob_size = builder->size;
    U8 *blob_data = ArenaPushArray(arena, U8, blob_size);
    MemoryCopy(blob_data, builder->data, blob_size);

    res = {
      .data = blob_data,
      .size = blob_size,
    };

    S32 i_count = mesh_table[0].index_count;
    R_IndexKind i_kind = mesh_table[0].index_kind;

    if (i_kind == R_IndexKind_U16) {
      U16 *indices = (U16 *)(blob_data + hdr->ib_bytes_off);
      for (S32 i = 0; i < i_count; i += 3) {
        Assert(indices[i+0] != indices[i+1]);
        Assert(indices[i+1] != indices[i+2]);
        Assert(indices[i+0] != indices[i+2]);
      }
    }
    else {
      U16 *indices = (U16 *)(blob_data + hdr->ib_bytes_off);
      for (S32 i = 0; i < i_count; i += 3) {
        Assert(indices[i+0] != indices[i+1]);
        Assert(indices[i+1] != indices[i+2]);
        Assert(indices[i+0] != indices[i+2]);
      }
    }
  }

  return res;
}

static void
ac_cache_model_blob(AC_Builder *builder, AC_Blob blob)
{
  TempArena scratch = arena_scratch_begin(0,0);

  String8 model_filename = filename_from_path(builder->model_path);
  String8 model_filename_stripped = remove_extension_from_path(model_filename);
  String8 cached_model_dir = S8("R:/KageEngine/assets/cache/models/"); // @Note: Temporary
  String8 cached_model_ext = S8(".mb");

  String8 cached_model_path = str8_cat(scratch.arena, cached_model_dir, model_filename_stripped);
  cached_model_path = str8_cat(scratch.arena, cached_model_path, cached_model_ext);
  // @Todo: os_file_write().
  FILE *f = fopen(chr_from_str8(cached_model_path), "wb+");
  if (f) {
    fwrite(blob.data, blob.size, 1, f);
    fclose(f);
  }

  arena_scratch_end(scratch);
}
