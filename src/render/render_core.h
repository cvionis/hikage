#pragma once

global R_Layout mesh_layout = {
  .elements = {
    { S8("POSITION"), 0, R_Format_R32G32B32_Float,    0,  0, R_VertexInputClass_PerVertex, 0 },
    { S8("NORMAL"),   0, R_Format_R32G32B32_Float,    0, 12, R_VertexInputClass_PerVertex, 0 },
    { S8("TANGENT"),  0, R_Format_R32G32B32A32_Float, 0, 32, R_VertexInputClass_PerVertex, 0 },
    { S8("TEXCOORD"), 0, R_Format_R32G32_Float,       0, 24, R_VertexInputClass_PerVertex, 0 },
  },
  .elements_count = 4,
};

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

struct R_FrameCB {
  Mat4x4 viewproj;
  V4F32  camera_ws;
};

struct R_DrawCB {
  Mat4x4 model;
  Mat4x4 normal;
  U32 material;
  U32 _pad[3];
};

// @Todo: Move somewhere more permanent: scene.h
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
