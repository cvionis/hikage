#pragma once

// ================================================== //
// Creation/deletion API for persistent GPU resources //
// ================================================== //

//
// CPU-side resource storage
//

#define R_RESOURCE_SLOTS_MAX 1024

enum R_ResourceKind {
  R_ResourceKind_Texture,
  R_ResourceKind_Buffer,
};

// @Todo: Will probably need "alive" state
struct R_ResourceSlot {
  R_ResourceKind kind;
  S32 descriptor_idx; // The specific heap we index into depends on .kind
  S32 gen;

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

struct R_Handle {
  S32 idx;
  S32 gen;
};

struct R_TextureData {
  void *data;
  S32 row_pitch;
  S32 slice_pitch;
};

enum R_TextureFmt {
  // Uncompressed
  R_TextureFmt_RGBA8_UNORM,
  R_TextureFmt_RGBA16_FLOAT,

  // Block-compressed
  R_TextureFmt_BC1_UNORM,
  R_TextureFmt_BC3_UNORM,
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

static R_Handle r_create_texture(R_TextureData *init, R_TextureDesc desc);

//
// Buffers
//

struct R_BufferData {
  void *data;
};

enum R_BufferMemory {
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

struct R_BufferDesc {
  S64 size;
  R_BufferUsage usage;
  R_BufferMemory memory;
};

static R_Handle r_create_buffer(R_BufferData *init, R_BufferDesc desc);
