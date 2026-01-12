//
// Basic math utilities
//

function F32
lerpf32(F32 a, F32 b, F32 t)
{
  return (1.f - t)*a + t*b;
}

//
// Easing functions
//

function F32
ease_quintic_inout(F32 t)
{
  return (t < 0.5f) ? (16*t*t*t*t*t) : (1 - powf32(-2* t + 2, 5) / 2);
}

function F32
ease_expo_in(F32 t)
{
  return 1 - powf32(2, -10 * t); 
}

function F32
ease_circle_inout(F32 t)
{
  return (t < 0.5f)
    ? (1 - sqrtf32(1 - powf32(2 * t, 2))) / 2
    : (sqrtf32(1 - powf32(-2 * t + 2, 2)) + 1) / 2;
}

//
// Two-dimensional vectors
//

// V2F32

function V2F32 
v2f32(F32 x, F32 y)
{
  V2F32 v = {0};
  v.x = x;
  v.y = y;
  return v;
}

function V2F32 
v2f32_add(V2F32 a, V2F32 b)
{
  V2F32 v = {0};
  v.x = a.x + b.x;
  v.y = a.y + b.y;
  return v;
}

function V2F32 
v2f32_sub(V2F32 a, V2F32 b)
{
  V2F32 v = {0};
  v.x = a.x - b.x;
  v.y = a.y - b.y;
  return v;
}

function V2F32 
v2f32_mul(V2F32 a, V2F32 b)
{
  V2F32 v = {0};
  v.x = a.x * b.x;
  v.y = a.y * b.y;
  return v;
}

function V2F32 
v2f32_div(V2F32 a, V2F32 b)
{
  V2F32 v = {0};
  v.x = a.x / b.x;
  v.y = a.y / b.y;
  return v;
}

function V2F32 
v2f32_scale(V2F32 v, F32 s)
{
  v.x *= s;
  v.y *= s;
  return v;
}

function V2F32 
v2f32_normalize(V2F32 v)
{
  return v2f32_scale(v, 1.f / v2f32_length(v));
}

function V2F32 
v2f32_lerp(V2F32 a, V2F32 b, F32 t)
{
  V2F32 v = {0};
  v.x = lerpf32(a.x, b.x, t);
  v.y = lerpf32(a.y, b.y, t);
  return v;
}

function F32 
v2f32_dot(V2F32 a, V2F32 b)
{
  return a.x*b.x + a.y*b.y;
}

function F32 
v2f32_length(V2F32 v)
{
  return sqrtf32(v2f32_dot(v, v));
}

function F32 
v2f32_angle(V2F32 v)
{
  F32 angle = atan2f32(v.x, v.y);
  return angle; 
}

// V2U32

function V2U32 
v2u32(U32 x, U32 y)
{
  V2U32 v = {0};
  v.x = x;
  v.y = y; 
  return v;
}

function V2U32 
v2u32_add(V2U32 a, V2U32 b)
{
  V2U32 v = {0};
  v.x = a.x + b.x;
  v.y = a.y + b.y;
  return v;
}

function V2U32 
v2u32_sub(V2U32 a, V2U32 b)
{
  V2U32 v = {0};
  v.x = a.x - b.x;
  v.y = a.y - b.y;
  return v;
}

function V2U32 
v2u32_mul(V2U32 a, V2U32 b)
{
  V2U32 v = {0};
  v.x = a.x * b.x;
  v.y = a.y * b.y;
  return v;
}

function V2U32 
v2u32_div(V2U32 a, V2U32 b)
{
  V2U32 v = {0};
  v.x = a.x / b.x;
  v.y = a.y / b.y;
  return v; 
}

function V2U32 
v2u32_scale(V2U32 v, U32 s)
{
  v.x *= s;
  v.y *= s;
  return v;
}

// V2S32

function V2S32 
v2s32(S32 x, S32 y)
{
  V2S32 v = {0};
  v.x = x; 
  v.y = y;
  return v;
}

function V2S32 
v2s32_add(V2S32 a, V2S32 b)
{
  V2S32 v = {0};
  v.x = a.x + b.x;
  v.y = a.y + b.y;
  return v;
}

function V2S32 
v2s32_sub(V2S32 a, V2S32 b)
{
  V2S32 v = {0};
  v.x = a.x - b.x;
  v.y = a.y - b.y;
  return v;
}

function V2S32 
v2s32_mul(V2S32 a, V2S32 b)
{
  V2S32 v = {0};
  v.x = a.x * b.x;
  v.y = a.y * b.y;
  return v;
}

function V2S32 
v2s32_div(V2S32 a, V2S32 b)
{
  V2S32 v = {0};
  v.x = a.x / b.x;
  v.y = a.y / b.y;
  return v; 
}

function V2S32 
v2s32_scale(V2S32 v, S32 s)
{
  v.x *= s;
  v.y *= s;
  return v;
}

//
// Three-dimensional vectors
//

// V3F32 

function V3F32 
v3f32(F32 x, F32 y, F32 z)
{
  V3F32 v = {0};
  v.x = x;
  v.y = y;
  v.z = z;
  return v;
}

function V3F32 
v3f32_add(V3F32 a, V3F32 b)
{
  V3F32 v = {0};
  v.x = a.x + b.x;
  v.y = a.y + b.y;
  v.z = a.z + b.z;
  return v;
}

function V3F32 
v3f32_sub(V3F32 a, V3F32 b)
{
  V3F32 v = {0};
  v.x = a.x - b.x;
  v.y = a.y - b.y;
  v.z = a.z - b.z;
  return v;
}

function V3F32 
v3f32_mul(V3F32 a, V3F32 b)
{
  V3F32 v = {0};
  v.x = a.x * b.x;
  v.y = a.y * b.y;
  v.z = a.z * b.z;
  return v;
}

function V3F32 
v3f32_div(V3F32 a, V3F32 b)
{
  V3F32 v = {0};
  v.x = a.x / b.x;
  v.y = a.y / b.y;
  v.z = a.z / b.z;
  return v;
}

function V3F32 
v3f32_scale(V3F32 v, F32 s)
{
  v.x *= s;
  v.y *= s;
  v.z *= s;
  return v;
}

function V3F32 
v3f32_normalize(V3F32 v)
{
  return v3f32_scale(v, 1.f / v3f32_length(v));
}

function V3F32 
v3f32_lerp(V3F32 a, V3F32 b, F32 t)
{
  V3F32 v = {0};
  v.x = lerpf32(a.x, b.x, t);
  v.y = lerpf32(a.y, b.y, t);
  v.z = lerpf32(a.z, b.z, t);
  return v;
}

function V3F32 
v3f32_cross(V3F32 a, V3F32 b)
{
  V3F32 v = {0};
  v.x = a.y*b.z - a.z*b.y;
  v.y = a.z*b.x - a.x*b.z;
  v.z = a.x*b.y - a.y*b.x;
  return v;
}

function F32 
v3f32_dot(V3F32 a, V3F32 b)
{
  return a.x*b.x + a.y*b.y + a.z*b.z;
}

function F32 
v3f32_length(V3F32 v)
{
  return sqrtf32(v3f32_dot(v, v));
}

// V3U32

function V3U32 
v3u32(U32 x, U32 y, U32 z)
{
  V3U32 v = {0};
  v.x = x;
  v.y = y;
  v.z = z;
  return v; 
}

function V3U32 
v3u32_add(V3U32 a, V3U32 b)
{
  V3U32 v = {0};
  v.x = a.x + b.x;
  v.y = a.y + b.y;
  v.z = a.z + b.z;
  return v; 
}

function V3U32 
v3u32_sub(V3U32 a, V3U32 b)
{
  V3U32 v = {0};
  v.x = a.x - b.x;
  v.y = a.y - b.y;
  v.z = a.z - b.z;
  return v; 
}

function V3U32 
v3u32_mul(V3U32 a, V3U32 b)
{
  V3U32 v = {0};
  v.x = a.x * b.x;
  v.y = a.y * b.y;
  v.z = a.z * b.z;
  return v;
}

function V3U32 
v3u32_div(V3U32 a, V3U32 b)
{
  V3U32 v = {0};
  v.x = a.x / b.x;
  v.y = a.y / b.y;
  v.z = a.z / b.z;
  return v;
}

function V3U32 
v3u32_scale(V3U32 v, U32 s)
{
  v.x *= s;
  v.y *= s;
  v.z *= s;
  return v;
}

// V3S32

function V3S32 
v3s32(S32 x, S32 y, S32 z)
{
  V3S32 v = {0};
  v.x = x;
  v.y = y;
  v.z = z;
  return v; 
}

function V3S32 
v3s32_add(V3S32 a, V3S32 b)
{
  V3S32 v = {0};
  v.x = a.x + b.x;
  v.y = a.y + b.y;
  v.z = a.z + b.z;
  return v; 
}

function V3S32 
v3s32_sub(V3S32 a, V3S32 b)
{
  V3S32 v = {0};
  v.x = a.x - b.x;
  v.y = a.y - b.y;
  v.z = a.z - b.z;
  return v; 
}

function V3S32 
v3s32_mul(V3S32 a, V3S32 b)
{
  V3S32 v = {0};
  v.x = a.x * b.x;
  v.y = a.y * b.y;
  v.z = a.z * b.z;
  return v;
}

function V3S32 
v3s32_div(V3S32 a, V3S32 b)
{
  V3S32 v = {0};
  v.x = a.x / b.x;
  v.y = a.y / b.y;
  v.z = a.z / b.z;
  return v;
}

function V3S32 
v3s32_scale(V3S32 v, S32 s)
{
  v.x *= s;
  v.y *= s;
  v.z *= s;
  return v;
}

//
// Four-dimensional vectors
//

function V4F32 
v4f32(F32 x, F32 y, F32 z, F32 w)
{
  V4F32 v = {0};
  v.x = x;
  v.y = y;
  v.z = z;
  v.w = w;
  return v;
}

function V4F32 
v4f32_add(V4F32 a, V4F32 b)
{
  V4F32 v = {0};
  v.x = a.x + b.x;
  v.y = a.y + b.y;
  v.z = a.z + b.z;
  v.w = a.w + b.w;
  return v;
}

function V4F32 
v4f32_sub(V4F32 a, V4F32 b)
{
  V4F32 v = {0};
  v.x = a.x - b.x;
  v.y = a.y - b.y;
  v.z = a.z - b.z;
  v.w = a.w - b.w;
  return v;
}

function V4F32 
v4f32_mul(V4F32 a, V4F32 b)
{
  V4F32 v = {0};
  v.x = a.x * b.x;
  v.y = a.y * b.y;
  v.z = a.z * b.z;
  v.w = a.w * b.w;
  return v;
}

function V4F32 
v4f32_div(V4F32 a, V4F32 b)
{
  V4F32 v = {0};
  v.x = a.x / b.x;
  v.y = a.y / b.y;
  v.z = a.z / b.z;
  v.w = a.w / b.w;
  return v;
}

function V4F32 
v4f32_scale(V4F32 v, F32 s)
{
  v.x *= s;
  v.y *= s;
  v.z *= s;
  v.w *= s;
  return v;
}

function V4F32 
v4f32_normalize(V4F32 v)
{
  return v4f32_scale(v, 1.f / v4f32_length(v));
}

function V4F32 
v4f32_lerp(V4F32 a, V4F32 b, F32 t)
{
  V4F32 v = {0};
  v.x = lerpf32(a.x, b.x, t);
  v.y = lerpf32(a.y, b.y, t);
  v.z = lerpf32(a.z, b.z, t);
  v.w = lerpf32(a.w, b.w, t);
  return v;
}

function F32 
v4f32_dot(V4F32 a, V4F32 b)
{
  return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
}

function F32 
v4f32_length(V4F32 v)
{
  return sqrtf32(v4f32_dot(v, v));
}

//
// 4x4 Matrices
//

function Mat4x4 
m4x4_identity(void)
{
  return { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1, };
}

function Mat4x4 
m4x4_mul(Mat4x4 *a, Mat4x4 *b)
{
  Mat4x4 m = m4x4_identity();
  
  m.m00 = a->m00 * b->m00 + a->m01 * b->m10 + a->m02 * b->m20 + a->m03 * b->m30;
  m.m01 = a->m00 * b->m01 + a->m01 * b->m11 + a->m02 * b->m21 + a->m03 * b->m31;
  m.m02 = a->m00 * b->m02 + a->m01 * b->m12 + a->m02 * b->m22 + a->m03 * b->m32;
  m.m03 = a->m00 * b->m03 + a->m01 * b->m13 + a->m02 * b->m23 + a->m03 * b->m33;
  
  m.m10 = a->m10 * b->m00 + a->m11 * b->m10 + a->m12 * b->m20 + a->m13 * b->m30;
  m.m11 = a->m10 * b->m01 + a->m11 * b->m11 + a->m12 * b->m21 + a->m13 * b->m31;
  m.m12 = a->m10 * b->m02 + a->m11 * b->m12 + a->m12 * b->m22 + a->m13 * b->m32;
  m.m13 = a->m10 * b->m03 + a->m11 * b->m13 + a->m12 * b->m23 + a->m13 * b->m33;
  
  m.m20 = a->m20 * b->m00 + a->m21 * b->m10 + a->m22 * b->m20 + a->m23 * b->m30;
  m.m21 = a->m20 * b->m01 + a->m21 * b->m11 + a->m22 * b->m21 + a->m23 * b->m31;
  m.m22 = a->m20 * b->m02 + a->m21 * b->m12 + a->m22 * b->m22 + a->m23 * b->m32;
  m.m23 = a->m20 * b->m03 + a->m21 * b->m13 + a->m22 * b->m23 + a->m23 * b->m33;
  
  m.m30 = a->m30 * b->m00 + a->m31 * b->m10 + a->m32 * b->m20 + a->m33 * b->m30;
  m.m31 = a->m30 * b->m01 + a->m31 * b->m11 + a->m32 * b->m21 + a->m33 * b->m31;
  m.m32 = a->m30 * b->m02 + a->m31 * b->m12 + a->m32 * b->m22 + a->m33 * b->m32;
  m.m33 = a->m30 * b->m03 + a->m31 * b->m13 + a->m32 * b->m23 + a->m33 * b->m33;
  
  return m;
}

function Mat4x4 
m4x4_scale(Mat4x4 *m, F32 s)
{
  Mat4x4 ms = *m;
  
  ms.f[0]  *= s;
  ms.f[1]  *= s;
  ms.f[2]  *= s;
  ms.f[3]  *= s;
  ms.f[4]  *= s;
  ms.f[5]  *= s;
  ms.f[6]  *= s;
  ms.f[7]  *= s;
  ms.f[8]  *= s;
  ms.f[9]  *= s;
  ms.f[10] *= s;
  ms.f[11] *= s;
  ms.f[12] *= s;
  ms.f[13] *= s;
  ms.f[14] *= s;
  ms.f[15] *= s;
  
  return ms;
}

function Mat4x4 
m4x4_transpose(Mat4x4 *m)
{
  Mat4x4 t = *m; 
  
  t.m01 = m->m10;
  t.m02 = m->m20;
  t.m03 = m->m30;
  
  t.m10 = m->m01;
  t.m12 = m->m21;
  t.m13 = m->m31;
  
  t.m20 = m->m02;
  t.m21 = m->m12;
  t.m23 = m->m32;
  
  t.m30 = m->m03;
  t.m31 = m->m13;
  t.m32 = m->m23;
  
  return t;
}

function Mat4x4 
m4x4_inverse(Mat4x4 *m)
{
  V3F32 a = v3f32(m->m00, m->m01, m->m02);
  V3F32 b = v3f32(m->m10, m->m11, m->m12);
  V3F32 c = v3f32(m->m20, m->m21, m->m22);
  V3F32 d = v3f32(m->m30, m->m31, m->m32);
  
  F32 x = m->m03;
  F32 y = m->m13;
  F32 z = m->m23;
  F32 w = m->m33;
  
  V3F32 s = v3f32_cross(a, b);
  V3F32 t = v3f32_cross(c, d);
  V3F32 u = v3f32_sub(v3f32_scale(a, y), v3f32_scale(b, x));
  V3F32 v = v3f32_sub(v3f32_scale(c, w), v3f32_scale(d, z));
  
  F32 inv_det = 1.f / (v3f32_dot(s, v) + v3f32_dot(t, u));
  
  s = v3f32_scale(s, inv_det);
  t = v3f32_scale(t, inv_det);
  u = v3f32_scale(u, inv_det);
  v = v3f32_scale(v, inv_det);
  
  V3F32 m0 = v3f32_add(v3f32_cross(b, v), v3f32_scale(t, y));
  V3F32 m1 = v3f32_sub(v3f32_cross(v, a), v3f32_scale(t, x));
  V3F32 m2 = v3f32_add(v3f32_cross(d, u), v3f32_scale(s, w));
  V3F32 m3 = v3f32_sub(v3f32_cross(u, c), v3f32_scale(s, z));
  
  return {
    m0.x, m1.x, m2.x, m3.x, 
    m0.y, m1.y, m2.y, m3.y, 
    m0.z, m1.z, m2.z, m3.z, 
    -v3f32_dot(b, t), v3f32_dot(a, t), -v3f32_dot(d, s), v3f32_dot(c, s),
  };
}

function Mat4x4 
translation_m4x4(V3F32 t)
{
  Mat4x4 m = m4x4_identity();
  m.m03 = t.x;
  m.m13 = t.y;
  m.m23 = t.z;
  return m;
}

function Mat4x4 
rotation_m4x4(V3F32 r)
{
  // NOTE: Rotation is applied in Z, Y, X order.
  
  F32 rx = r.x;
  F32 ry = r.y;
  F32 rz = r.z; 
  
  F32 cx = cosf32(rx); 
  F32 cy = cosf32(ry); 
  F32 cz = cosf32(rz);
  F32 sx = sinf32(rx); 
  F32 sy = sinf32(ry); 
  F32 sz = sinf32(rz);
  
  Mat4x4 rot_x = {
    1,0,  0, 0,
    0,cx,-sx,0,
    0,sx, cx,0,
    0,0, 0,  1,
  };
  
  Mat4x4 rot_y = {
    cy,0,sy,0,
    0, 1,0, 0,
    -sy,0,cy,0,
    0, 0,0, 1,
  };
  
  Mat4x4 rot_z = {
    cz,-sz,0,0,
    sz, cz,0,0,
    0,  0, 1,0,
    0,  0, 0,1,
  };
  
  Mat4x4 m = m4x4_mul(&rot_y, &rot_x);
  m = m4x4_mul(&rot_z, &m);
  return m;
}

function Mat4x4 
scale_m4x4(V3F32 s)
{
  Mat4x4 m = m4x4_identity();
  m.m00 = s.x;
  m.m11 = s.y;
  m.m22 = s.z;
  return m;
}

function Mat4x4 
lookat_m4x4(V3F32 eye, V3F32 target, V3F32 up)
{
  V3F32 f = v3f32_normalize(v3f32_sub(eye, target)); 
  V3F32 r = v3f32_normalize(v3f32_cross(up, f)); 
  V3F32 u = v3f32_cross(f, r);
  
  F32 tx = -v3f32_dot(r, eye);
  F32 ty = -v3f32_dot(u, eye);
  F32 tz = -v3f32_dot(f, eye);
  
  Mat4x4 m = {
    r.x,r.y,r.z,tx,
    u.x,u.y,u.z,ty,
    f.x,f.y,f.z,tz,
    0,0,0,1,
  };
  
  return m;
}

function Mat4x4 
orthographic_m4x4(F32 l, F32 r, F32 b, F32 t, F32 n, F32 f)
{
  F32 x =  2.f / (r - l);
  F32 y =  2.f / (t - b);
  F32 z = -2.f / (f - n); // NOTE: Assumes -z forward, right-handed. 
  
  F32 tx = -(r + l) / (r - l);
  F32 ty = -(t + b) / (t - b);
  F32 tz = -(f + n) / (f - n);
  
  Mat4x4 m = {
    x,0,0,tx,
    0,y,0,ty,
    0,0,z,tz,
    0,0,0,1,
  };
  
  return m;
}

function Mat4x4 
perspective_m4x4(F32 fov, F32 aspect, F32 n, F32 f)
{
  F32 htf = tanf32(0.5f * fov);
  
  F32 py = 1.f / htf;
  F32 px = 1.f / (aspect * htf);
  F32 zr = n - f;
  
  F32 a = (-f - n) / zr;
  F32 b = (2.f * f * n) / zr;
  
  Mat4x4 m = {
    px,0,0,0,
    0,py,0,0,
    0,0, a,b,
    0,0, 1,0,
  };
  
  return m;
}

//
// Miscellaneous vector and matrix functions
//

function V4F32 
v4f32_transform(Mat4x4 *m, V4F32 v) 
{
  V4F32 r0 = m->r0;
  V4F32 r1 = m->r1;
  V4F32 r2 = m->r2;
  V4F32 r3 = m->r3;
  
  V4F32 result = {0};
  result.x = v4f32_dot(r0, v);
  result.y = v4f32_dot(r1, v);
  result.z = v4f32_dot(r2, v);
  result.w = v4f32_dot(r3, v);
  return result;
}

// Returns the corresponding world-space position of the provided screen-space 
// point, in the plane specified in the screen-space position's z-component. 
function V3F32 
unproject(V3F32 scrn, V4F32 viewport, Mat4x4 *modelview, Mat4x4 *proj)
{
  F32 xmin = viewport.x;
  F32 xmax = viewport.y;
  F32 ymin = viewport.z;
  F32 ymax = viewport.w;
  
  // Undo the viewport transform: Go from screen space's x=[0, WIDTH], 
  // y=[0, HEIGHT] to normalized device coordinates within [-1, 1].
  V4F32 ndc = {0};
  ndc.x = (scrn.x - xmin) / xmax * 2.f - 1.f;
  ndc.y = (scrn.y - ymin) / ymax * 2.f - 1.f;
  ndc.z = (scrn.z * 2.f - 1.f);
  ndc.w = 1.f;
  
  // Undo the the effects of multiplying by model, view, and proj matrices: 
  // Go from normalized device coordinates to model-space coordinates.
  Mat4x4 pv   = m4x4_mul(proj, modelview);
  Mat4x4 ipv  = m4x4_inverse(&pv);
  V4F32 model = v4f32_transform(&ipv, ndc);
  
  V3F32 result = {0};
  
  // Undo the homogeneous divide. 
  if (model.w) {
    F32 iw = 1.f / model.w;
    model.x *= iw;
    model.y *= iw;
    model.z *= iw;
    result = v3f32(model.x, model.y, model.z);
  }
  
  return result; 
}

// 
// Quaternions
// 

function Quat 
quat_from_f32(F32 x, F32 y, F32 z, F32 w)
{
  Quat q = {0};
  q.x = x;
  q.y = y;
  q.z = z;
  q.w = w;
  return q;
}

function Quat 
quat_from_v3f32_f32(V3F32 v, F32 s)
{
  Quat q = {0};
  q.v = v;
  q.s = s;
  return q;
}

function Quat 
quat_from_v4f32(V4F32 v)
{
  Quat q = {0};
  q.x = v.x;
  q.y = v.y;
  q.z = v.z;
  q.w = v.w;
  return q; 
}

function Quat 
quat_from_axis_angle(V3F32 axis, F32 angle)
{
  F32 s = sinf32(0.5f * angle);
  F32 c = cosf32(0.5f * angle);
  
  Quat q = {0};
  q.x = axis.x * s;
  q.y = axis.y * s;
  q.z = axis.z * s;
  q.w = c;
  return q;
}

function Quat 
quat_from_m4x4(Mat4x4 *m)
{
  Quat q;
  
  F32 trace = m->m00 + m->m11 + m->m22;
  if (trace > 0) 
  {
    F32 r = sqrtf32(trace + 1.f); 
    F32 inv = 0.5f / r;
    
    q.x = (m->m21 - m->m12) * inv;
    q.y = (m->m02 - m->m20) * inv;
    q.z = (m->m10 - m->m01) * inv;
    q.w = 0.5f * r;
  }
  else if ((m->m00 > m->m11) && (m->m00 > m->m22)) 
  {
    F32 r = sqrtf32(1.f + m->m00 - m->m11 - m->m22);
    F32 inv = 0.5f / r;
    
    q.x = 0.5f * r;
    q.y = (m->m01 + m->m10) * inv;
    q.z = (m->m02 + m->m20) * inv;
    q.w = (m->m21 - m->m12) * inv;
  }
  else if (m->m11 > m->m22) 
  {
    F32 r = sqrtf32(1.f + m->m11 - m->m00 - m->m22);
    F32 inv = 0.5f / r;
    
    q.x = (m->m01 + m->m10) * inv;
    q.y = 0.5f * r;
    q.z = (m->m12 + m->m21) * inv;
    q.w = (m->m02 - m->m20) * inv;
  }
  else 
  {
    F32 r = sqrtf32(1.f + m->m22 - m->m00 - m->m11);
    F32 inv = 0.5f / r;
    
    q.x = (m->m02 + m->m20) * inv;
    q.y = (m->m12 + m->m21) * inv;
    q.z = 0.5f * r;
    q.w = (m->m10 - m->m01) * inv;
  }
  
  return q;
}

function Quat 
quat_identity(void)
{
  Quat q = {0};
  q.v = {0,0,0};
  q.s = 1;
  return q;
}

function Quat 
quat_conjugate(Quat q)
{
  q.x = -q.x;
  q.y = -q.y;
  q.z = -q.z;
  return q;
}

function Quat 
quat_normalize(Quat q)
{
  F32 l = 1.f / quat_norm(q);
  q.v = v3f32_scale(q.v, l);
  q.s *= l;
  return q;
}

function Quat 
quat_inverse(Quat q)
{
  return quat_scale(quat_conjugate(q), 1.f / quat_norm(q));
}

function Quat 
quat_add(Quat a, Quat b)
{
  Quat q = {0};
  q.x = a.x + b.x;
  q.y = a.y + b.y;
  q.z = a.z + b.z;
  q.w = a.w + b.w;
  return q;
}

function Quat 
quat_sub(Quat a, Quat b)
{
  Quat q = {0};
  q.x = a.x - b.x;
  q.y = a.y - b.y;
  q.z = a.z - b.z;
  q.w = a.w - b.w;
  return q;
}

function Quat 
quat_mul(Quat a, Quat b)
{
  Quat q = {0};
  q.v = v3f32_add(
                  v3f32_add(v3f32_scale(b.v, a.w), v3f32_scale(a.v, b.w)), 
                  v3f32_cross(a.v, b.v)
                  );
  q.s = b.s*b.s - v3f32_dot(a.v, b.v);
  return q;
}

function Quat 
quat_scale(Quat q, F32 s)
{
  q.v = v3f32_scale(q.v, s);
  q.s *= s;
  return q;
}

function Quat 
quat_diff(Quat a, Quat b)
{
  Quat c = quat_conjugate(a);
  return quat_mul(c, b);
}

function Quat 
quat_exponentiate(Quat q, F32 exp)
{
  Quat r = q; 
  // Check for the case that q is an identity quaternion (i.e. 
  // w == +- 1). Identity quaternion { 0 1 } would cause a division 
  // by zero in `mult` computation, since sin(0/2), the denominator, 
  // would be 0.
  if (absf32(q.w) < 0.9999f) {
    // Extract the half angle alpha (alpha = theta/2)
    F32 alpha = acosf32(q.w);
    // Compute new alpha value
    F32 new_alpha = alpha * exp;
    // Compute new w value
    r.w = cosf32(new_alpha);
    // Compute new xyz values
    F32 mult = sinf32(new_alpha) / sinf32(alpha);
    r.x *= mult; 
    r.y *= mult; 
    r.z *= mult;
  }
  return r;
}

function Quat 
quat_slerp(Quat a, Quat b, F32 t)
{
  // Compute the cosine of the angle between the quaternions.
  F32 cos_omega = quat_dot(a, b); 
  
  // If negative dot, negate one of the input quaternions 
  // to use the shortest 4D "arc" (neighborhood check).
  if (cos_omega < 0) {
    a = quat_scale(a, -1.f); 
    cos_omega = -cos_omega;
  }
  
  // k0 and k1 are the constants needed to represent the two input 
  // quaternions as a linear combination of our desired quaternion 
  // (k0*a + k1*b = r).
  F32 k0, k1;
  
  // Check if the input quats are very close together (i.e. the
  // angle between them is close to 1) to protect against divide-by-zero.
  if (cos_omega > 0.9999f) {
    // Very close; just use linear interpolation (i.e. (1 - t)a + tb)
    k0 = 1.f - t; 
    k1 = t;
  }
  else {
    // Compute the sin of the angle between a, b using the
    // trig identity sin^2(omega) + cos^2(omega) = 1
    F32 sin_omega = sqrtf32(1.f - cos_omega * cos_omega);
    // Compute the angle between the two quaternions from its sine 
    // and cosine.
    F32 omega = atan2f32(sin_omega, cos_omega);
    // Compute interpolation parameters.
    F32 inv_sin_omega = 1.f / sin_omega;
    k0 = sinf32((1.f - t) * omega) * inv_sin_omega;
    k1 = sinf32(t * omega) * inv_sin_omega;
  }
  
  // Interpolate
  Quat r = {0};
  r.x = a.x * k0 + b.x * k1;
  r.y = a.y * k0 + b.y * k1;
  r.z = a.z * k0 + b.z * k1;
  r.w = a.w * k0 + b.w * k1;
  
  return r; 
}

function F32 
quat_norm(Quat q)
{
  return sqrtf32(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
}

function F32 
quat_dot(Quat a, Quat b)
{
  return v3f32_dot(a.v, b.v) + a.w*b.w; 
}

function V3F32 
axis_from_quat(Quat q)
{
  return v3f32_normalize(q.v);
}

function F32 
angle_from_quat(Quat q)
{
  F32 angle = 2.f * acosf32(q.w);
  return angle;
}

function Mat4x4 
m4x4_from_quat(Quat q)
{
  Mat4x4 m; 
  
  F32 x = q.x;
  F32 y = q.y;
  F32 z = q.z;
  F32 w = q.w;
  
  m.m00 = 1 - 2*y*y - 2*z*z;
  m.m01 = 2*x*y - 2*z*w;
  m.m02 = 2*x*z + 2*y*w;
  m.m03 = 0;
  
  m.m10 = 2*x*y + 2*z*w;
  m.m11 = 1 - 2*x*x - 2*z*z;
  m.m12 = 2*y*z - 2*x*w;
  m.m13 = 0;
  
  m.m20 = 2*x*z - 2*y*w;
  m.m21 = 2*y*z + 2*x*w;
  m.m22 = 1 - 2*x*x - 2*y*y;
  m.m23 = 0;
  
  m.m30 = 0;
  m.m31 = 0;
  m.m32 = 0;
  m.m33 = 1;
  
  return m;
}

function V3F32 
quat_rotate_v3f32(Quat q, V3F32 v)
{
  Quat vq = quat_from_v3f32_f32(v, 0.f);
  Quat iq = quat_conjugate(q);
  
  Quat r = quat_mul(quat_mul(q, vq), iq);
  return v3f32(r.x, r.y, r.z);
}

//
// Scale-translation-rotation structures (SRTs)
//

function SRT 
srt(V3F32 s, Quat r, V3F32 t)
{
  SRT srt = {0};
  srt.rotation    = r;
  srt.translation = t;
  srt.scale       = s;
  return srt;
}

function SRT 
srt_from_m4x4(Mat4x4 *m)
{
  Quat  q = quat_from_m4x4(m);
  V3F32 t = v3f32(m->m03, m->m13, m->m23);
  V3F32 s = v3f32(m->m00, m->m11, m->m22);
  
  // In the case that the matrix contains rotation, we can't just directly
  // extract the scale factors. We can use the lengths of the column vectors 
  // though.
  if (!(q.x && q.y && q.z) && (q.w == 1.f)) {
    V3F32 c0 = v3f32(m->m00, m->m10, m->m20);  
    V3F32 c1 = v3f32(m->m01, m->m11, m->m21);
    V3F32 c2 = v3f32(m->m02, m->m12, m->m22);
    
    F32 l0 = v3f32_length(c0);
    F32 l1 = v3f32_length(c1);
    F32 l2 = v3f32_length(c2);
    
    s = v3f32(l0, l1, l2);
  }
  
  SRT srt = {0};
  srt.rotation    = q;
  srt.translation = t;
  srt.scale       = s;
  
  return srt;
}

function SRT 
srt_lerp(SRT *a, SRT *b, F32 t)
{
  SRT srt = {0};
  srt.rotation    = quat_normalize(quat_slerp(a->rotation, b->rotation, t));
  srt.translation = v3f32_lerp(a->translation, b->translation, t);
  srt.scale       = v3f32_lerp(a->scale, b->scale, t);
  return srt;
}

function SRT 
srt_diff(SRT *a, SRT *b)
{
  SRT srt = {0};
  srt.rotation    = quat_normalize(quat_diff(a->rotation, b->rotation));
  srt.translation = v3f32_sub(a->translation, b->translation);
  srt.scale       = v3f32_div(a->scale, b->scale);
  return srt;
}

function Mat4x4 
m4x4_from_srt(SRT *srt)
{
  Mat4x4 r = m4x4_from_quat(quat_normalize(srt->rotation));
  Mat4x4 t = translation_m4x4(srt->translation);
  Mat4x4 s = scale_m4x4(srt->scale);
  
  Mat4x4 m = m4x4_mul(&r, &s);
  m = m4x4_mul(&t, &m);
  return m;
}

//
// Rects
//

// RectF32

function RectF32
rect_f32(F32 x0, F32 y0, F32 x1, F32 y1)
{
  RectF32 r = {0};
  r.x0 = x0;
  r.y0 = y0;
  r.x1 = x1;
  r.y1 = y1;
  return r; 
}

function V2F32
rect_f32_dim(RectF32 rect)
{
  F32 x = rect.x1 - rect.x0;
  F32 y = rect.y1 - rect.y0;
  
  return v2f32(x, y);
}

// RectU32

function RectU32
rect_u32(U32 x0, U32 y0, U32 x1, U32 y1)
{
  RectU32 r = {0};
  r.x0 = x0;
  r.y0 = y0;
  r.x1 = x1;
  r.y1 = y1;
  return r; 
}

function V2U32
rect_u32_dim(RectU32 rect)
{
  U32 x = rect.x1 - rect.x0;
  U32 y = rect.y1 - rect.y0;
  
  return v2u32(x, y);
}

