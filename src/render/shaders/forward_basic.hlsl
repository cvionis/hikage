//
// Constants
//

#define R_BINDLESS_TEXTURES_MAX 1024

//
// Structures
//

// @Todo: Define mtl flags in here

struct Material {
  float4 base_color;
  float4 emissive;

  float metallic;
  float roughness;

  uint flags;

  uint tex_base_color;
  uint tex_normal;
  uint tex_metal_rough;
  uint tex_occlusion;
  uint tex_emissive;
};

//
// Constant buffers
//

cbuffer FrameCB : register(b0) {
  float4x4 viewproj;
  float4 camera_ws;
};

cbuffer DrawCB : register(b1) {
  float4x4 model;
  float4x4 normal;
  uint material;
};

//
// Resources
//

StructuredBuffer<Material> g_materials : register(t0, space1);
Texture2D g_textures[R_BINDLESS_TEXTURES_MAX] : register(t0, space0);
SamplerState g_sampler : register(s0);

//
// Inputs and outputs
//

struct VS_Input {
  float3 position : POSITION;
  float3 normal   : NORMAL;
  float4 tangent  : TANGENT;
  float2 uv       : TEXCOORD0;
};

struct PS_Input {
  float4 position   : SV_POSITION;
  float2 uv         : TEXCOORD0;
  float3 world_pos  : TEXCOORD1;
  float3 world_norm : TEXCOORD2;
};

//
// Vertex shader entry point
//

PS_Input vs_main(VS_Input input)
{
  PS_Input output;

  // @Note: Temporary
  float4x4 identity = float4x4(
    1,0,0,0,
    0,1,0,0,
    0,0,1,0,
    0,0,0,1
  );

  float4 world_pos = mul(float4(input.position, 1.0), identity);

  float3 world_normal =
    normalize(mul((float3x3)identity, input.normal));

  output.position   = mul(world_pos, viewproj);
  output.uv         = input.uv;
  output.world_pos  = world_pos.xyz;
  output.world_norm = world_normal;

  return output;
}

//
// Pixel shader entry point
//

float4 ps_main(PS_Input input) : SV_TARGET
{
  uint tex_idx = 0;
  float2 uv = input.uv;

  //float3 albedo = float3(1.,1.,1.);
  Material mtl = g_materials[NonUniformResourceIndex(material)];

  uint tex_base_color = mtl.tex_base_color;
  float3 albedo = g_textures[NonUniformResourceIndex(tex_base_color)].Sample(g_sampler, uv).rgb;

  float3 lig = float3(0.9, 0.2, 0.4);

  float3 N = normalize(input.world_norm);
  float3 V = normalize(camera_ws.xyz - input.world_pos);
  float3 L = normalize(lig);

  float NoL = saturate(dot(N, L));

  float3 amb = albedo*0.3;
  float3 lit = amb + float3(1.1, 0.6, 0.4) * NoL;

  float3 color = albedo * lit;

  color = pow(color, 1.0 / 2.2);
  color = saturate(color);

  return float4(color, 1.0);
}
