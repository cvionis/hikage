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
  R_ResourceKind_Pipeline,
  R_ResourceKind_Texture,
  R_ResourceKind_Buffer,
};

struct R_ResourceSlot {
  R_ResourceKind kind;

  S32 gen;
  B32 alive;

  S32 descriptor_idx; // @Note: Optional
  S64 fence_value; // @Note: need a value that indicates a fence is ready by default (e.g. 0)
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

// @Todo: init_count might be redundant as desc already contains `mips_count` (perhaps this makes the latter redundant instead)
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

//
// Pipelines
//

// @Todo: These should probably be elsewhere (render_core.h?)

enum R_VertexInputClass {
  R_VertexInputClass_PerVertex,
  R_VertexInputClass_PerInstance,
};

struct R_InputElement {
  String8 semantic_name;
  S32 semantic_index;

  R_TextureFmt format;
  S32 input_slot;
  S32 byte_offset;

  R_VertexInputClass input_class;
  S32 instance_step_rate;
};

struct R_Layout {
  R_InputElement elements[16];
  S32 elements_count;
};

enum R_FillMode {
  R_FillMode_Solid,
  R_FillMode_Wireframe,
};

enum R_CullMode {
  R_CullMode_None,
  R_CullMode_Front,
  R_CullMode_Back,
};

struct R_RasterizerState {
  R_FillMode fill_mode;
  R_CullMode cull_mode;
  B32 front_ccw;

  S32 depth_bias;
  F32 depth_bias_clamp;
  F32 slope_scaled_depth_bias;

  B32 depth_clip_enable;
  B32 multisample_enable;
};

enum R_CompareOp {
  R_CompareOp_Never,
  R_CompareOp_Less,
  R_CompareOp_Equal,
  R_CompareOp_LessEqual,
  R_CompareOp_Greater,
  R_CompareOp_NotEqual,
  R_CompareOp_GreaterEqual,
  R_CompareOp_Always,
};

enum R_StencilOp {
  R_StencilOp_Keep,
  R_StencilOp_Zero,
  R_StencilOp_Replace,
  R_StencilOp_IncClamp,
  R_StencilOp_DecClamp,
  R_StencilOp_Invert,
  R_StencilOp_IncWrap,
  R_StencilOp_DecWrap,
};

struct R_StencilFaceState {
  R_StencilOp fail_op;
  R_StencilOp depth_fail_op;
  R_StencilOp pass_op;
  R_CompareOp compare_op;
};

struct R_DepthStencilState {
  B32 depth_enable;
  B32 depth_write_enable;
  R_CompareOp depth_compare;

  B32 stencil_enable;
  U8 stencil_read_mask;
  U8 stencil_write_mask;

  R_StencilFaceState front_face;
  R_StencilFaceState back_face;
};

enum R_BlendFactor {
  R_BlendFactor_Zero,
  R_BlendFactor_One,
  R_BlendFactor_SrcColor,
  R_BlendFactor_InvSrcColor,
  R_BlendFactor_SrcAlpha,
  R_BlendFactor_InvSrcAlpha,
  R_BlendFactor_DestAlpha,
  R_BlendFactor_InvDestAlpha,
  R_BlendFactor_DestColor,
  R_BlendFactor_InvDestColor,
};

enum R_BlendOp {
  R_BlendOp_Add,
  R_BlendOp_Subtract,
  R_BlendOp_RevSubtract,
  R_BlendOp_Min,
  R_BlendOp_Max,
};

struct R_RenderTargetBlendState {
  B32 blend_enable;

  R_BlendFactor src_color;
  R_BlendFactor dst_color;
  R_BlendOp color_op;

  R_BlendFactor src_alpha;
  R_BlendFactor dst_alpha;
  R_BlendOp alpha_op;

  U8 write_mask;
};

struct R_BlendState {
  B32 alpha_to_coverage_enable;
  B32 independent_blend_enable;

  R_RenderTargetBlendState targets[8];
};

enum R_TopologyKind {
  R_TopologyKind_Triangle,
  R_TopologyKind_Line,
  R_TopologyKind_Point,
};

struct R_PipelineDesc {
  // @Todo: Use String8, convert
  LPCWSTR vs_path;
  LPCWSTR ps_path;

  R_Layout input_layout;

  R_RasterizerState raster;
  R_DepthStencilState depth_stencil;
  R_BlendState blend;
  R_TopologyKind topology;

  R_TextureFmt rt_formats[8];
  S32 rt_count;

  R_TextureFmt depth_format;
  S32 sample_count; // MSAA
};

static R_Handle r_create_pipeline(R_PipelineDesc desc);
