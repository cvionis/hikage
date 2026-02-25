// @Note: Temporary
#pragma warning(push, 0)
#define TINYEXR_USE_MINIZ 1
#define TINYEXR_USE_THREAD 1
#define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"
#include "miniz.c"
#pragma warning(pop)

#include "base/base_inc.h"
#include "os/os_inc.h"
#include "input/input_inc.h"
#include "async/async_inc.h"
#include "render/render_inc.h"
#include "asset/asset_inc.h"

#include "base/base_inc.cpp"
#include "os/os_inc.cpp"
#include "input/input_inc.cpp"
#include "async/async_inc.cpp"
#include "render/render_inc.cpp"
#include "asset/asset_inc.cpp"

struct AppState {
  Arena *arena;
  OS_Handle window;
  B32 quit;
};

struct ShadowCascadeBuild {
  // light view-projections for each cascade
  Mat4x4 viewproj[R_SHADOW_CASCADE_COUNT];
};

static ShadowCascadeBuild
build_shadow_cascades(Camera cam, V3F32 light_direction, F32 *cascade_splits, S32 cascade_count, S32 resolution)
{
  ShadowCascadeBuild res = {};

  // Constants to tweak according to scene scale
  const F32 light_back_off = 500.0f;
  const F32 z_pad = 50.0f;

  F32 half_fov = 0.5f * cam.fov;

  // Inverse of camera view (view -> world)
  Mat4x4 inv_view = m4x4_inverse(cam.view);

  // Normalize light direction
  light_direction = v3f32_normalize(light_direction);

  F32 aspect = cam.aspect;
  F32 zn_prev = cam.near_z;

  for (S32 i = 0; i < cascade_count; i += 1) {
    F32 zn = (i == 0) ? cam.near_z : cascade_splits[i - 1];
    F32 zf = cascade_splits[i];

    // 1) Frustum half sizes at zn and zf (view space, LH)
    F32 h_n = tanf32(half_fov) * zn;
    F32 w_n = h_n * aspect;
    F32 h_f = tanf32(half_fov) * zf;
    F32 w_f = h_f * aspect;

    // 2) 8 corners of cascade frustum in view space (+Z forward)
    V3F32 corners_vs[8] = {
      {-w_n, +h_n, zn}, {+w_n, +h_n, zn}, {+w_n, -h_n, zn}, {-w_n, -h_n, zn}, // near
      {-w_f, +h_f, zf}, {+w_f, +h_f, zf}, {+w_f, -h_f, zf}, {-w_f, -h_f, zf}, // far
    };

    // 3) Transform frustum corners to world space
    V3F32 corners_ws[8] = {};
    V3F32 center_ws = v3f32(0, 0, 0);

    for (U32 c = 0; c < 8; c += 1) {
      V4F32 v = v4f32(corners_vs[c].x, corners_vs[c].y, corners_vs[c].z, 1.0f);
      V4F32 w = v4f32_transform(inv_view, v);

      F32 iw = (w.w != 0.0f) ? 1.0f / w.w : 0.0f;
      corners_ws[c] = v3f32(w.x * iw, w.y * iw, w.z * iw);
      center_ws = v3f32_add(center_ws, corners_ws[c]);
    }

    center_ws = v3f32_scale(center_ws, 1.0f / 8.0f);

    // 4) Build light view from a position behind the cascade center
    V3F32 light_pos = v3f32_sub(center_ws, v3f32_scale(light_direction, light_back_off));

    // Choose up vector and get world->view for light in this cascade
    V3F32 up = v3f32(0, 1, 0);
    if (absf32(v3f32_dot(up, light_direction)) > 0.95f) {
      up = v3f32(0, 0, 1);
    }
    Mat4x4 light_view = lookat_m4x4(light_pos, center_ws, up);

    // 5) Fit orthographic bounds for this cascade's light projection
    F32 min_x = +FLT_MAX, min_y = +FLT_MAX, min_z = +FLT_MAX;
    F32 max_x = -FLT_MAX, max_y = -FLT_MAX, max_z = -FLT_MAX;

    for (U32 c = 0; c < 8; c += 1) {
      V4F32 lw = v4f32_transform(light_view, v4f32(corners_ws[c].x, corners_ws[c].y, corners_ws[c].z, 1.0f));
      min_x = fminf(min_x, lw.x); max_x = fmaxf(max_x, lw.x);
      min_y = fminf(min_y, lw.y); max_y = fmaxf(max_y, lw.y);
      min_z = fminf(min_z, lw.z); max_z = fmaxf(max_z, lw.z);
    }

    // 6) Texel stabilization
    F32 world_per_texel_x = (max_x - min_x) / (F32)resolution;
    F32 world_per_texel_y = (max_y - min_y) / (F32)resolution;

    F32 cx = 0.5f * (min_x + max_x);
    F32 cy = 0.5f * (min_y + max_y);
    F32 ex = 0.5f * (max_x - min_x);
    F32 ey = 0.5f * (max_y - min_y);

    cx = floorf(cx / world_per_texel_x) * world_per_texel_x;
    cy = floorf(cy / world_per_texel_y) * world_per_texel_y;

    min_x = cx - ex; max_x = cx + ex;
    min_y = cy - ey; max_y = cy + ey;

    // 7) Pad depth to avoid clipping
    min_z -= z_pad;
    max_z += z_pad;

    // 8) Create an orthographic projection for the light within this cascade
    Mat4x4 light_proj = orthographic_m4x4(min_x, max_x, min_y, max_y, min_z, max_z);

    // 9) Final light view-projection for this cascade
    res.viewproj[i] = m4x4_mul(light_proj, light_view);

    zn_prev = zf;
  }

  return res;
}

void
entry_point(void)
{
  os_init();
  os_gfx_init();

  S32 screen_w = 1280;
  S32 screen_h = 720;

  AppState app = {
    .arena = arena_alloc_default(),
    .window = os_window_open(S8("Kage"), screen_w, screen_h),
  };

  r_init(app.window);
  R_Context renderer = r_ctx_make(screen_w, screen_h);
  r_ctx_init_resources(&renderer);

  r_allocator = r_alloc_make(KiB(256));

  AssetContext assets = assets_make();
  assets_set_root_path(&assets, S8("R:/KageEngine/assets/models/"));
  AssetHandle a = assets_load_model(&assets, S8("Sponza"));

  int env_width;
  int env_height;
  float *env_data;
  {
    const char* input = "R:/KageEngine/assets/environments/citrus_orchard_road_puresky_4k.exr";
    const char* err = 0; // or nullptr in C++11

    int ret = LoadEXR(&env_data, &env_width, &env_height, input, &err);

    if (ret != TINYEXR_SUCCESS) {
      if (err) {
        fprintf(stderr, "ERR : %s\n", err);
        FreeEXRErrorMessage(err);
      }
    } else {
      free(env_data);
    }
  }

  {
    // Pipeline

    R_ComputePipelineDesc compute_pipeline_desc = {
      .cs_path = L"cubemap_from_env.hlsl",
    };
    R_Handle compute_pipeline = r_create_compute_pipeline(compute_pipeline_desc);

    // Textures

    S32 bytes_per_pixel = 16;
    S32 env_row_pitch   = env_width * bytes_per_pixel;
    S32 env_slice_pitch = env_height * env_row_pitch;
    S32 cube_dim = 2048; // @Note: Temp

    R_TextureDesc env_2d_desc = {
      .width       = env_width,
      .height      = env_height,
      .depth       = 1,
      .mips_count  = 1,
      .fmt         = R_Format_R32G32B32A32_Float,
      .usage       = R_TextureUsage_Sampled,
      .kind        = R_TextureKind_2D,
      .init_state  = R_ResourceState_ShaderRead_NP,
    };
    R_TextureInitData init = {
      .data = env_data,
      .row_pitch = env_row_pitch,
      .slice_pitch = env_slice_pitch,
    };
    R_Handle env_tex_2d = r_create_texture(&init, 1, env_2d_desc);

    R_ViewDesc env_tex_srv_desc = {
      .kind = R_ViewKind_ShaderResource,
      .fmt = R_Format_R32G32B32A32_Float,
      .range = {
        .mip_start = 0,
        .mip_count = 1,
        .slice_start = 0,
        .slice_count = 0,
      },
    };
    r_view_from_texture(env_tex_2d, env_tex_srv_desc);

    R_TextureDesc cubemap_desc = {
      .width =  cube_dim,
      .height = cube_dim,
      .depth = 6,
      .mips_count = 1,
      .fmt = R_Format_R16G16B16A16_Float,
      .usage = R_TextureUsage_Sampled|R_TextureUsage_UnorderedAccess,
      .kind = R_TextureKind_2D_Array,
      .init_state = R_ResourceState_UnorderedAccess,
    };
    R_Handle cubemap = r_create_texture(0, 0, cubemap_desc);

    R_ViewDesc cubemap_uav_desc = {
      .kind = R_ViewKind_UnorderedAccess,
      .fmt = R_Format_R16G16B16A16_Float,
      .range = {
        .mip_start = 0,
        .mip_count = 1,
        .slice_start = 0,
        .slice_count = 6,
      },
    };
    r_view_from_texture(cubemap, cubemap_uav_desc);

    // @Todo: Need to transition from unordered access to shader read to sample cubemap in shader later on.
    // @Resume: dispatch compute (+ root CBV and bind descriptor tables), figure out why compute shader is failing to compile (with no error msg...)

    struct CubemapFromEnvCB {
      U32 src_width;
      U32 src_height;
      U32 src_tex_idx;
      U32 dst_tex_idx;
    };
    R_Alloc alloc = r_alloc_push(&r_allocator, sizeof(CubemapFromEnvCB));
    auto *cb = (CubemapFromEnvCB *)alloc.cpu;
    cb->src_width = cube_dim;
    cb->src_height = cube_dim;
    cb->src_tex_idx = 0; // @Todo: Fill these out.
    cb->dst_tex_idx = 0;

    // Bind and dispatch

    // @Resume

    R_D3D12_Backend *backend = &r_ctx;
    ID3D12GraphicsCommandList* cl = backend->command_list;
    {
      auto *pipeline = (R_D3D12_ComputePipeline *)r_resource_table.slots[compute_pipeline.idx].backend_rsrc; // @Note: Temporary
      cl->SetPipelineState(pipeline->pso);
      cl->SetComputeRootSignature(backend->root_signature);

      // Descriptor heap
      ID3D12DescriptorHeap* heaps[] = { backend->srv_uav_heap };
      cl->SetDescriptorHeaps(1, heaps);

      // Bind root CBV (param 0)
      cl->SetGraphicsRootConstantBufferView(0, alloc.gpu);

      // Bind descriptor tables (same base handles you already compute)
      D3D12_GPU_DESCRIPTOR_HANDLE gpu_base =
        backend->srv_uav_heap->GetGPUDescriptorHandleForHeapStart();

      D3D12_GPU_DESCRIPTOR_HANDLE gpu_srv_tex_2d = gpu_base;
      gpu_srv_tex_2d.ptr +=
        (U64)R_D3D12_SRV_TEXTURE_2D_BASE * (U64)backend->srv_uav_descriptor_size;
      cl->SetComputeRootDescriptorTable(2, gpu_srv_tex_2d);

      D3D12_GPU_DESCRIPTOR_HANDLE gpu_uav_tex_2d_array = gpu_base;
      gpu_uav_tex_2d_array.ptr +=
        (U64)R_D3D12_UAV_TEXTURE_2D_ARRAY_BASE * (U64)backend->srv_uav_descriptor_size;
      backend->command_list->SetGraphicsRootDescriptorTable(5, gpu_uav_tex_2d_array);
    }

    {
      U32 gx = (cube_dim + 7) / 8;
      U32 gy = (cube_dim + 7) / 8;
      U32 gz = 6;
      cl->Dispatch(gx, gy, gz);
    }

  }

  Input input = {};

  // @Todo: Store models and camera in a minimal scene context
  ModelInstance models[SCENE_MODELS_COUNT] = {};
  S32 models_count = 0;
  {
    models[0].model = a;
    models[0].scale = v3f32(1.,1.,1.);
    models_count = 1;
  }
  Camera camera = {
    .position = v3f32(0,0.2f,-1),
    .direction = v3f32_normalize(v3f32_sub(v3f32(0,0,0), camera.position)),
    .near_z = 0.1f,
    .far_z = 1000.f,
    .fov = PI_F32/2,
  };

  {
    camera.position_target = camera.position;
    camera.pitch = asinf32(camera.direction.y);
    camera.yaw = atan2f32(camera.direction.x, camera.direction.z);
    camera.pitch_target = camera.pitch;
    camera.yaw_target = camera.yaw;
  }

  while (!app.quit) {
    OS_EventList *events = os_get_events();
    for (OS_Event *e = events->first; e != 0; e = e->next) {
      if (e->kind == OS_EventKind_WindowClose) {
        app.quit = 1;
      }
    }
    get_input(app.window, &input, events);

    static F64 prev_ticks = 0;
    F64 curr_ticks = os_get_ticks();
    F32 delta_time = (F32)(curr_ticks - prev_ticks) / (F32)os_get_ticks_frequency();
    prev_ticks = curr_ticks;

    // Update the camera
    F32 camera_move_speed = 3.f;
    F32 camera_look_speed = 2.2f;
    {
      V3F32 pos_delta = {};
      pos_delta.z += camera_move_speed * key_down(&input, Key_W);
      pos_delta.z -= camera_move_speed * key_down(&input, Key_S);
      pos_delta.x += camera_move_speed * key_down(&input, Key_D);
      pos_delta.x -= camera_move_speed * key_down(&input, Key_A);
      pos_delta.y += camera_move_speed * key_down(&input, Key_E);
      pos_delta.y -= camera_move_speed * key_down(&input, Key_Q);
      pos_delta = v3f32_scale(pos_delta, delta_time);

      F32 aspect = (F32)screen_w/(F32)screen_h;
      camera_update_position_aspect(&camera, pos_delta, aspect, delta_time);
    }
    {
      static S32 prev_x = input.mouse.x;
      static S32 prev_y = input.mouse.y;
      S32 dx = input.mouse.x - prev_x;
      S32 dy = input.mouse.y - prev_y;
      prev_x = input.mouse.x;
      prev_y = input.mouse.y;
      F32 yaw_delta   = delta_time * camera_look_speed *  (F32)dx;
      F32 pitch_delta = delta_time * camera_look_speed * -(F32)dy;

      if (mouse_down(&input, MouseButton_Right)) {
        camera_update_direction(&camera, yaw_delta, pitch_delta, delta_time);
      }
    }

    DirectionalLight sunlight {
      .direction = v3f32_normalize(v3f32(-0.4f, -0.8f, -0.1f)),
    };

    r_frame_begin(&renderer);

    // @Note: Temporary
    S32 cascade_count = R_SHADOW_CASCADE_COUNT;
    S32 resolution = R_SHADOW_MAP_RESOLUTION;
    F32 cascade_splits[R_SHADOW_CASCADE_COUNT] = { 10, 30, 80, 200 };
    ShadowCascadeBuild cascades = build_shadow_cascades(camera, sunlight.direction, cascade_splits, cascade_count, resolution);

    r_pass_add_shadow(&renderer, &assets, models, models_count, sunlight, camera, cascades.viewproj, cascade_splits);
    r_pass_add_lighting(&renderer, &assets, models, models_count, camera, cascades.viewproj, cascade_splits);
    #if 0
    r_pass_add_bloom_prefilter(&renderer);
    r_pass_add_bloom_downsample(&renderer);
    r_pass_add_bloom_accumulate(&renderer);
    #endif
    r_pass_add_composite(&renderer);
    r_frame_compile(&renderer);
    r_frame_execute(&renderer);

    r_frame_end(&renderer);
    r_alloc_reset(&r_allocator);
  }

  //r_alloc_release(&r_allocator);
  // @Todo: Release asset context
  r_ctx_release(&renderer);
  r_shutdown();
  os_window_close(app.window);
  arena_release(app.arena);
}
