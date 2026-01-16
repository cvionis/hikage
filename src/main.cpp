#include "base/base_inc.h"
#include "os/os_inc.h"
#include "async/async_inc.h"
#include "asset/asset_inc.h"

#include "base/base_inc.cpp"
#include "os/os_inc.cpp"
#include "async/async_inc.cpp"
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

  AC_Builder ac = ac_make();
  {
    AC_Blob blob = ac_blob_from_gltf(&ac, S8("R:/KageEngine/assets/models/CommercialRefrigerator/CommercialRefrigerator.gltf"));
    ac_cache_model_blob(&ac, blob);
  }
  ac_release(&ac);

  for (;;) {
    OS_EventList *events = os_get_events();
    for (OS_Event *e = events->first; e != 0; e = e->next) {
      if (e->kind == OS_EventKind_WindowClose) {
        app.quit = 1;
      }
    }
    if (app.quit) {
      break;
    }
  }

  arena_release(app.arena);
}
