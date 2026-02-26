cbuffer CB : register(b0) {
    uint src_width;  // optional (not required if sampling normalized UVs)
    uint src_height; // optional

    uint src_tex_idx;
    uint dst_tex_idx;
};

Texture2D<float4> g_textures_2d[512] : register(t0, space0);
RWTexture2DArray<float4> g_textures_rw_2d_array[512] : register(u0, space1);
SamplerState g_sampler : register(s0);

static const float PI = 3.14159;
static const uint cube_dim = 6;

float3 face_uv_to_dir(uint face, float u, float v)
{
    // u, v in [-1, 1], v positive up in math space
    // Face order: 0:+X 1:-X 2:+Y 3:-Y 4:+Z 5:-Z
    float3 d;
    if      (face == 0) d = float3( 1, -v, -u);
    else if (face == 1) d = float3(-1, -v,  u);
    else if (face == 2) d = float3( u,  1,  v);
    else if (face == 3) d = float3( u, -1, -v);
    else if (face == 4) d = float3( u, -v,  1);
    else                d = float3(-u, -v, -1);

    return normalize(d);
}

float2 dir_to_lat_long_uv(float3 d)
{
    // longitude theta in [-pi, pi], latitude phi in [-pi/2, pi/2]
    float theta = atan2(d.z, d.x);
    float phi   = asin(clamp(d.y, -1.0, 1.0));

    float u = (theta + PI) / (2.0 * PI);      // [0,1]
    float v = (PI * 0.5 - phi) / PI;          // [0,1] top->bottom
    return float2(u, v);
}

[numthreads(8, 8, 1)]
void cs_main(uint3 tid : SV_DispatchThreadID)
{
    uint x = tid.x;
    uint y = tid.y;
    uint face = tid.z;

    if (face >= 6) {
      return;
    }
    if (x >= cube_dim || y >= cube_dim) {
      return;
    }

    // Pixel center in [0,1]
    float2 p = (float2(x, y) + 0.5) / float(cube_dim);

    // Map to [-1,1], with v positive up (note: y increases downward in texture space)
    float u = 2.0 * p.x - 1.0;
    float v = 1.0 - 2.0 * p.y;

    float3 dir = face_uv_to_dir(face, u, v);
    float2 uv  = dir_to_lat_long_uv(dir);

    float4 c = g_textures_2d[src_tex_idx].SampleLevel(g_sampler, uv, 0.0);
    g_textures_rw_2d_array[dst_tex_idx][uint3(x, y, face)] = c;
}
