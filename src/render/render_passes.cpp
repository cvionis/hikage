//
// AAAAAAAAAAAAAAAAAAAAAAAAAAAA
//

// @Todo: Need to upload directional light and use that in shader rather than doing temporary sync between shader constant and value in
// CPU-side scene (not in HERE).



static void
r_draw_models(AssetContext *assets, ModelInstance *models, S32 models_count)
{
  R_D3D12_Backend *backend = &r_ctx;

  // Draw models
  for (S32 model_idx = 0; model_idx < models_count; model_idx += 1) {
    ModelInstance *m = &models[model_idx];
    Model *model = assets_get_model(assets, m->model);

    // @Todo: rotation
    Mat4x4 tr = translation_m4x4(m->position);
    Mat4x4 sc = scale_m4x4(m->scale);
    Mat4x4 mmat = m4x4_mul(sc, tr);

    Mat4x4 inv = m4x4_inverse(mmat);
    Mat4x4 normal = m4x4_transpose(inv);

    auto vertex_buffer_view = r_d3d12_vertex_buffer_view_from_buffer(model->vertex_buffer);
    auto index_buffer_view = r_d3d12_index_buffer_view_from_buffer(model->index_buffer);
    backend->command_list->IASetVertexBuffers(0, 1, &vertex_buffer_view);
    backend->command_list->IASetIndexBuffer(&index_buffer_view);

    for (S32 mesh_idx = 0; mesh_idx < model->meshes_count; mesh_idx += 1) {
      Mesh *mesh = &model->meshes[mesh_idx];

      struct DrawCB {
        Mat4x4 model;
        Mat4x4 normal;
        U32 material;
        U32 _pad[3];
      };

      R_Alloc alloc = r_alloc_push(&r_allocator, sizeof(DrawCB));
      DrawCB *cb = (DrawCB *)alloc.cpu;
      cb->model = mmat;
      cb->normal = normal;
      cb->material = (U32)mesh->material;
      backend->command_list->SetGraphicsRootConstantBufferView(1, alloc.gpu);

      S32 index_off = mesh->ib_off;
      S32 index_count = mesh->ib_count;

      backend->command_list->DrawIndexedInstanced(index_count, 1, index_off, mesh->vb_off, 0);
    }
  }
}

//
// Basic forward render pass
//

struct R_ForwardPassData {
  AssetContext *assets;

  ModelInstance *models;
  S32 models_count;

  Camera camera;
};

R_PASS_EXECUTE_PROC(r_pass_execute_forward)
{
  (void *)pass;

  struct R_FrameCB {
    Mat4x4 viewproj;
    V4F32  camera_ws;
  };

  R_D3D12_Backend *backend = &r_ctx;
  R_ForwardPassData *data = (R_ForwardPassData *)userdata;

  AssetContext *assets = (AssetContext *)data->assets;
  ModelInstance *models = (ModelInstance *)data->models;
  S32 models_count = data->models_count;
  Camera camera = data->camera;

  R_Alloc alloc = r_alloc_push(&r_allocator, sizeof(R_FrameCB));
  R_FrameCB *cb = (R_FrameCB *)alloc.cpu;
  cb->viewproj = camera.viewproj;
  cb->camera_ws = v4f32(camera.position.x, camera.position.y, camera.position.z, 0.f);
  backend->command_list->SetGraphicsRootConstantBufferView(0, alloc.gpu);

  r_draw_models(assets, models, models_count);
}

static void
r_pass_add_forward(R_Context *ctx, AssetContext *assets, ModelInstance *models, S32 models_count, Camera camera)
{
  R_Pass *pass = r_frame_push_pass(ctx);
  pass->name = S8("forward");
  pass->pipeline = ctx->pipeline_forward;

  // @Note: Used for rendering. Needs to match PSO desc.
  R_Handle col_target = ctx->hdr_color; //r_current_back_buffer();
  pass->render_targets[0] = col_target;
  pass->render_targets_count = 1;
  pass->depth_target = ctx->forward_depth;

  // @Note: Used for barrier generation.
  // @Todo: wrap in something like r_pass_add_read(R_Handle h), r_pass_add_write(R_Handle h).
  pass->write_resources[0] = col_target;
  pass->write_count = 1;

  pass->color_final_state = R_ResourceState_ShaderRead;
  // @Note: Don't care about depth state transitions yet;

  pass->viewport = ctx->default_viewport;
  pass->scissor = ctx->default_scissor;

  pass->clear_flags = R_ClearFlag_Color|R_ClearFlag_Depth;
  pass->clear_color = v4f32(0.95f,0.9f, 0.9f, 1.f);
  pass->clear_depth = 1.0f;

  pass->topology = R_Topology_TriangleList;

  R_ForwardPassData *data = ArenaPushStruct(ctx->userdata_arena, R_ForwardPassData);
  data->assets = assets;
  data->models = models;
  data->models_count = models_count;
  data->camera = camera;

  pass->userdata = data;
  pass->execute = r_pass_execute_forward;
}

//
// Shadow pass
//

struct R_ShadowPassData {
  AssetContext *assets;
  DirectionalLight light;
  Camera camera;
  ModelInstance *models;
  S32 models_count;
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

R_PASS_EXECUTE_PROC(r_pass_execute_shadow)
{
  struct ShadowFrameCB {
    Mat4x4 viewproj;
    Mat4x4 light_viewproj[R_SHADOW_CASCADE_COUNT];
    F32 cascade_splits[R_SHADOW_CASCADE_COUNT];
    U32 cascade_idx;
    U32 _pad[1];
    V4F32 camera_pos;
  };

  R_D3D12_Backend *backend = &r_ctx;
  R_ShadowPassData *data = (R_ShadowPassData *)userdata;

  AssetContext *assets = data->assets;
  DirectionalLight light = data->light;
  Camera camera = data->camera;
  ModelInstance *models = data->models;
  S32 models_count = data->models_count;

  F32 cascade_splits[R_SHADOW_CASCADE_COUNT] = { 1.1f, 4.3f, 16.6f, 100.f };
  S32 cascade_count = R_SHADOW_CASCADE_COUNT;
  S32 resolution = R_SHADOW_MAP_RESOLUTION;

  ShadowCascadeBuild cascades = build_shadow_cascades(camera, light.direction, cascade_splits, cascade_count, resolution);

  R_Handle shadow_cascades_depth = pass->depth_target;
  for (S32 cascade_idx = 0; cascade_idx < cascade_count; cascade_idx += 1) {
    R_Alloc alloc = r_alloc_push(&r_allocator, sizeof(ShadowFrameCB));
    ShadowFrameCB *cb = (ShadowFrameCB *)alloc.cpu;
    cb->viewproj = camera.viewproj;
    cb->camera_pos = v4f32(camera.position.x, camera.position.y, camera.position.z, 1.);
    MemoryCopy(cb->light_viewproj, cascades.viewproj, sizeof(Mat4x4) * cascade_count);
    MemoryCopy(cb->cascade_splits, cascade_splits, sizeof(F32) * cascade_count);
    cb->cascade_idx = cascade_idx;

    backend->command_list->SetGraphicsRootConstantBufferView(0, alloc.gpu);

    R_ViewDesc desc = {
      .kind = R_ViewKind_DepthStencil,
      .fmt = R_Format_D32_Float,
      .range = {
        .mip_start = 0,
        .mip_count = 1,
        .slice_start = cascade_idx,
        .slice_count = 1,
      },
    };
    R_Handle view = r_view_from_texture(shadow_cascades_depth, desc);
    D3D12_CPU_DESCRIPTOR_HANDLE dsv = r_d3d12_dsv_from_view(view);

    backend->command_list->OMSetRenderTargets(0, 0, FALSE, &dsv);
    backend->command_list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, 0);

    r_draw_models(assets, models, models_count);
  }
}

static void
r_pass_add_shadow(R_Context *ctx, AssetContext *assets, ModelInstance *models, S32 models_count, DirectionalLight light, Camera camera)
{
  S32 shadow_resolution = R_SHADOW_MAP_RESOLUTION;

  R_Pass *pass = r_frame_push_pass(ctx);
  pass->name = S8("shadow");
  pass->pipeline = ctx->pipeline_shadow;

  pass->depth_target = ctx->shadow_cascades_depth;
  pass->clear_depth = 1.f;

  pass->depth_final_state = R_ResourceState_ShaderRead;

  pass->viewport = {
    .rect = rect_f32(0.,0., (F32)shadow_resolution, (F32)shadow_resolution),
    .min_depth = 0.f,
    .max_depth = 1.f,
  };
  pass->scissor = {
    .rect = rect_s32(0,0, shadow_resolution, shadow_resolution),
  };
  pass->clear_flags = R_ClearFlag_Depth;
  pass->topology = R_Topology_TriangleList;

  pass->userdata = 0;
  pass->execute = r_pass_execute_shadow;

  R_ShadowPassData *userdata = ArenaPushStruct(ctx->userdata_arena, R_ShadowPassData);
  userdata->assets = assets;
  userdata->light = light;
  userdata->camera = camera;
  userdata->models = models;
  userdata->models_count = models_count;

  pass->userdata = userdata;
}

//
// Post processing pass
//

R_PASS_EXECUTE_PROC(r_pass_execute_post)
{
  (void *)userdata;

  struct R_PostProcessCB {
    U32 tex_hdr_color;
  };

  R_D3D12_Backend *backend = &r_ctx;

  R_Handle hdr_color_tex = pass->read_resources[0];  // @Note: Temporary

  R_ViewDesc view_desc = {
    .kind = R_ViewKind_ShaderResource,
    .fmt = r_texture_get_fmt(hdr_color_tex),
    .range = {
      .mip_start = 0,
      .mip_count = 1,
      .slice_start = 0,
      .slice_count = 0,
    },
  };
  R_Handle shader_resource_view = r_view_from_texture(hdr_color_tex, view_desc);
  S32 hdr_color_idx_abs = r_descriptor_idx_from_view(shader_resource_view);
  S32 hdr_color_idx = hdr_color_idx_abs - R_D3D12_TEXTURE_TABLE_BASE;

  R_Alloc alloc = r_alloc_push(&r_allocator, sizeof(R_PostProcessCB));
  R_PostProcessCB *cb = (R_PostProcessCB *)alloc.cpu;
  cb->tex_hdr_color = hdr_color_idx;
  backend->command_list->SetGraphicsRootConstantBufferView(0, alloc.gpu);

  backend->command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  backend->command_list->DrawInstanced(3, 1, 0, 0);
}

static void
r_pass_add_post(R_Context *ctx)
{
  R_Pass *pass = r_frame_push_pass(ctx);
  pass->name = S8("postprocess");
  pass->pipeline = ctx->pipeline_post;

  R_Handle col_target = r_current_back_buffer();
  pass->render_targets[0] = col_target;
  pass->render_targets_count = 1;
  pass->depth_target = {-1,-1}; // @Note: Temporary {0,0} refers to first backbuffer :0)
  //pass->depth_targets_count; // @Todo: JUST FUCKING ADD THIS TO CHECK IF IT HAS A DEPTH TARGET....

  // @Todo: wrap in something like r_pass_push_read(R_Pass *pass, R_Handle h), r_pass_push_write(R_Pass *pass, R_Handle h).
  pass->read_resources[0] = ctx->hdr_color;
  pass->read_count = 1;
  pass->write_resources[0] = col_target;
  pass->write_count = 1;

  pass->color_final_state = R_ResourceState_Present;

  pass->viewport = ctx->default_viewport;
  pass->scissor = ctx->default_scissor;

  pass->clear_flags = R_ClearFlag_Color;
  pass->clear_color = v4f32(0.95f,0.9f, 0.9f, 1.f);

  pass->topology = R_Topology_TriangleList;

  pass->userdata = 0;
  pass->execute = r_pass_execute_post;
}
