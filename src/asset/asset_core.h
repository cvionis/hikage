#define ASSETS_MAX 512

struct R_Handle {
  S32 dummy;
};

struct Mesh {
  S32 vb_off;
  S32 vb_count;
  S32 ib_off;
  S32 ib_count;

  S32 material;
};

struct Material {
  R_Handle tex_base;
  R_Handle text_metal;
  R_Handle tex_rough;
  // ... etc.
  // ... numeric values.
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
};

struct AssetHandle {
  S32 idx;
  S32 gen;
};

struct Asset {
  String8 path;
  AssetStatus status;
  AssetKind kind;
  union {
    Model model;
    Material material;
  }v;
};

struct AssetContext {
  Arena *arena;

  String8 root_path;

  // @Todo: Each entry should be a slot with a list in case hash collisions occur.
  Asset assets[ASSETS_MAX];
  S32 assets_count;
};

// Internal helpers
static B32 asset_cached(AssetContext *ctx, String8 path);
static AssetHandle alloc_asset_handle(AssetContext *ctx, AssetKind kind);

// Public API
static AssetContext assets_make(void);
static void assets_release(AssetContext *ctx);
static void assets_set_root_path(AssetContext *ctx, String8 path);
static AssetHandle assets_load_model(AssetContext *ctx, String8 path);
