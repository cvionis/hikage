// ================================================================ //
// Draw a fullscreen triangle (for fullscreen/post-process effects) //
// ================================================================ //

//
// Inputs/Outputs
//

struct VS_Out {
  float4 pos : SV_Position;
  float2 uv  : TEXCOORD0;
};

//
// Vertex shader entry point
//

VS_Out vs_main(uint id : SV_VertexID)
{
  VS_Out o;

  float2 pos[3] = {
    float2(-1, -1),
    float2(-1,  3),
    float2( 3, -1)
  };

  float2 uv[3] = {
    float2(0, 1),
    float2(0, -1),
    float2(2, 1)
  };

  o.pos = float4(pos[id], 0, 1);
  o.uv  = uv[id];
  return o;
}
