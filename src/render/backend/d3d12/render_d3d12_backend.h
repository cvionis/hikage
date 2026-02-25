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

#define R_D3D12_MAX_DRAWS 4096
#define R_D3D12_FRAME_COUNT 2

#define R_D3D12_MATERIAL_MAX 4096

#define R_D3D12_SRV_TEXTURE_2D_MAX       512
#define R_D3D12_SRV_TEXTURE_2D_ARRAY_MAX 512
#define R_D3D12_SRV_TEXTURE_TOTAL_MAX    (R_D3D12_SRV_TEXTURE_2D_MAX + R_D3D12_SRV_TEXTURE_2D_ARRAY_MAX)

#define R_D3D12_UAV_TEXTURE_2D_MAX       512
#define R_D3D12_UAV_TEXTURE_2D_ARRAY_MAX 512
#define R_D3D12_UAV_TEXTURE_TOTAL_MAX    (R_D3D12_UAV_TEXTURE_2D_MAX + R_D3D12_UAV_TEXTURE_2D_ARRAY_MAX)

#define R_D3D12_SRV_UAV_HEAP_SIZE  (R_D3D12_SRV_TEXTURE_TOTAL_MAX + R_D3D12_UAV_TEXTURE_TOTAL_MAX + 1) // @Note: +1 is for material buffer

#define R_D3D12_SRV_TEXTURE_2D_BASE          0
#define R_D3D12_SRV_TEXTURE_2D_ARRAY_BASE (R_D3D12_SRV_TEXTURE_2D_BASE + R_D3D12_SRV_TEXTURE_2D_MAX) // @Todo: Will need to enforce ranges when alloc. descriptors

#define R_D3D12_UAV_TEXTURE_2D_BASE       (R_D3D12_SRV_TEXTURE_2D_BASE + R_D3D12_SRV_TEXTURE_TOTAL_MAX)
#define R_D3D12_UAV_TEXTURE_2D_ARRAY_BASE (R_D3D12_UAV_TEXTURE_2D_BASE + R_D3D12_UAV_TEXTURE_2D_MAX)

#define R_D3D12_MATERIAL_BUFFER_BASE (R_D3D12_UAV_TEXTURE_2D_BASE + R_D3D12_UAV_TEXTURE_TOTAL_MAX)

// @Todo: Set the rest of these and use them instead of literals
#define R_D3D12_ROOT_SIG_SLOT_FRAME_CBV 0
#define R_D3D12_ROOT_SIG_SLOT_DRAW_CBV 1
#define R_D3D12_ROOT_SIG_SLOT_SRV_TEXTURE_2D 2
#define R_D3D12_ROOT_SIG_SLOT_SRV_TEXTURE_2D_ARRAY 3
#define R_D3D12_ROOT_SIG_SLOT_UAV_TEXTURE_2D 4
#define R_D3D12_ROOT_SIG_SLOT_UAV_TEXTURE_2D_ARRAY 4
#define R_D3D12_ROOT_SIG_SLOT_MATERIALS 6

struct R_D3D12_Backend {
  Arena *arena;

  // Window
  OS_Handle window;
  S32 width;
  S32 height;

  // Core pipeline objects
  IDXGISwapChain3 *swapchain;
  ID3D12Device *device;

  ID3D12Resource *back_buffers[R_D3D12_FRAME_COUNT];

  ID3D12CommandAllocator *command_allocators[R_D3D12_FRAME_COUNT];
  ID3D12CommandQueue *command_queue; // @Todo: Rename gfx or draw queue or something to differentiate from upload/copy queue
  ID3D12GraphicsCommandList *command_list;
  ID3D12RootSignature *root_signature;

  // Frame synchronization
  U32 frame_idx;
  HANDLE fence_event;
  ID3D12Fence *fence;
  U64 fence_values[R_D3D12_FRAME_COUNT];

  ID3D12Resource *material_buffer;
  U32 material_srv_idx; // @Todo: Remove. Redundant. Use #define you already have.
  U32 material_capacity;

  ID3D12Fence *copy_fence;
  HANDLE copy_fence_event;
  U64 copy_fence_value;

  ID3D12GraphicsCommandList *copy_cmd_list;
  ID3D12CommandAllocator *copy_cmd_allocator;

  /*
  SRV heap:
    textures (2D, 2D array)
    uav's
    material buffer
  ...
  */

  ID3D12DescriptorHeap *srv_uav_heap;
  ID3D12DescriptorHeap *rtv_heap;
  ID3D12DescriptorHeap *dsv_heap;
  ID3D12DescriptorHeap *sampler_heap;  // @Note: Currently unused. Just using static samplers for now.

  // Descriptor allocation (for texture views)
  S32 srv_uav_descriptor_size;
  S32 rtv_descriptor_size;
  S32 dsv_descriptor_size;

  S32 srv_2d_next_idx;
  S32 srv_2darray_next_idx;
  S32 uav_2d_next_idx;
  S32 uav_2darray_next_idx;
  S32 mtl_next_idx; // Material table slot allocation

  S32 rtv_next_idx;
  S32 dsv_next_idx;

  //
  // Compute (new)
  //

};

static R_D3D12_Backend r_ctx;
