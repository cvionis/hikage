#pragma once

// ================================================== //
// Creation/deletion API for persistent GPU resources //
// ================================================== //

//
// CPU-side resource storage
//

#define R_VIEW_SLOTS_MAX     1024
#define R_RESOURCE_SLOTS_MAX 1024

struct R_Handle {
  S32 idx;
  S32 gen;
};

enum R_Format {
  R_Format_Invalid,

  // 8-bit normalized color
  R_Format_R8_UNorm,
  R_Format_R8G8_UNorm,
  R_Format_R8G8B8A8_UNorm,
  R_Format_R8G8B8A8_UNorm_Srgb,

  // 16-bit / 32-bit float color (HDR / G-buffer)
  R_Format_R16_Float,
  R_Format_R16G16_Float,
  R_Format_R16G16B16A16_Float,
  R_Format_R32_Float,
  R_Format_R32G32_Float,
  R_Format_R32G32B32_Float,
  R_Format_R32G32B32A32_Float,

  // Packed / special
  R_Format_R11G11B10_Float,     // HDR lighting buffers
  R_Format_R10G10B10A2_UNorm,   // optional G-buffer / lighting

  // Block-compressed (BCn)
  R_Format_BC1_UNorm,
  R_Format_BC1_UNorm_Srgb,
  R_Format_BC3_UNorm,
  R_Format_BC3_UNorm_Srgb,
  R_Format_BC4_UNorm,
  R_Format_BC5_UNorm,
  R_Format_BC7_UNorm,
  R_Format_BC7_UNorm_Srgb,

  // Depth / Stencil
  R_Format_D32_Float,
  R_Format_D24_UNorm_S8_UInt,

  // Typeless
  R_Format_R32_Typeless,
};

enum R_ResourceKind {
  R_ResourceKind_None,
  R_ResourceKind_Pipeline,
  R_ResourceKind_Texture,
  R_ResourceKind_Buffer,
};

enum R_ResourceState {
  R_ResourceState_Invalid,

  R_ResourceState_Common,

  R_ResourceState_RenderTarget,
  R_ResourceState_DepthWrite,
  R_ResourceState_DepthRead,

  R_ResourceState_ShaderRead,
  R_ResourceState_ShaderReadWrite,

  R_ResourceState_CopySrc,
  R_ResourceState_CopyDst,

  R_ResourceState_Present,
};

struct R_ResourceTransition {
  R_Handle rsrc;
  R_ResourceState state_before;
  R_ResourceState state_after;
};

struct R_CreateResource {
  R_ResourceState state;
  S32 srv_idx = -1; // @Todo: Remove these from this struct
  S32 rtv_idx = -1;
  S32 dsv_idx = -1;
  S64 fence_value; // @Todo: Remove from this struct
  void *backend;
};

struct R_ResourceSlot {
  R_ResourceKind kind;
  R_ResourceState state; // @Note: Not used by all resource types (e.g. pipelines)

  S32 gen;
  B32 alive; // @Todo: Make sure this is set properly.

  R_Format fmt; // @Note: Not sure if this is really an intrinsic property of the resource or this is a property of a view on that resource.

  S64 fence_value; // @Note: need a value that indicates a fence is ready by default (e.g. 0)
  void *backend_rsrc;
};

struct R_SubresourceRange {
  S32 mip_start;
  S32 mip_count;
  S32 slice_start;
  S32 slice_count;
};

enum R_ViewKind {
  R_ViewKind_None,
  R_ViewKind_ShaderResource,
  R_ViewKind_RenderTarget,
  R_ViewKind_DepthStencil,
};

struct R_View {
  S32 resource;
  R_ViewKind kind;
  R_SubresourceRange range;
  S32 descriptor_idx; // Index into SRV, RTV, or DSV heap.
};

struct R_ViewDesc {
  R_ViewKind kind;
  R_Format fmt; // In case the resource was created as typless and it needs to reinterpreted.
  R_SubresourceRange range;
};

// Either returns existing view if found in cache or creates a new one and allocates a descriptor for it.
static R_Handle r_view_from_texture(R_Handle texture, R_ViewDesc desc);

struct R_ResourceTable {
  R_ResourceSlot slots[R_RESOURCE_SLOTS_MAX];
  S32 count;
};

struct R_ViewTable {
  R_View slots[R_VIEW_SLOTS_MAX];
  S32 count;
};

/*
 @Todo:

 struct R_ResourceContext {
  R_Resource resources;
  S32 resources_count;

  R_View views;
  S32 views_count;
 };

 global R_ResourceContext r_resources;
 */

global R_ResourceTable r_resource_table; // -> r_resources.
global R_ViewTable r_views;

//
// Textures
//

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

 // @Todo: Deprecate, replace with newer R_ResourceState wherever used.
 enum R_TextureInitState {
   R_TextureInitState_Default,
   R_TextureInitState_RenderTarget,
   R_TextureInitState_DepthWrite,
   R_TextureInitState_CopyDest,
   R_TextureInitState_ShaderRead,
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

  R_Format fmt;
  U32 usage;
  R_TextureKind kind;
  R_TextureInitState init_state;

  B32 has_clear_value;
  union {
    V4F32 clear_color;
    struct {
      F32 depth;
      U8 stencil;
    }clear_ds;
  };
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

  R_Format format;
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

  R_Layout *input_layout;

  R_RasterizerState raster;
  R_DepthStencilState depth_stencil;
  R_BlendState blend;
  R_TopologyKind topology;

  R_Format rt_formats[8];
  S32 rt_count;

  R_Format depth_format;
  S32 sample_count; // MSAA
};

static R_Handle r_create_pipeline(R_PipelineDesc desc);
