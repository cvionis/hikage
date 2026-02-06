#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// @Todo: Move these
#pragma warning(push, 0)
#include <windows.h>
#include <d3d12.h>
#include "third_party/D3DX12/d3dx12.h"
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <shellapi.h>
#pragma warning(pop, 0)

#define R_D3D12_FRAME_COUNT 2

global R_Layout mesh_layout = {
  .elements = {
    { S8("POSITION"), 0, R_Format_R32G32B32_Float,    0,  0, R_VertexInputClass_PerVertex, 0 },
    { S8("NORMAL"),   0, R_Format_R32G32B32_Float,    0, 12, R_VertexInputClass_PerVertex, 0 },
    { S8("TANGENT"),  0, R_Format_R32G32B32A32_Float, 0, 32, R_VertexInputClass_PerVertex, 0 },
    { S8("TEXCOORD"), 0, R_Format_R32G32_Float,       0, 24, R_VertexInputClass_PerVertex, 0 },
  },
  .elements_count = 4,
};

// @Note: Not a fan of having a static set of pipelines like this, but it simplifies things for now (allows pass's execute procedures
// to set pipeline directly without having to pass a pipeline)

struct R_Pipelines {
  R_Handle forward;
  // ...
};
static void r_create_pipelines(void);

// @Todo: -> R_D3D12_Context, move to backend/d3d12/render_context_d3d12.h
struct R_Context {
  Arena *arena;

  // Window
  OS_Handle window;
  S32 width;
  S32 height;

  R_Pipelines pipelines; // @Note: Temporary

  // Core pipeline objects
  IDXGISwapChain3 *swapchain;
  ID3D12Device *device;

  ID3D12Resource *render_targets[R_D3D12_FRAME_COUNT];

  ID3D12PipelineState *pipeline_state;

  ID3D12CommandAllocator *command_allocators[R_D3D12_FRAME_COUNT];
  ID3D12CommandQueue *command_queue; // @Todo: Rename gfx or draw queue or something to differentiate from upload/copy queue
  ID3D12GraphicsCommandList *command_list;
  ID3D12RootSignature *root_signature;

  // Depth/stencil buffers
  ID3D12Resource *depth_buffer;

  // Color buffer
  ID3D12Resource *color_buffer;

  // Cached for shader reload
  D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc;
  D3D12_INPUT_ELEMENT_DESC input_desc[4];

  // Frame synchronization
  U32 frame_idx;
  HANDLE fence_event;
  ID3D12Fence *fence;
  U64 fence_values[R_D3D12_FRAME_COUNT];

  //                                         //
  // ============ NEW STUFF ================ //
  //                                         //

  ID3D12Resource *material_buffer;
  U32 material_srv_idx; // @Todo: Remove. Redundant. Use #define you already have.
  U32 material_capacity;

  ID3D12Resource *frame_cb;
  U8 *frame_cb_mapped;

  ID3D12Resource *draw_cb_buffer;
  U8 *draw_cb_buffer_mapped;
  U32 draw_cb_stride;        // always 256
  U32 draw_cb_capacity;      // number of slots
  U32 draw_cb_write_idx;     // cursor

  ID3D12Fence *copy_fence;
  HANDLE copy_fence_event;
  U64 copy_fence_value;

  ID3D12GraphicsCommandList *copy_cmd_list;
  ID3D12CommandAllocator *copy_cmd_allocator;

  ID3D12DescriptorHeap *dsv_heap;
  ID3D12DescriptorHeap *rtv_heap;

  /*
  0–1        CBVs
  2–1025     textures (fixed region)
  1026       material buffer
  ...
  */

  ID3D12DescriptorHeap *srv_heap;       // Per-frame and per-draw data, texture table, material table
  ID3D12DescriptorHeap *sampler_heap;   // @Note: Just using a static sampler for now.

  S32 rtv_descriptor_size;
  S32 srv_descriptor_size;

  S32 srv_next_idx; // @Todo: Rename. Used exclusively for texture table entries.
};

global R_Context r_ctx;

struct R_MaterialGPU {
  V4F32 base_color;
  V3F32 emissive;

  F32 metallic;
  F32 roughness;

  U32 flags;

  U32 tex_base_color;
  U32 tex_normal;
  U32 tex_metal_rough;
  U32 tex_occlusion;
  U32 tex_emissive;
};

// @Todo: Move somewhere more permanent (probably outside render layer?)
struct Camera {
  Mat4x4 view;
  Mat4x4 proj;
  Mat4x4 viewproj;

  V3F32 position;
  V3F32 position_target;

  V3F32 direction;
  F32 yaw;
  F32 pitch;
  F32 yaw_target;
  F32 pitch_target;

  F32 fov;
  B32 ortho;
};

static void camera_update_position_aspect(Camera *camera, V3F32 delta, F32 aspect, F32 delta_time);
static void camera_update_direction(Camera *camera, F32 yaw_delta, F32 pitch_delta, F32 delta_time);
