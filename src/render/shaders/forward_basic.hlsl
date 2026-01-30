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

cbuffer FrameCB : register(b0) {
  float4x4 viewproj;
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

StructuredBuffer<Material> g_materials : register(t0, space1);
Texture2D g_textures[BINDLESS_TEXTURES_MAX] : register(t0, space0);
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
  float4 position    : SV_POSITION;
  float2 uv          : TEXCOORD0;
  float3 position_ws : TEXCOORD1;
  float3 normal      : TEXCOORD2;
  float4 tangent     : TEXCOORD3;
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

//
// Vertex shader entry point
//

PS_Input vs_main(VS_Input input)
{
  PS_Input output;

  float4 position_ws = mul(float4(input.position, 1.0), model_matrix);

  float3x3 normal_matrix_3x3 = (float3x3)normal_matrix;
  float3 normal = normalize(mul(input.normal, normal_matrix_3x3));
  float3 tangent = normalize(mul(input.tangent.xyz, normal_matrix_3x3));
  // Gram–Schmidt to prevent skew after interpolation (I have no idea how this works)
  tangent = normalize(tangent - normal*dot(tangent, normal));

  output.position = mul(position_ws, viewproj);
  output.uv = input.uv;
  output.position_ws = position_ws.xyz;

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

  // Base color
  float3 albedo = mtl.base_color.rgb;
  if (mtl.flags & MaterialFlag_BaseColor) {
    albedo = g_textures[NU(mtl.tex_base_color)].Sample(g_sampler, uv).rgb;
  }
  // Normal
  float3 normal_ws = input.normal;
  if (mtl.flags & MaterialFlag_Normal) {
    float2 normal_xy = g_textures[NU(mtl.tex_normal)].Sample(g_sampler, uv).rg;
    normal_xy = 2.0 * normal_xy - 1.;
    float normal_z = sqrt(saturate(1.0 - dot(normal_xy, normal_xy)));

    float3 normal_ts = float3(normal_xy.x, normal_xy.y, normal_z);
    normal_ws = normal_ws_from_ts(normal_ts, input.tangent, input.normal);
  }
  // Metal-roughness
  float2 metal_rough = float2(mtl.metallic, mtl.roughness);
  if (mtl.flags & MaterialFlag_MetalRough) {
    metal_rough = g_textures[NU(mtl.tex_metal_rough)].Sample(g_sampler, uv).rg;
  }
  // Emissive
  float3 emissive = mtl.emissive;
  if (mtl.flags & MaterialFlag_Emissive) {
    emissive = g_textures[NU(mtl.tex_emissive)].Sample(g_sampler, uv).rgb;
  }
  // Occlusion
  float occlusion = 1.;
  if (mtl.flags & MaterialFlag_Occlusion) {
    occlusion = g_textures[NU(mtl.tex_occlusion)].Sample(g_sampler, uv).r;
  }

  float3 lig = float3(0.9, 0.2, 0.4);

  float3 N = normalize(normal_ws);
  float3 V = normalize(camera_ws.xyz - input.position_ws);
  float3 L = normalize(lig);

  float NoL = saturate(dot(N, L));

  // Note: Entering non-physically-correct territory (temporary)

  float sky_dif = saturate(0.5+0.5*normal_ws.y);
  float bot_dif = 0.4*saturate(0.5-0.5*normal_ws.y);

  float3 amb = albedo*0.3;
  float3 lit = amb*occlusion +
    float3(1.1, 0.6, 0.4) * NoL +
    float3(0.4,0.6,1.) * sky_dif +
    float3(1.0,1.0,1.0) * bot_dif;

  float3 color = albedo * lit + emissive*1.2;

  color = pow(color, 1.0 / 2.2);

  return float4(color, 1.0);
}
