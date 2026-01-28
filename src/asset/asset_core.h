#pragma once

enum A_MaterialFlags {
  A_MaterialFlag_None = 0,
  A_MaterialFlag_BaseColor = (1 << 0),
  A_MaterialFlag_Normal = (1 << 1),
  A_MaterialFlag_MetalRough = (1 << 2),
  A_MaterialFlag_Occlusion = (1 << 3),
  A_MaterialFlag_Emissive = (1 << 4),
};

struct A_Vertex {
  V3F32 position;
  V3F32 normal;
  V4F32 tangent;
  V2F32 uv;
};

#define A_VERTEX_STRIDE sizeof(A_Vertex)
