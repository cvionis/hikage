// Packed CPU-side format for models

#define AC_MAGIC 0x4C444F4D  // 'MODL'
#define AC_VERSION 1

/*
 * TODO:
 *  Align vb_bytes_off and ib_bytes_off to 256 bytes.
 *  Align each texture payload start to 256 bytes (simplifies upload staging).
 *  Align tables to 16 bytes.
*/

/*
 * Format:
    [Header]

    [Mesh table]
    [Material table]
    [Texture table]

    [Vertex buffer data]
    [Index buffer data]
    [Texture payloads]
 */

enum AC_TextureFmt {
  AC_TextureFmt_BC1_UNORM,
  AC_TextureFmt_BC3_UNORM,
  AC_TextureFmt_BC4_UNORM,
  AC_TextureFmt_BC5_UNORM,
  AC_TextureFmt_BC6_UNORM,
  AC_TextureFmt_BC7_UNORM,
};

enum AC_IndexKind {
  AC_IndexKind_U16,
  AC_IndexKind_U32,
};

struct AC_Vertex {
  V3F32 position;
  V3F32 normal;
  V4F32 tangent;
  V2F32 uv;
};

#define AC_VERTEX_STRIDE sizeof(AC_Vertex)

struct AC_Header {
  U32 magic;            // MODL_MAGIC
  U32 version;          // MODL_VERSION

  U32 flags;            // @Note: Reserved for later.

  U32 mesh_count;
  U32 material_count;
  U32 texture_count;

  // Offsets (bytes) from start of blob
  U32 mesh_table_off;
  U32 material_table_off;
  U32 texture_table_off;

  U32 vb_bytes_off;     // contiguous vertex buffer bytes
  U32 vb_bytes_size;

  U32 ib_bytes_off;     // contiguous index buffer bytes (typically u32 or u16)
  U32 ib_bytes_size;

  U32 tex_bytes_off;    // contiguous compressed texture payload bytes (all mips, all textures)
  U32 tex_bytes_size;
};

struct AC_MeshEntry {
  U32 vertex_offset_bytes;   // into vb section
  U32 vertex_count;
  U32 vertex_stride;         // bytes per vertex (fixed for blob version or stored here)

  U32 index_offset_bytes;    // into ib section
  U32 index_count;
  AC_IndexKind index_kind;

  U32 material_index;        // into material table
};

struct AC_MaterialEntry {
  V4F32 base_color;
  V3F32 emissive;

  F32 metallic;
  F32 roughness;

  U32 base_color_tex;         // index into texture table or MODL_TEX_NONE
  U32 normal_tex;
  U32 metallic_roughness_tex;
  U32 occlusion_tex;
  U32 emissive_tex;

  U32 sampler_id;             // @Todo: determine if you want to use this; maybe just include all sampler info here (convert gltf enum -> yours)
};

struct AC_TextureEntry {
  AC_TextureFmt format;
  U32 width;
  U32 height;
  U32 mip_count;

  U32 data_offset_bytes;      // into tex_bytes section
  U32 data_size_bytes;        // total bytes for all mips
};

struct AC_Builder {
  Arena *arena;
  void *data;
  U64 size;
};

struct AC_Blob {
  void *data;
  U64 size;
};

struct AC_Primitive {
  cgltf_primitive *gltf_primitive;
  cgltf_material  *gltf_material;

  Mat4x4 world_transform;

  U32 vertex_count;
  U32 index_count;
};

// Flattened array of the primitives (submeshes) in a gltf file, with
// any scene node transforms that will later be baked into their vertex data.
struct AC_PrimitiveArray {
  AC_Primitive *v;
  S32 count;
};

// Internal helpers

// @Todo: ac_append_() helpers

// Public API
static AC_Builder ac_make(void);
static void ac_release(AC_Builder *builder);
static void ac_set_align(AC_Builder *builder, U64 align);
static AC_Blob ac_blob_from_gltf(AC_Builder *builder, String8 gltf_path);
