// Constant buffers

cbuffer CameraCB : register(b0) {
  float4x4 viewproj;
  float3   camera_ws;
  float    _pad0;
  float4x4 view;
};

cbuffer ModelCB : register(b1) {
  float4x4 model;
  float4x4 normal_matrix;
};

cbuffer MaterialCB : register(b2) {
  float4 base_color;
};

// Inputs and outputs

struct VS_Input {
  float3 position : POSITION;
  float4 color    : COLOR;
  float3 normal   : NORMAL;
};

struct PS_Input {
  float4 position   : SV_POSITION;
  float3 world_pos  : TEXCOORD0;
  float3 world_norm : TEXCOORD1;
  float4 color      : COLOR0;
};

// Helper functions

float3 tonemap_aces(float3 x)
{
  float a = 2.51;
  float b = 0.03;
  float c = 2.43;
  float d = 0.59;
  float e = 0.14;
  return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// Entry points

PS_Input vs_main(VS_Input input)
{
  PS_Input output;

  float4 world_pos       = mul(float4(input.position, 1.0), model);
  float3x3 inv_transpose = (float3x3)normal_matrix;
  float3 world_normal    = normalize(mul(inv_transpose, input.normal)); // @Todo: Multiplication order...

  output.position   = mul(world_pos, viewproj);
  output.world_pos  = world_pos.xyz;
  output.world_norm = world_normal;
  output.color      = input.color;

  return output;
}

float4 ps_main(PS_Input input) : SV_TARGET
{
  float3 lig = float3(0.9, 0.2, 0.4);

  float3 N = normalize(input.world_norm);
  float3 V = normalize(camera_ws - input.world_pos);
  float3 L = normalize(lig);
  float3 H = normalize(L + V);

  float NoL = saturate(dot(N, L));
  float NoV = saturate(dot(N, V));
  float NoH = saturate(dot(N, H));
  float VoH = saturate(dot(V, H));

  float3 diff = NoL;

  float3 lit = float3(0.,0.,0.);
  lit += float3(1.1,0.6,0.4)*diff;

  // Tone map + gamma
  float3 color = base_color;
  color = color * lit;

  //float exposure = 0.25;
  //color = tonemap_aces(color * exposure);
  color = pow(color, 1.0 / 2.2);
  color = saturate(color);

  return float4(color,1.0);
}
