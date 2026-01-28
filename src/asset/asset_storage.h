#pragma once

#define ASSETS_MODELS_MAX    64
#define ASSETS_MATERIALS_MAX 128
#define ASSETS_TEXTURES_MAX  512

struct Mesh {
  S32 vb_off;
  S32 vb_count;
  S32 ib_off;
  S32 ib_count;

  S32 material;
};

struct Texture {
  R_Handle tex;
  R_TextureFmt fmt;
  U32 width;
  U32 height;
  U32 mip_count;
};

struct Material {
  V4F32 base_color;
  V3F32 emissive;

  F32 metallic;
  F32 roughness;

  U32 flags;

  S32 tex_base_color;
  S32 tex_normal;
  S32 tex_metal_rough;
  S32 tex_occlusion;
  S32 tex_emissive;
};

struct Model {
  R_Handle vertex_buffer;
  R_Handle index_buffer;

  S32 meshes_count;
  Mesh *meshes;
};

enum AssetStatus {
  AssetStatus_None,
  AssetStatus_Requested,
  AssetStatus_Loading,
  AssetStatus_Failed,
  AssetStatus_Completed,
};

enum AssetKind {
  AssetKind_Model,
  AssetKind_Material,
  AssetKind_Texture,
};

struct AssetHandle {
  S32 idx;
  S32 gen;
};

struct Asset {
  String8 name; // @Todo: Fill this out
  AssetStatus status;
  AssetKind kind;
  union {
    Model model;
    Material material;
    Texture texture;
  }v;
};

struct AssetContext {
  Arena *arena;
  String8 root_path;

  Asset models[ASSETS_MODELS_MAX];
  Asset materials[ASSETS_MATERIALS_MAX];
  Asset textures[ASSETS_TEXTURES_MAX];

  S32 models_count;
  S32 materials_count;
  S32 textures_count;
};

// Internal helpers
static B32 asset_cached(AssetContext *ctx, String8 path);
static AssetHandle alloc_asset_handle(AssetContext *ctx, AssetKind kind);

// Public API
static AssetContext assets_make(void);
static void assets_release(AssetContext *ctx);
static void assets_set_root_path(AssetContext *ctx, String8 path);
static AssetHandle assets_load_model(AssetContext *ctx, String8 name);
