#include "base/base_inc.h"
#include "os/os_inc.h"
#include "async/async_inc.h"
#include "render/render_inc.h"
#include "asset/asset_inc.h"

#include "base/base_inc.cpp"
#include "os/os_inc.cpp"
#include "async/async_inc.cpp"
#include "render/render_inc.cpp"
#include "asset/asset_inc.cpp"

struct AppState {
  Arena *arena;
  OS_Handle window;
  B32 quit;
};

enum Key {
  Key_Null,
  Key_Esc,
  Key_Space,
  Key_Enter,
  Key_Up,
  Key_Left,
  Key_Down,
  Key_Right,
  Key_W,
  Key_A,
  Key_S,
  Key_D,
  Key_Q,
  Key_E,
  Key_R,
  Key_Z,
  Key_0,
  Key_1,
  Key_2,
  Key_3,
  Key_4,
  Key_5,
  Key_6,
  Key_7,
  Key_8,
  Key_9,
  Key_F1,
  Key_F2,
  Key_F3,
  Key_F4,
  Key_F5,
  Key_F6,
  Key_F7,
  Key_F8,
  Key_F9,
  Key_F10,
  Key_F11,
  Key_F12,
  Key_Minus,
  Key_COUNT,
};

enum MouseButton {
  MouseButton_Null,
  MouseButton_Left,
  MouseButton_Middle,
  MouseButton_Right,
  MouseButton_COUNT,
};

struct Input {
  B32 keys[Key_COUNT];
  struct {
    S32 x;
    S32 y;
    B32 buttons[MouseButton_COUNT];
  }mouse;
};

static void get_input(OS_Handle window, Input *input, OS_EventList *events);
static B32 key_pressed(Input *input, Key key);
static B32 key_down(Input *input, Key key);
static B32 mouse_pressed(Input *input, MouseButton btn);
static B32 mouse_down(Input *input, MouseButton btn);

static void
get_input(OS_Handle window, Input *input, OS_EventList *events)
{
  for (OS_Event *e = events->first; e != 0; e = e->next) {
    Key slot = Key_Null;
    MouseButton mouse_slot = MouseButton_Null;

    switch (e->key) {
      case OS_Key_Esc:   { slot = Key_Esc;   }break;
      case OS_Key_Space: { slot = Key_Space; }break;
      case OS_Key_Enter: { slot = Key_Enter; }break;
      case OS_Key_Up:    { slot = Key_Up;    }break;
      case OS_Key_Down:  { slot = Key_Down;  }break;
      case OS_Key_Left:  { slot = Key_Left;  }break;
      case OS_Key_Right: { slot = Key_Right; }break;
      case OS_Key_W:     { slot = Key_W;     }break;
      case OS_Key_A:     { slot = Key_A;     }break;
      case OS_Key_S:     { slot = Key_S;     }break;
      case OS_Key_D:     { slot = Key_D;     }break;
      case OS_Key_Q:     { slot = Key_Q;     }break;
      case OS_Key_E:     { slot = Key_E;     }break;
      case OS_Key_R:     { slot = Key_R;     }break;
      case OS_Key_Z:     { slot = Key_Z;     }break;
      case OS_Key_0:     { slot = Key_0;     }break;
      case OS_Key_1:     { slot = Key_1;     }break;
      case OS_Key_2:     { slot = Key_2;     }break;
      case OS_Key_3:     { slot = Key_3;     }break;
      case OS_Key_4:     { slot = Key_4;     }break;
      case OS_Key_5:     { slot = Key_5;     }break;
      case OS_Key_6:     { slot = Key_6;     }break;
      case OS_Key_7:     { slot = Key_7;     }break;
      case OS_Key_8:     { slot = Key_8;     }break;
      case OS_Key_9:     { slot = Key_9;     }break;
      case OS_Key_F1:    { slot = Key_F1;    }break;
      case OS_Key_F2:    { slot = Key_F2;    }break;
      case OS_Key_F3:    { slot = Key_F3;    }break;
      case OS_Key_F4:    { slot = Key_F4;    }break;
      case OS_Key_F5:    { slot = Key_F5;    }break;
      case OS_Key_F6:    { slot = Key_F6;    }break;
      case OS_Key_F7:    { slot = Key_F7;    }break;
      case OS_Key_F8:    { slot = Key_F8;    }break;
      case OS_Key_F9:    { slot = Key_F9;    }break;
      case OS_Key_F10:   { slot = Key_F10;   }break;
      case OS_Key_F11:   { slot = Key_F11;   }break;
      case OS_Key_F12:   { slot = Key_F12;   }break;
      case OS_Key_Minus: { slot = Key_Minus; }break;

      case OS_Key_MouseLeft:   { mouse_slot = MouseButton_Left;   }break;
      case OS_Key_MouseMiddle: { mouse_slot = MouseButton_Middle; }break;
      case OS_Key_MouseRight:  { mouse_slot = MouseButton_Right;  }break;
    }

    switch (e->kind) {
      case OS_EventKind_KeyPress:   { input->keys[slot] = 1; }break;
      case OS_EventKind_KeyRelease: { input->keys[slot] = 0; }break;

      case OS_EventKind_MousePress:   { input->mouse.buttons[mouse_slot] = 1; }break;
      case OS_EventKind_MouseRelease: { input->mouse.buttons[mouse_slot] = 0; }break;
    }
  }

  V2S32 mouse_pos = os_window_cursor_pos(window);
  input->mouse.x = mouse_pos.x;
  input->mouse.y = mouse_pos.y;
}

static B32
key_pressed(Input *input, Key key)
{
  B32 result = input->keys[key];
  input->keys[key] = 0;
  return result;
}

static B32
key_down(Input *input, Key key)
{
  return input->keys[key];
}

static B32
mouse_pressed(Input *input, MouseButton btn)
{
  B32 result = input->mouse.buttons[btn];
  input->mouse.buttons[btn] = 0;
  return result;
}

static B32
mouse_down(Input *input, MouseButton btn)
{
  return input->mouse.buttons[btn];
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

  AssetContext assets = assets_make();
  assets_set_root_path(&assets, S8("R:/KageEngine/assets/models/"));
  AssetHandle a = assets_load_model(&assets, S8("DamagedHelmet"));

  Input input = {};

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

  ModelTmp models[SCENE_MODELS_COUNT] = {0};
  {
    models[0].scale = v3f32(10,1,10);
    models[0].position = v3f32(0,-1,0);
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

      camera_update_position_aspect(&camera, pos_delta, 1920.f/1080.f, delta_time);
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

    // Render
    {
      R_Context *ctx = &r_ctx;

      // Begin frame
      {
        ctx->command_allocators[ctx->frame_idx]->Reset();
        ctx->command_list->Reset(ctx->command_allocators[ctx->frame_idx], 0);
      }

      // Do render passes
      r_render_forward(&camera, models);

      // End frame
      {
        ctx->command_list->Close();
        ID3D12CommandList *lists[] = { ctx->command_list };
        ctx->command_queue->ExecuteCommandLists(1, lists);
        ctx->swapchain->Present(1, 0);
        r_wait_for_previous_frame();
      }
    }
  }

  r_shutdown();
  os_window_close(app.window);
  arena_release(app.arena);
}
