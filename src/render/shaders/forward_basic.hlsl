//
// Constants
//

#define R_BINDLESS_TEXTURES_MAX 1024

//
// Constant buffers
//

cbuffer FrameCB : register(b0)
{
  float4x4 viewproj;
  float4   camera_ws;
};

cbuffer DrawCB : register(b1)
{
  float4x4 model;
  float4x4 normal;
};

// ======================================================
// Bindless resources
// ======================================================

Texture2D g_textures[R_BINDLESS_TEXTURES_MAX] : register(t0);
SamplerState g_sampler : register(s0);

//
// Inputs and outputs
//

struct VS_Input
{
  float3 position : POSITION;
  float4 color    : COLOR;
  float3 normal   : NORMAL;
};

struct PS_Input
{
  float4 position   : SV_POSITION;
  float3 world_pos  : TEXCOORD0;
  float3 world_norm : TEXCOORD1;
  float4 color      : COLOR0;
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
  output.world_pos  = world_pos.xyz;
  output.world_norm = world_normal;
  output.color      = input.color;

  return output;
}

//
// Pixel shader entry point
//

float4 ps_main(PS_Input input) : SV_TARGET
{
  uint tex_idx = 1;
  float2 uv = input.position.xy * 0.01;

  //float3 albedo = float3(1.,1.,1.);
  float3 albedo = g_textures[tex_idx].Sample(g_sampler, uv).rgb;

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
