#pragma once

// ================================================== //
// Creation/deletion API for persistent GPU resources //
// ================================================== //

//
// CPU-side resource storage
//

#define R_RESOURCE_SLOTS_MAX 1024

struct R_Handle {
  S32 idx;
  S32 gen;
  S64 fence_value;
};

struct R_CreateResource {
  S64 fence_value;
  void *backend;
};

enum R_ResourceKind {
  R_ResourceKind_Texture,
  R_ResourceKind_Buffer,
};

struct R_ResourceSlot {
  R_ResourceKind kind;

  S32 gen;
  B32 alive;

  S32 descriptor_idx;
  S64 fence_value;
  void *backend_rsrc;
};

struct R_ResourceTable {
  R_ResourceSlot slots[R_RESOURCE_SLOTS_MAX];
  S32 count;
};

global R_ResourceTable r_resource_table; // @Todo: Store in r_ctx

//
// Textures
//

enum R_TextureFmt {
  // Uncompressed
  R_TextureFmt_RGBA8_UNORM,
  R_TextureFmt_RGBA16_FLOAT,

  // Block-compressed
  R_TextureFmt_BC1_UNORM,
  R_TextureFmt_BC3_UNORM,
  R_TextureFmt_BC4_UNORM,
  R_TextureFmt_BC5_UNORM,
  R_TextureFmt_BC7_UNORM,

  // Depth
  R_TextureFmt_D32_FLOAT,
};

enum R_TextureUsage {
  R_TextureUsage_Default      = 0,
  R_TextureUsage_Sampled      = (1 << 0),
  R_TextureUsage_RenderTarget = (1 << 1),
  R_TextureUsage_DepthStencil = (1 << 2),
  R_TextureUsage_Unordered    = (1 << 3),
};

enum R_TextureKind {
  R_TextureKind_2D,
  R_TextureKind_2D_Array,
  R_TextureKind_3D,
  R_TextureKind_Cube,
  R_TextureKind_Cube_Array,
};

struct R_TextureInitData {
  void *data;
  S32 row_pitch;
  S32 slice_pitch;
};

/* @Note:
   Will need to enforce these:
   * (kind == 2D) --> depth == 1
   * (kind == Cube) --> depth = 6
   * (kind == Cube_Array) --> depth % 6 == 0
   * (kind == 3D) --> depth > 1
 */
struct R_TextureDesc {
  S32 width;
  S32 height;
  S32 depth; // depth for 3D, array size for array textures
  S32 mips_count;

  R_TextureFmt fmt;
  R_TextureUsage usage;
  R_TextureKind kind;
};

// @Todo: init_count might be redundant as desc already contains `mips_count`.
static R_Handle r_create_texture(R_TextureInitData *init, S32 init_count, R_TextureDesc desc);

//
// Buffers
//

struct R_BufferInitData {
  void *data;
};

enum R_BufferMemoryKind {
  R_BufferMemory_Default,
  R_BufferMemory_Upload,
  R_BufferMemory_Readback,
};

enum R_BufferUsage {
  R_BufferUsage_Vertex,
  R_BufferUsage_Index,
  R_BufferUsage_Structured,
  R_BufferUsage_Raw,
  R_BufferUsage_Unordered,
};

enum R_IndexKind {
  R_IndexKind_U16,
  R_IndexKind_U32,
};

struct R_BufferDesc {
  S64 size;
  S32 stride_bytes; // used for vertex buffers

  R_IndexKind index_kind; // used for index buffers
  R_BufferUsage usage;
  R_BufferMemoryKind memory;
};

static R_Handle r_create_buffer(R_BufferInitData init, R_BufferDesc desc);
