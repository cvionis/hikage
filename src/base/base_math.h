#pragma once

#include <math.h>

//
// Constants
//

#if OS_WINDOWS
# pragma warning(disable: 4310)
#endif

global S8  MIN_S8  = (S8) 0x80;
global S16 MIN_S16 = (S16)0x8000;
global S32 MIN_S32 = (S32)0x80000000;
global S64 MIN_S64 = (S64)0x8000000000000000llu;

#if OS_WINDOWS
#pragma warning(default: 4310)
#endif

global S8  MAX_S8  = (S8) 0x7f;
global S16 MAX_S16 = (S16)0x7fff;
global S32 MAX_S32 = (S32)0x7fffffff;
global S64 MAX_S64 = (S64)0x7fffffffffffffffllu;

global U8  MAX_U8  = 0xff;
global U16 MAX_U16 = 0xffff;
global U32 MAX_U32 = 0xffffffff;
global U64 MAX_U64 = 0xffffffffffffffffllu;

global F32 EPSILON_F32 = 1.1920929e-7f;
global F32 PI_F32 = 3.14159265359f;
global F32 TAU_F32 = 6.28318530718f;
global F32 E_F32 = 2.71828182846f;

global F64 EPSILON_F64 = 2.220446e-16;
global F64 PI_F64 = 3.14159265359;
global F64 TAU_F64 = 6.28318530718;
global F64 E_F64 = 2.71828182846;

//
// Macros
//

#define TO_RADIANS 0.01745f  // PI/180
#define TO_DEGREES 57.29577f // 180/PI

#define Radians(deg) (deg * TO_RADIANS)
#define Degrees(rad) (rad * TO_DEGREES)

#define Max(a, b) ((a) > (b) ? (a) : (b))
#define Min(a, b) ((a) < (b) ? (a) : (b))
#define Clamp(x, min, max) ((x) < (min) ? (min) : (x) > (max) ? (max) : (x))
#define Sign(n) ((n) > 0 ? 1 : -1)

//
// Basic math utilities
//

#define sinf32(x)      sinf((x))
#define cosf32(x)      cosf((x))
#define tanf32(x)      atanf((x))
#define asinf32(x)     asinf((x))
#define acosf32(x)     acosf((x))
#define atanf32(x)     atanf((x))
#define atan2f32(y, x) atan2f((y),(x))
#define sqrtf32(x)     sqrtf((x))
#define absf32(x)      fabsf((x))
#define powf32(x, exp) powf((x),(exp))
#define logf32(x)      logf((x))
#define modf32(x,y)    fmodf((x),(y))
#define floorf32(x)    floorf((x))
#define ceilf32(x)     ceilf((x))

function F32 lerpf32(F32 a, F32 b, F32 t);

//
// Easing functions
//

function F32 ease_quintic_inout(F32 t);
function F32 ease_expo_in(F32 t);
function F32 ease_circle_inout(F32 t);

//
// Axes
//

enum Axis2D {
  Axis2D_X,
  Axis2D_Y,
  Axis2D_COUNT,
};

enum Axis3D {
  Axis3D_X,
  Axis3D_Y,
  Axis3D_Z,
  Axis3D_COUNT,
};

//
// Two-dimensional vectors
//

// V2F32

union V2F32 {
  struct { F32 x, y; };
  struct { F32 u, v; };
  struct { F32 r, g; };
  F32 elements[2];
  F32 e[2];
};

function V2F32 v2f32(F32 x, F32 y);
function V2F32 v2f32_add(V2F32 a, V2F32 b);
function V2F32 v2f32_sub(V2F32 a, V2F32 b);
function V2F32 v2f32_mul(V2F32 a, V2F32 b);
function V2F32 v2f32_div(V2F32 a, V2F32 b);
function V2F32 v2f32_scale(V2F32 v, F32 s);
function V2F32 v2f32_normalize(V2F32 v);
function V2F32 v2f32_lerp(V2F32 a, V2F32 b, F32 t);

function F32 v2f32_dot(V2F32 a, V2F32 b);
function F32 v2f32_length(V2F32 v);
function F32 v2f32_angle(V2F32 v);

// V2U32

union V2U32 {
  struct { U32 x, y; };
  struct { U32 r, g; };
  struct { U32 u, v; };
  struct { U32 s, t; };
  U32 elements[2];
  U32 e[2];
};

function V2U32 v2u32(U32 x, U32 y);
function V2U32 v2u32_add(V2U32 a, V2U32 b);
function V2U32 v2u32_sub(V2U32 a, V2U32 b);
function V2U32 v2u32_mul(V2U32 a, V2U32 b);
function V2U32 v2u32_div(V2U32 a, V2U32 b);
function V2U32 v2u32_scale(V2U32 v, U32 s);

// V2S32

union V2S32 {
  struct { S32 x, y; };
  struct { S32 r, g; };
  struct { S32 u, v; };
  struct { S32 s, t; };
  S32 elements[2];
  S32 e[2];
};

function V2S32 v2s32(S32 x, S32 y);
function V2S32 v2s32_add(V2S32 a, V2S32 b);
function V2S32 v2s32_sub(V2S32 a, V2S32 b);
function V2S32 v2s32_mul(V2S32 a, V2S32 b);
function V2S32 v2s32_div(V2S32 a, V2S32 b);
function V2S32 v2s32_scale(V2S32 v, S32 s);

//
// Three-dimensional vectors
//

// V3F32

union V3F32 {
  struct { F32 x, y, z; };
  struct { F32 r, g, b; };
  F32 elements[3];
  F32 e[3];
};

function V3F32 v3f32(F32 x, F32 y, F32 z);
function V3F32 v3f32_add(V3F32 a, V3F32 b);
function V3F32 v3f32_sub(V3F32 a, V3F32 b);
function V3F32 v3f32_mul(V3F32 a, V3F32 b);
function V3F32 v3f32_div(V3F32 a, V3F32 b);
function V3F32 v3f32_scale(V3F32 v, F32 s);
function V3F32 v3f32_normalize(V3F32 v);
function V3F32 v3f32_lerp(V3F32 a, V3F32 b, F32 t);
function V3F32 v3f32_cross(V3F32 a, V3F32 b);

function F32 v3f32_dot(V3F32 a, V3F32 b);
function F32 v3f32_length(V3F32 v);

// V3U32

union V3U32 {
  struct { U32 x, y, z; };
  struct { U32 r, g, b; };
  U32 elements[3];
  U32 e[3];
};

function V3U32 v3u32(U32 x, U32 y, U32 z);
function V3U32 v3u32_add(V3U32 a, V3U32 b);
function V3U32 v3u32_sub(V3U32 a, V3U32 b);
function V3U32 v3u32_mul(V3U32 a, V3U32 b);
function V3U32 v3u32_div(V3U32 a, V3U32 b);
function V3U32 v3u32_scale(V3U32 v, U32 s);

// V3S32

union V3S32 {
  struct { S32 x, y, z; };
  struct { S32 r, g, b; };
  S32 elements[3];
  S32 e[3];
};

function V3S32 v3s32(S32 x, S32 y, S32 z);
function V3S32 v3s32_add(V3S32 a, V3S32 b);
function V3S32 v3s32_sub(V3S32 a, V3S32 b);
function V3S32 v3s32_mul(V3S32 a, V3S32 b);
function V3S32 v3s32_div(V3S32 a, V3S32 b);
function V3S32 v3s32_scale(V3S32 v, S32 s);

//
// Four-dimensional vectors
//

union V4F32 {
  struct { F32 x, y, z, w; };
  struct { F32 r, g, b, a; };
  F32 elements[4];
  F32 e[4];
};

function V4F32 v4f32(F32 x, F32 y, F32 z, F32 w);
function V4F32 v4f32_add(V4F32 a, V4F32 b);
function V4F32 v4f32_sub(V4F32 a, V4F32 b);
function V4F32 v4f32_mul(V4F32 a, V4F32 b);
function V4F32 v4f32_div(V4F32 a, V4F32 b);
function V4F32 v4f32_scale(V4F32 v, F32 s);
function V4F32 v4f32_normalize(V4F32 v);
function V4F32 v4f32_lerp(V4F32 a, V4F32 b, F32 t);

function F32 v4f32_dot(V4F32 a, V4F32 b);
function F32 v4f32_length(V4F32 v);

//
// 4x4 Matrices
//

// NOTE: Matrices are stored row-major
union Mat4x4 {
  struct {
    F32 m00, m01, m02, m03,
    m10, m11, m12, m13,
    m20, m21, m22, m23,
    m30, m31, m32, m33;
  };
  struct {
    V4F32 r0, r1, r2, r3;
  };

  F32 f[16];
  F32 elements[4][4];
  F32 e[4][4];
};

function Mat4x4 m4x4_identity(void);
function Mat4x4 m4x4_mul(Mat4x4 *a, Mat4x4 *b);
function Mat4x4 m4x4_scale(Mat4x4 *a, F32 s);
function Mat4x4 m4x4_transpose(Mat4x4 *m);
function Mat4x4 m4x4_inverse(Mat4x4 *m);

function Mat4x4 translation_m4x4(V3F32 t);
function Mat4x4 rotation_m4x4(V3F32 r);
function Mat4x4 scale_m4x4(V3F32 s);
function Mat4x4 lookat_m4x4(V3F32 eye, V3F32 target, V3F32 up);
function Mat4x4 orthographic_m4x4(F32 l, F32 r, F32 b, F32 t, F32 n, F32 f);
function Mat4x4 perspective_m4x4(F32 fov, F32 aspect, F32 n, F32 f);

//
// Miscellaneous vector and matrix functions
//

function V4F32 v4f32_transform(Mat4x4 *m, V4F32 v);
function V3F32 unproject(V3F32 scrn, V4F32 viewport, Mat4x4 *modelview, Mat4x4 *proj);

//
// Quaternions
//

union Quat {
  struct { F32  x, y, z, w; };
  struct { V3F32 v; F32 s; };
  F32 elements[4];
  F32 e[4];
};

function Quat quat_from_f32(F32 x, F32 y, F32 z, F32 w);
function Quat quat_from_v3f32_f32(V3F32 v, F32 s);
function Quat quat_from_v4f32(V4F32 v);
function Quat quat_from_axis_angle(V3F32 axis, F32 angle);
function Quat quat_from_m4x4(Mat4x4 *m);

function Quat quat_identity(void);
function Quat quat_conjugate(Quat q);
function Quat quat_normalize(Quat q);
function Quat quat_inverse(Quat q);

function Quat quat_add(Quat a, Quat b);
function Quat quat_sub(Quat a, Quat b);
function Quat quat_mul(Quat a, Quat b);
function Quat quat_scale(Quat q, F32 s);
function Quat quat_diff(Quat a, Quat b);
function Quat quat_exponentiate(Quat q, F32 exp);
function Quat quat_slerp(Quat a, Quat b, F32 t);

function F32 quat_norm(Quat q);
function F32 quat_dot(Quat a, Quat b);

function F32 angle_from_quat(Quat q);
function V3F32 axis_from_quat(Quat q);
function Mat4x4 m4x4_from_quat(Quat q);

function V3F32 quat_rotate_v3f32(Quat q, V3F32 v);

//
// Scale-translation-rotation structures (SRTs)
//

struct SRT {
  Quat rotation;
  V3F32 translation;
  V3F32 scale;
};

function SRT srt(V3F32 s, Quat r, V3F32 t);
function SRT srt_from_m4x4(Mat4x4 *m);
function SRT srt_lerp(SRT *a, SRT *b, F32 t);
function SRT srt_diff(SRT *a, SRT *b);

function Mat4x4 m4x4_from_srt(SRT *srt);

//
// Rects
//

// RectF32

union RectF32 {
  struct {
    F32 x0;
    F32 y0;
    F32 x1;
    F32 y1;
  };
  struct {
    V2F32 p0;
    V2F32 p1;
  };
};

function RectF32 rect_f32(F32 x0, F32 y0, F32 x1, F32 y1);
function V2F32   rect_f32_dim(RectF32 rect);

// RectU32

union RectU32 {
  struct {
    U32 x0;
    U32 y0;
    U32 x1;
    U32 y1;
  };
  struct {
    V2U32 p0;
    V2U32 p1;
  };
};

function RectU32 rect_u32(U32 x0, U32 y0, U32 x1, U32 y1);
function V2U32   rect_u32_dim(RectU32 rect);
