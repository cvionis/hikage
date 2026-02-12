#define SHADOW_CASCADE_COUNT 4

//
// Constant buffers
//

cbuffer ShadowFrameCB : register(b0) {
  float4x4 viewproj;
  float4 camera_ws;
  float4x4 light_viewproj[R_SHADOW_CASCADE_COUNT];
	float4 cascade_splits; // @Note: Assumes <= 4 splits..
};

cbuffer ShadowDrawCB: register(b1) {
  uint cascade_index;
}

//
// Inputs and outputs
//

// @Note: Not using 90% of these...
struct VS_Input {
  float3 position : POSITION;
  float3 normal   : NORMAL;
  float4 tangent  : TANGENT;
  float2 uv       : TEXCOORD0;
};

struct VS_Output {
  float4 position : SV_POSITION;
};

VS_Out vs_main(VS_Input i)
{
  VS_Output o;
  float4 wpos = mul(float4(i.position,1), model);
  o.position = mul(wpos, light_viewproj[cascade_index]);
  return o;
}
