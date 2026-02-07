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
  R_D3D12_Backend *backend = &r_ctx;
  (void *)backend;
  (void *)userdata;
}

static void
r_pass_add_forward(R_Context *ctx, AssetContext *assets, ModelInstance *models, S32 models_count, Camera camera)
{
  R_Pass *pass = r_frame_push_pass(ctx);
  pass->name = S8("forward");
  pass->pipeline = ctx->pipeline_forward; // @Note: Temporary

  // @Note: Used for rendering. Needs to match PSO desc.
  pass->color_targets[0] = ctx->forward_color;
  pass->color_targets_count = 1;
  pass->depth_target = ctx->forward_depth;
  // @Note: Used for barrier generation.
  pass->write_resources[0] = ctx->forward_color;
  pass->write_count = 1;

  pass->viewport = ctx->default_viewport;
  pass->scissor = ctx->default_scissor;

  pass->clear_flags = (R_ClearFlag_Color | R_ClearFlag_Depth);
  pass->clear_color = v4f32(0.95f,0.9f, 0.9f, 1.f);
  pass->clear_depth = 1.0f;

  R_ForwardPassData *data = ArenaPushStruct(ctx->userdata_arena, R_ForwardPassData);
  data->assets = assets;
  data->models = models;
  data->models_count = models_count;
  data->camera = camera;

  pass->userdata = data;
  pass->execute = r_pass_execute_forward;
}
