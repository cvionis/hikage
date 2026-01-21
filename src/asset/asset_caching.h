// Packed CPU-side format for models

#define AC_MAGIC 0x4C444F4D  // 'MODL'
#define AC_VERSION 1

#define AC_TEXTURE_NONE 0xFFFFFFFFu

/*
 * Format:
    [Header]

    [Mesh table]
    [Vertex buffer data]
    [Index buffer data]

    [Material table]
    [Texture table]
    [Image data]
 */

 // @Todo: Distinguish between sided/unsigned variants
enum AC_ImageFormat {
  AC_ImageFormat_BC1,
  AC_ImageFormat_BC3,
  AC_ImageFormat_BC4,
  AC_ImageFormat_BC5,
  AC_ImageFormat_BC6H,
  AC_ImageFormat_BC7,
};

enum AC_IndexKind {
  AC_IndexKind_U16,
  AC_IndexKind_U32,
};

enum AC_MaterialFlags {
  AC_MaterialFlag_None = 0,
  AC_MaterialFlag_BaseColor = (1 << 0),
  AC_MaterialFlag_Normal = (1 << 1),
  AC_MaterialFlag_MetalRough = (1 << 2),
  AC_MaterialFlag_Occlusion = (1 << 3),
  AC_MaterialFlag_Emissive = (1 << 4),
};

struct AC_Vertex {
  V3F32 position;
  V3F32 normal;
  V4F32 tangent;
  V2F32 uv;
};

#define AC_VERTEX_STRIDE sizeof(AC_Vertex)

struct AC_Header {
  U32 magic;
  U32 version;

  U32 flags;            // @Note: Reserved for later.

  U32 mesh_count;
  U32 material_count;
  U32 texture_count;
  U32 image_count;

  // Offsets (bytes) from start of blob
  U32 mesh_table_off;
  U32 material_table_off;
  U32 texture_table_off;
  U32 image_table_off;

  U32 vb_bytes_off;     // contiguous vertex buffer bytes
  U32 vb_bytes_size;

  U32 ib_bytes_off;     // contiguous index buffer bytes (typically u32 or u16)
  U32 ib_bytes_size;

  U32 image_bytes_off;    // contiguous compressed image bytes (all mips in order)
  U32 image_bytes_size;
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

  U32 flags;

  U32 base_color_tex;         // index into texture table or AC_TEXTURE_NONE
  U32 normal_tex;
  U32 metallic_roughness_tex;
  U32 occlusion_tex;
  U32 emissive_tex;
};

struct AC_TextureEntry {
  U32 img_index;
  // @Todo: Store sampler state.
};

struct AC_ImageEntry {
  AC_ImageFormat format;
  U32 width;
  U32 height;
  U32 mip_count;

  U32 data_offset_bytes;      // into img_bytes section
  U32 data_size_bytes;        // total bytes for all mips
};

enum AC_ImageUsage {
  AC_ImageUsage_None,
  AC_ImageUsage_BaseColor,
  AC_ImageUsage_Normal,
  AC_ImageUsage_MetalRough,
  AC_ImageUsage_Occlusion,
  AC_ImageUsage_Emissive,
  AC_ImageUsage_COUNT,
};

#define AC_IMAGES_MAX 512
struct AC_Builder {
  Arena *arena;
  U8 *data;
  U64 size;
  String8 model_path;
  AC_ImageUsage img_usages[512];
};

// @Todo: AC_ModelBlob, or AC_PackedModelData
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

struct AC_BuildResult {
  void *data; // entry data; put in bottom with other optional field, `count`.
  U32 offset;
  U32 size;
  U32 count; // entry count, for table builders
};

// Public API
static AC_Builder ac_make(void);
static void ac_release(AC_Builder *builder);
static void ac_set_align(AC_Builder *builder, U64 align);
static AC_Blob ac_blob_from_gltf(AC_Builder *builder, String8 gltf_path);
