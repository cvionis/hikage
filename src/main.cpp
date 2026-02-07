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

  AssetContext assets = assets_make();
  assets_set_root_path(&assets, S8("R:/KageEngine/assets/models/"));
  AssetHandle a = assets_load_model(&assets, S8("DamagedHelmet"));

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

    r_frame_begin(&renderer);

    r_pass_add_forward(&renderer, &assets, models, models_count, camera);

    r_frame_compile(&renderer);
    r_frame_execute(&renderer);

    r_frame_end(&renderer);

    #if 0
    // Render
    {
      R_D3D12_Backend *ctx = &r_ctx;

      // Begin frame
      {
        ctx->command_allocators[ctx->frame_idx]->Reset();
        ctx->command_list->Reset(ctx->command_allocators[ctx->frame_idx], 0);
      }

      // Do render passes
      r_render_forward(&assets, &camera, models, 1); // @Note: Temporary count. Track w/ global model list.

      // End frame
      {
        ctx->command_list->Close();
        ID3D12CommandList *lists[] = { ctx->command_list };
        ctx->command_queue->ExecuteCommandLists(1, lists);
        ctx->swapchain->Present(1, 0);
        r_d3d12_wait_for_previous_frame();
      }
    }
    #endif
  }

  // @Todo: Release asset context
  r_ctx_release(&renderer);
  r_shutdown();
  os_window_close(app.window);
  arena_release(app.arena);
}
