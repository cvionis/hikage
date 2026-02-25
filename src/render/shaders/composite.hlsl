// @Todo: common.hlsl/resource.hlsl

//
// Constants
//

#define BINDLESS_TEXTURES_MAX 1024

#define MaterialFlag_None        0u
#define MaterialFlag_BaseColor   (1u << 0)
#define MaterialFlag_Normal      (1u << 1)
#define MaterialFlag_MetalRough  (1u << 2)
#define MaterialFlag_Occlusion   (1u << 3)
#define MaterialFlag_Emissive    (1u << 4)

//
// Materials
//

struct Material {
  float4 base_color; // @Todo: Should be float3, no?
  float3 emissive;

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

cbuffer PostProcessCB : register(b0) {
  uint tex_hdr_color;
};

//
// Resources
//

Texture2D g_textures_2d[512] : register(t0, space0);
Texture2DArray<float> g_textures_2d_array[512] : register(t0, space1);
StructuredBuffer<Material> g_materials : register(t0, space2);

SamplerState           g_sampler : register(s0);
SamplerComparisonState g_sampler_shadow : register(s1);

//
// Inputs/Outputs
//

struct VS_Out {
  float4 pos : SV_Position;
  float2 uv  : TEXCOORD0;
};

//
// Helpers
//

// @Todo: common.hlsl
#define NU(x) NonUniformResourceIndex(x)

//
// Pixel shader entry point
//

float4 ps_main(VS_Out i) : SV_Target
{
  float3 hdr = g_textures_2d[NU(tex_hdr_color)].Sample(g_sampler, i.uv).rgb;

  float3 tot = hdr;

  // Tonemap
  tot = tot /(1.0 + tot);
  // Gamma correct
  tot = pow(tot, 0.4545);
  // Color grading
  tot.r *= 1.06;
  tot.g *= 1.02;
  tot.b *= 0.96;

  //Texture2DArray<float> shadow_map = g_textures_2d_array[514];
  //tot.xyz = shadow_map.Sample(g_sampler, float3(i.uv.xy, 0)).r;

  return float4(tot, 1.0);
}
