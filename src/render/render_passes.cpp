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

  struct R_DrawCB {
    Mat4x4 model;
    Mat4x4 normal;
    U32 material;
    U32 _pad[3];
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

      R_DrawCB draw_cb_data = {
        .model  = mmat,
        .normal = normal,
        .material = (U32)mesh->material,
      };

      R_Alloc alloc = r_alloc_push(&r_allocator, sizeof(R_DrawCB));
      R_DrawCB *cb = (R_DrawCB *)alloc.cpu;
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

R_PASS_EXECUTE_PROC(r_pass_execute_shadow)
{
  (void *)pass;
  R_D3D12_Backend *backend = &r_ctx;
  (void *)backend;
}

static void
r_pass_add_shadow(R_Context *ctx)
{
  R_Pass *pass = r_frame_push_pass(ctx);
  pass->name = S8("shadow");
  pass->pipeline = ctx->pipeline_shadow;

  #if 0
  pass->render_targets_count = 1;
  pass->read_resources[0] = ctx->hdr_color;
  pass->read_count = 1;
  pass->write_resources[0] = col_target;
  pass->write_count = 1;

  pass->color_final_state = R_ResourceState_Present;
  #endif

  pass->viewport = ctx->default_viewport;
  pass->scissor = ctx->default_scissor;

  pass->clear_flags = R_ClearFlag_Depth;
  pass->clear_color = v4f32(0.f,0.f, 0.f, 0.f);

  pass->topology = R_Topology_TriangleList;

  pass->userdata = 0;
  pass->execute = r_pass_execute_shadow;
}

//
// Post processing pass
//

R_PASS_EXECUTE_PROC(r_pass_execute_post)
{
  struct R_PostProcessCB {
    U32 tex_hdr_color;
  };

  R_D3D12_Backend *backend = &r_ctx;

  R_Handle hdr_color = pass->read_resources[0];  // @Note: Temporary
  R_ResourceSlot *slot = &r_resource_table.slots[hdr_color.idx]; // @Todo: Create helper for this.
  // @Todo: use new view_from_texture api.
  S32 hdr_color_idx = 0;// @Todo: slot->srv_idx - R_D3D12_TEXTURE_TABLE_BASE; // @Todo: Create helper for this.

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
