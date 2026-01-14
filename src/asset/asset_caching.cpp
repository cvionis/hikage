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
  if (node->has_matrix) {
    // @Todo: Implement
    //return m4x4_from_column_major(node->matrix);
  }

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

  // @Todo: Don't pass pointers to m4x4 operations
  return {};
  #if 0
  m4x4_mul(
           translation_m4x4(t),
           m4x4_mul(
             m4x4_from_quat(r),
             scale_m4x4(s)));
  #endif
}

static U32
ac_vertex_count_from_primitive(cgltf_primitive *prim)
{
  for (U32 i = 0; i < prim->attributes_count; ++i) {
    if (prim->attributes[i].type == cgltf_attribute_type_position) {
      return (U32)prim->attributes[i].data->count;
    }
  }
  return 0;
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

      AC_Primitive *dst = ArenaPushStruct(arena, AC_Primitive);
      out->v[out->count++] = *dst;

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

function AC_PrimitiveArray
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

static AC_Blob
ac_blob_from_gltf(AC_Builder *builder, String8 gltf_path)
{
  AC_Blob res = {};

  cgltf_data *gltf = ac_parse_gltf(gltf_path);
  if (gltf) {
    // @Todo: Build header placeholder, fill out as you go along using return values of ac_build_ helpers.
    AC_PrimitiveArray primitives = ac_flatten_gltf(builder->arena, gltf);
    ac_free_gltf(gltf);

    #if 0
    // @Todo: Order
    ac_build_mesh_table(builder, primitives);
    ac_build_material_texture_tables(builder, meshes);
    ac_build_geometry_payload(builder);
    ac_build_texture_payload(builder);
    #endif

    res = {
      .data = builder->data,
      .size = builder->size,
    };
  }

  return res;
}
