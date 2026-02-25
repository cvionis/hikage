#pragma once

// @Todo: render_core -> render_common ?

enum R_ClearFlags {
  R_ClearFlag_None  = 0,
  R_ClearFlag_Color = (1 << 0),
  R_ClearFlag_Depth = (1 << 1),
};

struct R_Viewport {
  RectF32 rect;
  F32 min_depth;
  F32 max_depth;
};

struct R_Scissor {
  RectS32 rect;
};

enum R_Topology {
  R_Topology_TriangleList,
  R_Topology_TriangleStrip,
  R_Topology_LineList,
  R_Topology_LineStrip,
  R_Topology_PointList,
};

// @Todo: Not sure where to store this. I don't think this is a good place.
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

// Linear GPU allocator

struct R_LinearAllocator {
  void *backend;
  U8 *cpu_base;
  U64 gpu_base;
  U64 pos;
  U64 capacity;
};

struct R_Alloc {
  void *cpu;
  U64 gpu;
};

static R_LinearAllocator r_allocator; // @Note: Temporarily static.

static R_LinearAllocator r_alloc_make(U64 size); // Backend-specific impl
static void r_alloc_release(R_LinearAllocator *alloc); // Backend-specific impl
static void r_alloc_reset(R_LinearAllocator *alloc);
static R_Alloc r_alloc_push(R_LinearAllocator *alloc, U64 size);

//
// Globals
//

static R_Layout r_mesh_layout = {
  .elements = {
    { S8("POSITION"), 0, R_Format_R32G32B32_Float,    0,  0, R_VertexInputClass_PerVertex, 0 },
    { S8("NORMAL"),   0, R_Format_R32G32B32_Float,    0, 12, R_VertexInputClass_PerVertex, 0 },
    { S8("TANGENT"),  0, R_Format_R32G32B32A32_Float, 0, 24, R_VertexInputClass_PerVertex, 0 },
    { S8("TEXCOORD"), 0, R_Format_R32G32_Float,       0, 40, R_VertexInputClass_PerVertex, 0 },
  },
  .elements_count = 4,
};

//
// TEMPORARY LOCATION
//

// @Todo: Move somewhere more permanent: scene.h

struct DirectionalLight {
  V3F32 color;
  F32 brightness;
  V3F32 direction;
};

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

  F32 near_z;
  F32 far_z;
  F32 aspect;

  F32 fov;
  B32 ortho;
};

static void camera_update_position_aspect(Camera *camera, V3F32 delta, F32 aspect, F32 delta_time);
static void camera_update_direction(Camera *camera, F32 yaw_delta, F32 pitch_delta, F32 delta_time);
