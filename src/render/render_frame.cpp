//
// Render passes
//

static void
r_pass_begin(R_Pass *pass)
{
}

static void
r_pass_end(R_Pass *pass)
{
}

//
// Frames
//

static void
r_frame_begin(R_FrameData *frame)
{
  R_Context *ctx = &r_ctx;
  ctx->command_allocators[ctx->frame_idx]->Reset();
  ctx->command_list->Reset(ctx->command_allocators[ctx->frame_idx], 0);

  arena_clear(frame->pass_arena);
  arena_clear(frame->userdata_arena);

  frame->passes = ArenaPushArray(frame->pass_arena, R_Pass, 16);
  frame->passes_count = 0;

  frame->compiled_passes = ArenaPushArray(frame->pass_arena, R_CompiledPass, 16);
  frame->compiled_passes_count = 0;
  // ... reset cb allocator ...
}

static R_Pass *
r_frame_push_pass(R_FrameData *frame)
{
  R_Pass *result = &frame->passes[frame->passes_count];
  frame->passes_count += 1;
  return result;
}

// Determine transitions needed for resource dependencies, produce a list of compiled passes each with a list of
// barriers to issue before execution.
static void
r_frame_compile(R_FrameData *frame)
{
  // @Todo: Create barriers
  for (S32 pass_idx = 0; pass_idx < frame->passes_count; pass_idx += 1) {
    R_Pass *pass = &frame->passes[pass_idx];

    R_CompiledPass *compiled = &frame->compiled_passes[frame->compiled_passes_count];
    frame->compiled_passes_count += 1;

    compiled->pass = pass;
    compiled->barriers_count = 0;
  }
}

// Iterate over each pass, issuing its list of transition barriers, and calling pass_begin, execute, pass_end.
static void
r_frame_execute(R_FrameData *frame)
{
  for (S32 compiled_idx = 0; compiled_idx < frame->compiled_passes_count; compiled_idx += 1) {
    // @Todo: Issue barriers
    R_CompiledPass *compiled = &frame->compiled_passes[compiled_idx];
    R_Pass *pass = compiled->pass;

    r_pass_begin(pass);
    pass->execute(pass->userdata);
    r_pass_end(pass);
  }
}

static void r_d3d12_wait_for_previous_frame(void);

// Close and execute command lists, present
static void
r_frame_end(R_FrameData *frame)
{
  R_Context *ctx = &r_ctx;

  ctx->command_list->Close();
  ID3D12CommandList *lists[] = { ctx->command_list };
  ctx->command_queue->ExecuteCommandLists(1, lists);
  ctx->swapchain->Present(1, 0);

  r_d3d12_wait_for_previous_frame();
}
