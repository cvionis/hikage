#include "base/base_inc.h"
#include "os/os_inc.h"
#include "async/async_inc.h"
#include "asset/asset_inc.h"
#include "render/render_inc.h"

#include "base/base_inc.cpp"
#include "os/os_inc.cpp"
#include "async/async_inc.cpp"
#include "asset/asset_inc.cpp"
#include "render/render_inc.cpp"

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

  #if 0
  {
    AC_Builder ac = ac_make();
    // @Todo: Might want to pass an arena onto which to allocate the blob data, or give each blob its own arena.
    // If you call ac_release before you're completely done using the blob, the blob's memory is gonezo -- something
    // not immediately made clear in the API.
    AC_Blob blob = ac_load_model_blob_gltf(&ac, S8("R:/KageEngine/assets/models/DamagedHelmet/DamagedHelmet.gltf"));
    ac_cache_model_blob(&ac, blob);
    ac_release(&ac);
  }
  {
    AC_Blob blob = ac_load_model_blob_cached(app.arena, S8("R:/KageEngine/assets/cache/models/DamagedHelmet.mb"));
    U8 *data = (U8 *)blob.data;

    AC_Header *hdr = (AC_Header *)data;
    U32 mesh_table_off = hdr->mesh_table_off;
    U32 mtl_table_off = hdr->material_table_off;
    U32 tex_table_off = hdr->texture_table_off;
    U32 img_table_off = hdr->image_table_off;

    auto *mesh_table = (AC_MeshEntry *)(data + mesh_table_off);
    auto *mtl_table = (AC_MaterialEntry *)(data + mtl_table_off);
    auto *tex_table = (AC_TextureEntry *)(data + tex_table_off);
    auto *img_table = (AC_ImageEntry *)(data + img_table_off);
  }
  #endif

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
