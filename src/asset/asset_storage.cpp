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

  if (ctx) {
    TempArena scratch = arena_scratch_begin(0,0);

    AC_Blob blob = ac_load_model_blob_cached(scratch.arena, name);
    if (!blob.size) {
      AC_Builder ac = ac_make();

      String8 gltf_path;
      gltf_path = str8_cat(scratch.arena, ctx->root_path, name);
      gltf_path = str8_cat(scratch.arena, gltf_path, S8("/"));
      gltf_path = str8_cat(scratch.arena, gltf_path, name);
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
      // @Note: Temporary. Should just assume index kind is constant per-model
      // (doesn't vary per submesh) and store it in the header once rather than in each mesh entry.
      R_IndexKind model_index_kind = mesh_table[0].index_kind;

      U32 mtl_count = hdr->material_count;
      U32 mtl_table_off = hdr->material_table_off;
      auto *mtl_table  = (AC_MaterialEntry *)(data + mtl_table_off);

      U32 tex_count = hdr->texture_count;
      U32 tex_table_off = hdr->texture_table_off;
      auto *tex_table  = (AC_TextureEntry *)(data + tex_table_off);

      U32 mip_table_off = hdr->mip_table_off;
      auto *mip_table  = (AC_MipEntry *)(data + mip_table_off);

      U32 img_table_off = hdr->image_table_off;
      auto *img_table  = (AC_ImageEntry *)(data + img_table_off);

      U32 vb_data_off = hdr->vb_bytes_off;
      U32 vb_data_size = hdr->vb_bytes_size;
      U8 *vb_data = data + vb_data_off;

      U32 ib_data_off = hdr->ib_bytes_off;
      U32 ib_data_size = hdr->ib_bytes_size;
      U8 *ib_data = data + ib_data_off;

      U32 img_data_off = hdr->image_bytes_off;

      //
      // Create model asset and any child assets
      //

      Asset *asset = &ctx->models[ctx->models_count];
      asset->name = name;
      asset->kind = AssetKind_Model;

      // @Todo: Create vertex and index buffer for model, return handles
      // @Todo: Consider storing index kind (U32/U16)
      Model *model = &asset->v.model;
      model->meshes_count = mesh_count;
      model->meshes = ArenaPushArray(ctx->arena, Mesh, mesh_count);

      // Vertex buffer (shared by submeshes)
      {
        R_BufferInitData init = {
          .data = vb_data,
        };
        R_BufferDesc desc = {
          .size = vb_data_size,
          .stride_bytes = sizeof(A_Vertex),
          .usage = R_BufferUsage_Vertex,
          .memory = R_BufferMemory_Default,
        };

        model->vertex_buffer = r_create_buffer(init, desc);
      }

      // Index buffer (shared by submeshes)
      {
        R_BufferInitData init = {
          .data = ib_data,
        };
        R_BufferDesc desc = {
          .size = ib_data_size,
          .index_kind = model_index_kind,
          .usage = R_BufferUsage_Index,
          .memory = R_BufferMemory_Default,
        };

        model->index_buffer = r_create_buffer(init, desc);
      }

      // Per-submesh slices into vertex/index buffers
      for (U32 mesh_idx = 0; mesh_idx < mesh_count; mesh_idx += 1) {
        Mesh *dst = &model->meshes[mesh_idx];
        AC_MeshEntry *src = &mesh_table[mesh_idx];

        dst->vb_off = src->vertex_offset_bytes;
        dst->vb_count = src->vertex_count;
        dst->ib_off = src->index_offset_bytes;
        dst->ib_count = src->index_count;
        dst->material = src->material_index;
      }

      for (U32 tex_idx = 0; tex_idx < tex_count; tex_idx += 1) {
        Texture *dst = &ctx->textures[tex_idx].v.texture;
        AC_TextureEntry *src = &tex_table[tex_idx];

        U32 img_idx = src->img_index;
        AC_ImageEntry *img = &img_table[img_idx];
        dst->fmt = img->format;
        dst->width = img->width;
        dst->height = img->height;
        dst->mip_count = img->mip_count;

        R_TextureDesc desc = {
          .width = (S32)img->width,
          .height = (S32)img->height,
          .depth = 1,
          .mips_count = (S32)img->mip_count,
          .fmt = img->format,
          .usage = R_TextureUsage_Sampled, // @Note: Currently not used.
          .kind = R_TextureKind_2D,
        };

        U32 mips_end = img->mips_begin + img->mip_count;

        R_TextureInitData *init = ArenaPushArray(scratch.arena, R_TextureInitData, img->mip_count);
        U32 init_idx = 0;
        for (U32 mip_idx = img->mips_begin; mip_idx < mips_end; mip_idx += 1) {
          AC_MipEntry *mip = &mip_table[mip_idx];
          U32 img_base_off = img_data_off + img->data_offset_bytes;
          U32 mip_off = img_base_off + mip->image_offset_bytes;
          U8 *mip_data = data + mip_off; // @Todo: Verify

          init[init_idx] = {
            .data = mip_data,
            .row_pitch = (S32)mip->row_pitch,
            .slice_pitch = (S32)mip->slice_pitch,
          };
          init_idx += 1;
        }

        dst->tex = r_create_texture(init, img->mip_count, desc);
        ctx->textures_count += 1;
      }

      // @Todo: I want to store materials on the GPU so I can expose them to the shader
      // as a structured buffer that I can index into inside the shader using the material id,
      //  which would be a per-draw/per-model constant.
      for (U32 mtl_idx = 0; mtl_idx < mtl_count; mtl_idx += 1) {
        Material *dst = &ctx->materials[mtl_idx].v.material;
        AC_MaterialEntry *src = &mtl_table[mtl_idx];

        dst->base_color = src->base_color;
        dst->emissive = src->emissive;
        dst->metallic = src->metallic;
        dst->roughness = src->roughness;

        dst->tex_base_color = src->base_color_tex;
        dst->tex_normal = src->normal_tex;
        dst->tex_metal_rough = src->metallic_roughness_tex;
        dst->tex_occlusion = src->occlusion_tex;
        dst->tex_emissive = src->emissive_tex;

        ctx->materials_count += 1;
      }

      ctx->models_count += 1;
    }

    arena_scratch_end(scratch);
  }

  return result;
}
