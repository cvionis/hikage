#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#pragma warning(push, 0)
#include <windows.h>
#include <d3d12.h>
#include "third_party/D3DX12/d3dx12.h"
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <shellapi.h>
#pragma warning(pop, 0)

#define R_D3D12_FRAME_COUNT 2

// @Todo: -> R_D3D12_Context, move to backend/d3d12/render_context_d3d12.h
struct R_Context {
  // Window
  OS_Handle window;
  S32 width;
  S32 height;

  // Core pipeline objects
  IDXGISwapChain3 *swapchain;
  ID3D12Device *device;

  ID3D12Resource *render_targets[R_D3D12_FRAME_COUNT];

  ID3D12PipelineState *pipeline_state;

  ID3D12CommandAllocator *command_allocators[R_D3D12_FRAME_COUNT];
  ID3D12CommandQueue *command_queue; // @Todo: Rename gfx queue or something to differentiate from upload queue
  ID3D12GraphicsCommandList *command_list;
  ID3D12RootSignature *root_signature;

  // Depth/stencil buffers
  ID3D12Resource *depth_buffer;

  // Color buffer
  ID3D12Resource *color_buffer;

  // Cached for shader reload
  D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc;
  D3D12_INPUT_ELEMENT_DESC input_desc[3];

  // Vertex & index buffer
  ID3D12Resource *vertex_buffer;
  D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;
  ID3D12Resource *index_buffer;
  D3D12_INDEX_BUFFER_VIEW index_buffer_view;

  //!! @Todo: Deprecate !!//
  // Uniform descriptor heap (shared by all uniforms)
  ID3D12DescriptorHeap *cbv_heap;
  S32 cbv_descriptor_size;
  S32 cbv_count;         // Total descriptors in cbv_heap
  S32 model_cbv_base;    // Base index for models
  S32 material_cbv_base; // Base index for materials

  // Uniform data backing store: camera
  ID3D12Resource *camera_cb;
  U8 *camera_cb_mapped;

  // Uniform data backing store: model transforms
  ID3D12Resource *model_cb;
  U8 *model_cb_mapped;
  U32 model_cb_stride;

  // Uniform data backing store: materials
  ID3D12Resource *material_cb;
  U8 *material_cb_mapped;
  U32 material_cb_stride;

  // Frame synchronization
  U32 frame_idx;
  HANDLE fence_event;
  ID3D12Fence *fence;
  U64 fence_values[R_D3D12_FRAME_COUNT];

  //                                         //
  // ============ NEW STUFF ================ //
  //                                         //

  ID3D12Fence *copy_fence;
  HANDLE copy_fence_event;
  U64 copy_fence_value;

  ID3D12GraphicsCommandList *copy_cmd_list;
  ID3D12CommandAllocator *copy_cmd_allocator;

  ID3D12DescriptorHeap *dsv_heap;
  ID3D12DescriptorHeap *rtv_heap;
  ID3D12DescriptorHeap *srv_heap;       // Per-frame and per-draw data, texture table, material table
  ID3D12DescriptorHeap *sampler_heap;

  S32 rtv_descriptor_size;
  S32 srv_descriptor_size;

  S32 srv_next_idx;
};

global R_Context r_ctx;

// @Todo: These definitely wouldn't be defined in render_core.h
struct R_CameraCB {
  Mat4x4 viewproj;
  V4F32 camera_ws;
  Mat4x4 view;
};

struct R_MaterialCB {
  V3F32 base_color;
};

// @Todo: Move somewhere more permanent

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

struct ModelTmp {
  V3F32 position;
  V3F32 scale;
};

static void camera_update_position_aspect(Camera *camera, V3F32 delta, F32 aspect, F32 delta_time);
static void camera_update_direction(Camera *camera, F32 yaw_delta, F32 pitch_delta, F32 delta_time);
