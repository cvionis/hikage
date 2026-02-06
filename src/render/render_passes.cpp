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

  R_Context *ctx = &r_ctx;
  (void *)ctx;
  (void *)userdata;
}

static void
r_pass_add_forward(R_FrameData *frame, AssetContext *assets, ModelInstance *models, S32 models_count, Camera camera)
{
  R_Context *ctx = &r_ctx;

  R_Pass *pass = r_frame_push_pass(frame);
  pass->name = S8("forward");
  pass->pipeline = ctx->pipelines.forward; // @Note: Temporary

  // @Todo: Create resources and store handle in frame data (in main.cpp)

  // @Note: Used for rendering. Needs to match PSO desc.
  pass->color_targets[0] = frame->forward_color;
  pass->color_targets_count = 1;
  pass->depth_target = frame->forward_depth;

  // @Note: Used for barrier generation.
  pass->write_resources[0] = frame->forward_color;
  pass->write_count = 1;

  R_ForwardPassData *data = ArenaPushStruct(frame->userdata_arena, R_ForwardPassData);
  data->assets = assets;
  data->models = models;
  data->models_count = models_count;
  data->camera = camera;

  pass->userdata = data;
  pass->execute = r_pass_execute_forward;
}
