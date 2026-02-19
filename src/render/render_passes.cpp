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
// Basic lighting render pass
//

struct R_ForwardPassData {
  AssetContext *assets;

  Camera camera;
  ModelInstance *models;
  S32 models_count;

  Mat4x4 *light_viewproj;
  F32 *cascade_splits;
};

R_PASS_EXECUTE_PROC(r_pass_execute_lighting)
{
  (void *)pass;

  struct FrameCB {
    Mat4x4 viewproj;
    Mat4x4 view;
    Mat4x4 light_viewproj[R_SHADOW_CASCADE_COUNT];
    F32 cascade_splits[R_SHADOW_CASCADE_COUNT]; // @Note: Maximum 4 splits
    V4F32 camera_ws;
  };

  R_D3D12_Backend *backend = &r_ctx;
  R_ForwardPassData *data = (R_ForwardPassData *)userdata;

  AssetContext *assets = (AssetContext *)data->assets;
  ModelInstance *models = (ModelInstance *)data->models;
  S32 models_count = data->models_count;
  Camera camera = data->camera;
  Mat4x4 *light_viewproj = data->light_viewproj;
  F32 *cascade_splits = data->cascade_splits;

  R_Alloc alloc = r_alloc_push(&r_allocator, sizeof(FrameCB));
  FrameCB *cb = (FrameCB *)alloc.cpu;
  cb->viewproj = camera.viewproj;
  cb->view = camera.view;
  cb->camera_ws = v4f32(camera.position.x, camera.position.y, camera.position.z, 0.f);
  MemoryCopy(cb->light_viewproj, light_viewproj, sizeof(Mat4x4) * R_SHADOW_CASCADE_COUNT);
  MemoryCopy(cb->cascade_splits, cascade_splits, sizeof(F32) * R_SHADOW_CASCADE_COUNT);

  backend->command_list->SetGraphicsRootConstantBufferView(0, alloc.gpu);

  r_draw_models(assets, models, models_count);
}

static void
r_pass_add_lighting(R_Context *ctx, AssetContext *assets, ModelInstance *models, S32 models_count, Camera camera,
  Mat4x4 *light_viewproj, F32 *cascade_splits)
{
  R_Pass *pass = r_frame_push_pass(ctx);
  pass->name = S8("lighting");
  pass->pipeline = ctx->pipeline_lighting;

  // @Note: Used for rendering. Needs to match PSO desc.
  R_Handle col_target = ctx->hdr_color; //r_current_back_buffer();
  pass->render_targets[0] = col_target;
  pass->render_targets_count = 1;
  pass->depth_target = ctx->lighting_depth;

  // @Note: Used for barrier generation.
  // @Todo: wrap in something like r_pass_add_read(R_Handle h), r_pass_add_write(R_Handle h).
  pass->write_resources[0] = col_target;
  pass->write_count = 1;

  pass->color_final_state = R_ResourceState_ShaderRead;
  // @Note: Don't care about depth state transitions yet;

  pass->viewport = ctx->default_viewport;
  pass->scissor = ctx->default_scissor;

  pass->clear_flags = R_ClearFlag_Color|R_ClearFlag_Depth;
  pass->clear_color = v4f32(0.4f, 0.5f, 1.1f, 1.f);
  pass->clear_depth = 1.0f;

  pass->topology = R_Topology_TriangleList;

  R_ForwardPassData *data = ArenaPushStruct(ctx->userdata_arena, R_ForwardPassData);
  data->assets = assets;
  data->models = models;
  data->models_count = models_count;
  data->camera = camera;
  data->light_viewproj = light_viewproj;
  data->cascade_splits = cascade_splits;

  pass->userdata = data;
  pass->execute = r_pass_execute_lighting;
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
  Mat4x4 *light_viewproj;
  F32 *cascade_splits;
};

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
  DirectionalLight light = data->light; // @Note: Not even used...
  Camera camera = data->camera;
  ModelInstance *models = data->models;
  S32 models_count = data->models_count;

  Mat4x4 *light_viewproj = data->light_viewproj;
  F32 *cascade_splits = data->cascade_splits;
  S32 cascade_count = R_SHADOW_CASCADE_COUNT;

  R_Handle shadow_cascades_depth = pass->depth_target;

  // Create shader resource view for shadow map array
  R_ViewDesc desc = {
    .kind = R_ViewKind_ShaderResource,
    .fmt = R_Format_R32_Float,
    .range = {
      .mip_start = 0,
      .mip_count = 1,
      .slice_start = 0,
      .slice_count = R_SHADOW_CASCADE_COUNT,
    },
  };
  r_view_from_texture(shadow_cascades_depth, desc);

  for (S32 cascade_idx = 0; cascade_idx < cascade_count; cascade_idx += 1) {
    R_Alloc alloc = r_alloc_push(&r_allocator, sizeof(ShadowFrameCB));
    ShadowFrameCB *cb = (ShadowFrameCB *)alloc.cpu;
    cb->viewproj = camera.viewproj;
    cb->camera_pos = v4f32(camera.position.x, camera.position.y, camera.position.z, 1.);
    MemoryCopy(cb->light_viewproj, light_viewproj, sizeof(Mat4x4) * cascade_count);
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
    backend->command_list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, pass->clear_depth, 0, 0, 0);

    r_draw_models(assets, models, models_count);
  }
}

static void
r_pass_add_shadow(R_Context *ctx, AssetContext *assets, ModelInstance *models, S32 models_count, DirectionalLight light, Camera camera,
  Mat4x4 *light_viewproj, F32 *cascade_splits)
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
  pass->clear_flags = 0; // @Note: We overrite this pass's clear in its execute() proc
  pass->topology = R_Topology_TriangleList;

  pass->userdata = 0;
  pass->execute = r_pass_execute_shadow;

  R_ShadowPassData *userdata = ArenaPushStruct(ctx->userdata_arena, R_ShadowPassData);
  userdata->assets = assets;
  userdata->light = light;
  userdata->camera = camera;
  userdata->models = models;
  userdata->models_count = models_count;
  userdata->light_viewproj = light_viewproj;
  userdata->cascade_splits = cascade_splits;

  pass->userdata = userdata;
}

//
// Bloom passes
//

// Bloom pass 0: prefilter
static void
r_pass_add_bloom_prefilter(R_Context *ctx)
{
  R_Pass *pass = r_frame_push_pass(ctx);
  pass->name = S8("bloom_prefilter");
  pass->pipeline = ctx->pipeline_bloom_prefilter;

  R_Handle target = ctx->bloom_tex_down;
  pass->render_targets[0] = target;
  pass->render_targets_count = 1;
  pass->depth_target = {-1,-1 }; // @Note: Temporary {0,0} refers to first backbuffer :0)
  //pass->depth_targets_count; // @Todo: JUST FUCKING ADD THIS TO CHECK IF IT HAS A DEPTH TARGET....

  // @Todo: wrap in something like r_pass_push_read(R_Pass *pass, R_Handle h), r_pass_push_write(R_Pass *pass, R_Handle h).
  pass->read_resources[0] = ctx->hdr_color;
  pass->read_count = 1;
  pass->write_resources[0] = target;
  pass->write_count = 1;

  pass->color_final_state = R_ResourceState_ShaderRead;

  pass->viewport = ctx->default_viewport;
  pass->scissor = ctx->default_scissor;

  pass->clear_flags = R_ClearFlag_Color;
  pass->clear_color = v4f32(0.4f, 0.5f, 1.1f, 1.f);

  pass->topology = R_Topology_TriangleList;

  pass->userdata = 0;
  pass->execute = 0;
}

// Bloom pass 1: downsample/blur
static void
r_pass_add_bloom_downsample(R_Context *ctx)
{
  R_Pass *pass = r_frame_push_pass(ctx);
  pass->name = S8("bloom_downsample");
  pass->pipeline = ctx->pipeline_bloom_downsample;

  R_Handle target = ctx->bloom_tex_down;
  pass->render_targets[0] = target;
  pass->render_targets_count = 1;
  pass->depth_target = {-1,-1 }; // @Note: Temporary {0,0} refers to first backbuffer :0)
  //pass->depth_targets_count; // @Todo: JUST FUCKING ADD THIS TO CHECK IF IT HAS A DEPTH TARGET....

  // @Todo: wrap in something like r_pass_push_read(R_Pass *pass, R_Handle h), r_pass_push_write(R_Pass *pass, R_Handle h).

  // @Resume: Either overwrite or adjust pass system so that rendering to individual mips is built in. See day28.txt
  pass->read_resources[0] = target;
  pass->read_count = 1;
  pass->write_resources[0] = target;
  pass->write_count = 1;

  pass->color_final_state = R_ResourceState_RenderTarget;

  pass->viewport = ctx->default_viewport;
  pass->scissor = ctx->default_scissor;

  pass->clear_flags = R_ClearFlag_Color;
  pass->clear_color = v4f32(0.4f, 0.5f, 1.1f, 1.f);

  pass->topology = R_Topology_TriangleList;

  pass->userdata = 0;
  pass->execute = 0;
}

// Bloom pass 2: upsample/accumulate
static void
r_pass_add_bloom_accumulate(R_Context *ctx)
{
  R_Pass *pass = r_frame_push_pass(ctx);
  pass->name = S8("bloom_accumulate");
  pass->pipeline = ctx->pipeline_bloom_accumulate;

  R_Handle target = ctx->bloom_tex_up;
  pass->render_targets[0] = target;
  pass->render_targets_count = 1;
  pass->depth_target = {-1,-1 }; // @Note: Temporary {0,0} refers to first backbuffer :0)
  //pass->depth_targets_count; // @Todo: JUST FUCKING ADD THIS TO CHECK IF IT HAS A DEPTH TARGET....

  // @Todo: wrap in something like r_pass_push_read(R_Pass *pass, R_Handle h), r_pass_push_write(R_Pass *pass, R_Handle h)

  // @Resume: Either overwrite or adjust pass system so that rendering to individual mips is built in. See day28.txt
  pass->read_resources[0] = ctx->bloom_tex_down;
  pass->read_count = 1;
  pass->write_resources[0] = target;
  pass->write_count = 1;

  pass->color_final_state = R_ResourceState_RenderTarget;

  pass->viewport = ctx->default_viewport;
  pass->scissor = ctx->default_scissor;

  pass->clear_flags = R_ClearFlag_Color;
  pass->clear_color = v4f32(0.4f, 0.5f, 1.1f, 1.f);

  pass->topology = R_Topology_TriangleList;

  pass->userdata = 0;
  pass->execute = 0;
}

//
// Compositing pass
//

R_PASS_EXECUTE_PROC(r_pass_execute_composite)
{
  (void *)userdata;

  struct R_CompositeCB {
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

  R_Alloc alloc = r_alloc_push(&r_allocator, sizeof(R_CompositeCB));
  R_CompositeCB *cb = (R_CompositeCB *)alloc.cpu;
  cb->tex_hdr_color = hdr_color_idx;
  backend->command_list->SetGraphicsRootConstantBufferView(0, alloc.gpu);

  backend->command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  backend->command_list->DrawInstanced(3, 1, 0, 0);
}

static void
r_pass_add_composite(R_Context *ctx)
{
  R_Pass *pass = r_frame_push_pass(ctx);
  pass->name = S8("composite");
  pass->pipeline = ctx->pipeline_composite;

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
  pass->clear_color = v4f32(0.4f, 0.5f, 1.1f, 1.f);

  pass->topology = R_Topology_TriangleList;

  pass->userdata = 0;
  pass->execute = r_pass_execute_composite;
}
