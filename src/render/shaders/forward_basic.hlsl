//
// Constants
//

#define PI 3.14159

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
// Specular BRDF implementations
//

// @Note: All of these are directly from the following page:
// https://google.github.io/filament/main/filament.html#materialsystem

float D_GGX(float a, float NoH)
{
  float a2 = a * a;
  float f = (NoH * a2 - NoH) * NoH + 1.0;
  return a2 / (PI * f * f);
}

float V_SmithGGXCorrelated(float NoV, float NoL, float a)
{
  float a2 = a * a;
  float GGXV = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
  float GGXL = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
  return 0.5 / (GGXV + GGXL + 1e-5);
}

float3 F_Schlick(float VoH, float3 f0)
{
  return f0 + (1.0 - f0) * pow(1.0 - VoH, 5.0);
}

float F_Schlick(float u, float f0, float f90)
{
  return f0 + (f90 - f0) * pow(1.0 - u, 5.0);
}

//
// Diffuse BRDF implementations
//

// @Note: All of these are directly from the following page:
// https://google.github.io/filament/main/filament.html#materialsystem

float Fd_Lambert()
{
  return 1.0 / PI;
}

float Fd_Burley(float NoV, float NoL, float LoH, float roughness)
{
  float f90 = 0.5 + 2.0 * roughness * LoH * LoH;
  float lightScatter = F_Schlick(NoL, 1.0, f90);
  float viewScatter = F_Schlick(NoV, 1.0, f90);
  return lightScatter * viewScatter * (1.0 / PI);
}

//
// Constant buffers
//

#define SHADOW_CASCADE_COUNT 4

cbuffer FrameCB : register(b0) {
  float4x4 viewproj;
  float4x4 view; // @Note: Redundant and temporary. Need to just upload view+proj separately.
  float4x4 light_viewproj[SHADOW_CASCADE_COUNT];
  float4 cascade_splits; // @Note: Maximum 4 splits
  float4 camera_ws;
};

cbuffer DrawCB : register(b1) {
  float4x4 model_matrix;
  float4x4 normal_matrix;
  uint material;
};

//
// Resources
//

Texture2D g_textures_2d[512]       : register(t0, space0);
Texture2DArray<float> g_textures_2d_array[512] : register(t0, space1);
StructuredBuffer<Material> g_materials : register(t0, space2);

SamplerState           g_sampler : register(s0);
SamplerComparisonState g_sampler_shadow : register(s1);

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
  float4 position    : SV_POSITION;
  float2 uv          : TEXCOORD0;
  float3 position_ws : TEXCOORD1;
  float3 position_vs : TEXCOORD2;
  float3 normal      : TEXCOORD3;
  float4 tangent     : TEXCOORD4;
};

//
// Helpers
//

#define NU(x) NonUniformResourceIndex(x)

float3 normal_ws_from_ts(float3 normal_ts, float4 t, float3 n)
{
  float3 N = normalize(n);
  float3 T = normalize(t.xyz);
  float h = t.w;

  // Re-orthogonalize at the shading point (again!)
  T = normalize(T - N*dot(T, N));
  float3 B = cross(N, T) * h;

  float3x3 TBN = float3x3(T, B, N);
  return normalize(mul(normal_ts, TBN));
}

uint cascade_from_viewspace_depth(float depth)
{
  uint cascade_idx;

  if (depth < cascade_splits.x) cascade_idx = 0;
  else if (depth < cascade_splits.y) cascade_idx = 1;
  else if (depth < cascade_splits.z) cascade_idx = 2;
  else cascade_idx = 3;

  return cascade_idx;
}

//
// Vertex shader entry point
//

PS_Input vs_main(VS_Input input)
{
  PS_Input output;

  float4 position_ws = mul(float4(input.position, 1.0), model_matrix);
  float4 position_vs = mul(float4(input.position, 1.0), view);

  float3x3 normal_matrix_3x3 = (float3x3)normal_matrix;
  float3 normal = normalize(mul(input.normal, normal_matrix_3x3));
  float3 tangent = normalize(mul(input.tangent.xyz, normal_matrix_3x3));
  // Gram–Schmidt to prevent skew after interpolation (I have no idea how this works)
  tangent = normalize(tangent - normal*dot(tangent, normal));

  output.position = mul(position_ws, viewproj);
  output.uv = input.uv;
  output.position_ws = position_ws.xyz;
  output.position_vs = position_vs.xyz;

  output.normal = normal;
  output.tangent = float4(tangent.xyz, input.tangent.w);

  return output;
}

//
// Pixel shader entry point
//

float4 ps_main(PS_Input input) : SV_TARGET
{
  float2 uv = input.uv;
  Material mtl = g_materials[NU(material)];

  //
  // Get material parameters
  //

  // Base color
  float3 base_color = mtl.base_color.rgb;
  if (mtl.flags & MaterialFlag_BaseColor) {
    base_color = g_textures_2d[NU(mtl.tex_base_color)].Sample(g_sampler, uv).rgb;
  }
  // Normal
  float3 normal_ws = input.normal;
  if (mtl.flags & MaterialFlag_Normal) {
    float2 normal_xy = g_textures_2d[NU(mtl.tex_normal)].Sample(g_sampler, uv).rg;
    normal_xy = 2.0 * normal_xy - 1.;
    float normal_z = sqrt(saturate(1.0 - dot(normal_xy, normal_xy)));

    float3 normal_ts = float3(normal_xy.x, normal_xy.y, normal_z);
    normal_ws = normal_ws_from_ts(normal_ts, input.tangent, input.normal);
  }
  // Metallic and roughness
  float2 metal_rough = float2(mtl.metallic, mtl.roughness);
  if (mtl.flags & MaterialFlag_MetalRough) {
    metal_rough = g_textures_2d[NU(mtl.tex_metal_rough)].Sample(g_sampler, uv).rg;
  }
  float metallic = metal_rough.x;
  float roughness = metal_rough.y;
  float a = max(roughness*roughness, 1e-4);

  // Emissive
  float3 emissive = mtl.emissive;
  if (mtl.flags & MaterialFlag_Emissive) {
    emissive = g_textures_2d[NU(mtl.tex_emissive)].Sample(g_sampler, uv).rgb;
  }
  // Occlusion
  float occlusion = 1.;
  if (mtl.flags & MaterialFlag_Occlusion) {
    occlusion = g_textures_2d[NU(mtl.tex_occlusion)].Sample(g_sampler, uv).r;
  }

  //
  // Lighting vectors
  //

  float3 light_direction = normalize(float3(-0.4, -0.8, -0.1)); // @Todo: Upload in frame CB

  float3 n = normalize(normal_ws);
  float3 v = normalize(camera_ws.xyz - input.position_ws);
  float3 l = normalize(-light_direction);
  float3 h = normalize(l + v);

  float NoL = saturate(dot(n, l));
  float NoH = saturate(dot(n, h));
  float NoV = saturate(dot(n, v));
  float LoH = saturate(dot(l, h));
  float VoH = saturate(dot(v, h));

  //
  // Shadows
  //

  Texture2DArray<float> shadow_map = g_textures_2d_array[514]; // @Todo: Temp.

  float depth_vs = input.position_vs.z;
  uint cascade_idx = cascade_from_viewspace_depth(depth_vs);

  float4 shadow_clip = mul(float4(input.position_ws, 1.0), light_viewproj[cascade_idx]);
  float3 shadow_ndc = shadow_clip.xyz / shadow_clip.w;
  float2 shadow_uv = float2(
    shadow_ndc.x * 0.5f + 0.5f,
    -shadow_ndc.y * 0.5f + 0.5f // @Todo: Briefly work out why flip is necessary
  );

  float shadow_depth = shadow_ndc.z;
  float3 shadow_map_coord = float3(shadow_uv, shadow_depth);
  float bias = 0.005;
  float shadow = shadow_map.SampleCmpLevelZero(g_sampler_shadow, float3(shadow_map_coord.xy, cascade_idx), shadow_map_coord.z - bias);

  //
  // Compute diffuse and specular reflectance
  //

  float3 f0 = lerp(float3(0.04, 0.04, 0.04), base_color, metallic);
  float3 diffuse_color = (1.0 - metallic) * base_color.rgb;

  // Specular BRDF
  float D = D_GGX(NoH, a);
  float V = V_SmithGGXCorrelated(NoV, NoL, a);
  float3 F = F_Schlick(VoH, f0);
  float3 Fs = (D * V * F); /// @Note: V bakes in G and the division by 4(NoV*NoL)?

  // Diffuse BRDF
  float3 Fd = diffuse_color * Fd_Burley(NoV, NoL, LoH, roughness);

  //
  // Compute direct lighting
  //

  float3 light_color = float3(1., 0.8, 0.6);
  float light_intensity = 20.;
  float3 radiance = light_color * light_intensity;

  float3 direct = (Fd + Fs) * radiance * NoL;

  //
  // Compute indirect lighting (@Todo)
  //

  float3 ambient_tmp = diffuse_color*float3(0.09, 0.09, 0.09);
  float3 indirect = ambient_tmp;

  //
  // Final color
  //

  float3 Lo = direct*shadow + indirect*occlusion;
  float3 color = Lo + emissive;

  return float4(color, 1.0);
}
