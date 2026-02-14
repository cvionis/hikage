#define SHADOW_CASCADE_COUNT 4

// @Todo: lot's of redundancy and unused stuff here.

//
// Constant buffers
//

cbuffer ShadowFrameCB : register(b0) {
  float4x4 viewproj;
  float4 camera_ws;
  float4x4 light_viewproj[SHADOW_CASCADE_COUNT];
	float cascade_splits[SHADOW_CASCADE_COUNT];
	uint cascade_idx;
};

cbuffer DrawCB : register(b1) {
  float4x4 model_matrix;
  float4x4 normal_matrix;
  uint material;
};

//
// Inputs and outputs
//

struct VS_Input {
  float3 position : POSITION;
  float3 normal   : NORMAL;
  float4 tangent  : TANGENT;
  float2 uv       : TEXCOORD0;
};

struct VS_Output {
  float4 position : SV_POSITION;
};

VS_Output vs_main(VS_Input i)
{
  VS_Output o;
  float4 wpos = mul(float4(i.position, 1), model_matrix);
  o.position = mul(wpos, light_viewproj[cascade_idx]);
  return o;
}
