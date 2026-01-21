static AssetContext
assets_make(void)
{
  AssetContext result = {};
  result.arena = arena_alloc_default();

  return result;
}

static void
assets_release(AssetContext *ctx)
{
  arena_release(ctx->arena);
  MemoryZeroStruct(ctx);
}

static void
assets_set_root_path(AssetContext *ctx, String8 path)
{
  if (ctx) {
    ctx->root_path = path;
  }
}

static AssetHandle
assets_load_model(AssetContext *ctx, String8 name)
{
  AssetHandle result = {};

  // @Todo: Might want to pass an arena onto which to allocate the blob data, or give each blob its own arena.
  // If you call ac_release before you're completely done using the blob, the blob's memory is gonezo -- something
  // not immediately made clear in the API.
  // Don't even need this ridiculous, overengineered "AC_Builder" API probably.

  if (ctx) {
    TempArena scratch = arena_scratch_begin(0,0);

    AC_Blob blob = ac_load_model_blob_cached(scratch.arena, name);
    if (!blob.size) {
      AC_Builder ac = ac_make();

      String8 gltf_path;
      gltf_path = str8_cat(scratch.arena, ctx->root_path, name);
      gltf_path = str8_cat(scratch.arena, gltf_path, S8(".gltf"));

      blob = ac_load_model_blob_gltf(scratch.arena, &ac, gltf_path);
      if (blob.size > 0) {
        ac_cache_model_blob(&ac, blob);
      }

      ac_release(&ac);
    }

    if (blob.size > 0) {
      U8 *data = (U8 *)blob.data;

      AC_Header *hdr = (AC_Header *)data;

      U32 mesh_table_off = hdr->mesh_table_off;
      U32 mesh_count = hdr->mesh_count;
      auto *mesh_table = (AC_MeshEntry *)(data + mesh_table_off);

      U32 mtl_count = hdr->material_count;
      U32 mtl_table_off = hdr->material_table_off;
      auto *mtl_table  = (AC_MaterialEntry *)(data + mtl_table_off);

      U32 tex_count = hdr->texture_count;
      U32 tex_table_off = hdr->texture_table_off;
      auto *tex_table  = (AC_TextureEntry *)(data + tex_table_off);

      U32 img_count = hdr->image_count;
      U32 img_table_off = hdr->image_table_off;
      auto *img_table  = (AC_ImageEntry *)(data + img_table_off);

      U32 vb_data_off = hdr->vb_bytes_off;
      U32 vb_data_size = hdr->vb_bytes_size;
      U8 *vb_data = data + vb_data_off;

      U32 ib_data_off = hdr->ib_bytes_off;
      U32 ib_data_size = hdr->ib_bytes_size;
      U8 *ib_data = data + ib_data_off;

      U32 img_data_off = hdr->image_bytes_off;
      U32 img_data_size = hdr->image_bytes_size;
      U8 *img_data = data + img_data_off;

      // @Todo: Allocate slot for asset's metadata and handles in asset table;
      // Create actual GPU resources and upload data to GPU buffers.
      // Will have to create materials separately; not sure if I should store those in a separate
      // table or in the asset table. The latter is really the only thing that makes sense
      // in the context of this function.

      Asset *asset = &ctx->models[ctx->models_count];
      asset->name = name;
      asset->kind = AssetKind_Model;

      // @Todo: Create vertex and index buffer for model, return handles
      // @Todo: Consider storing index kind (U32/U16)
      Model *model = &asset->v.model;
      model->meshes_count = mesh_count;
      model->meshes = ArenaPushArray(ctx->arena, Mesh, mesh_count);
      for (U32 mesh_idx = 0; mesh_idx < mesh_count; mesh_idx += 1) {
        Mesh *dst = &model->meshes[mesh_idx];
        AC_MeshEntry *src = &mesh_table[mesh_idx];
        dst->vb_off = src->vertex_offset_bytes;
        dst->vb_count = src->vertex_count;
        dst->ib_off = src->index_offset_bytes;
        dst->ib_count = src->index_count;
        dst->material_index = src->material_index;
      }

      for (U32 tex_idx = 0; tex_idx < tex_count; tex_idx += 1) {
        Texture *dst = &ctx->textures[tex_idx].v.texture;
        AC_TextureEntry *src = &tex_table[tex_idx];

        U32 img_idx = src->img_index;
        AC_ImageEntry *img = &img_table[img_idx];

        //AC_ImageFormat format;
        dst->width = img->width;
        dst->height = img->height;
        dst->mip_count = img->mip_count;

        // @Todo:
        // U32 img_data_off = ...
        // U32 img_data_size = ...
        // U8 *data = img_data + img_data_off;
        // dst->tex = r_create_texture(data, ...);

        ctx->textures_count += 1;
      }

      // @Todo: Create texture for each entry in the texture table (new asset kind for storing texture metadata!)
      // Maybe each material can store an index into the array of texture assets, which themselves have render handles!
      for (U32 mtl_idx = 0; mtl_idx < mtl_count; mtl_idx += 1) {
        Material *dst = &ctx->materials[mtl_idx].v.material;
        AC_MaterialEntry *src = &mtl_table[mtl_idx];

        dst->base_color = src->base_color;
        dst->emissive = src->emissive;
        dst->metallic = src->metallic;
        dst->roughness = src->roughness;

        ctx->materials_count += 1;
      }

      ctx->models_count += 1;
    }

    arena_scratch_end(scratch);
  }

  return result;
}
