//
// Render passes
//

static void
r_pass_begin(R_Pass *pass)
{
  R_Context *ctx = &r_ctx;

  // Bind pipeline + root signature

  R_D3D12_Pipeline *pipeline = (R_D3D12_Pipeline *)r_resource_table.slots[pass->pipeline.idx].backend_rsrc; // @Note: Temporary
  ctx->command_list->SetPipelineState(pipeline->pso);
  ctx->command_list->SetGraphicsRootSignature(ctx->root_signature); // Use a single authoritive root signature for now

  // Viewport & scissor

  RectF32 vp_rect = pass->viewport.rect;
  V2F32 vp_dim = rect_f32_dim(vp_rect);
  D3D12_VIEWPORT vp = {
    .TopLeftX = vp_rect.x0,
    .TopLeftY = vp_rect.y0,
    .Width    = vp_dim.x,
    .Height   = vp_dim.y,
    .MinDepth = pass->viewport.min_depth,
    .MaxDepth = pass->viewport.max_depth,
  };

  RectS32 sc_rect = pass->scissor.rect;
  V2S32 sc_dim = rect_s32_dim(sc_rect);
  D3D12_RECT sc = {
    .left   = sc_rect.x0,
    .top    = sc_rect.y0,
    .right  = sc_rect.x0 + sc_dim.x,
    .bottom = sc_rect.y0 + sc_dim.y,
  };

  ctx->command_list->RSSetViewports(1, &vp);
  ctx->command_list->RSSetScissorRects(1, &sc);

  // Descriptor heaps (bound once per pass)

  // Bind unified descriptor heap
  {
    ID3D12DescriptorHeap *heaps[] = { ctx->srv_heap };
    ctx->command_list->SetDescriptorHeaps(1, heaps);

    D3D12_GPU_DESCRIPTOR_HANDLE gpu_base =
      ctx->srv_heap->GetGPUDescriptorHandleForHeapStart();
    ctx->command_list->SetGraphicsRootDescriptorTable(0, gpu_base);

    D3D12_GPU_DESCRIPTOR_HANDLE gpu_tex = gpu_base;
    gpu_tex.ptr +=
      (U64)R_D3D12_TEXTURE_TABLE_BASE * (U64)ctx->srv_descriptor_size;
    ctx->command_list->SetGraphicsRootDescriptorTable(2, gpu_tex);

    D3D12_GPU_DESCRIPTOR_HANDLE gpu_material =
      CD3DX12_GPU_DESCRIPTOR_HANDLE(gpu_base, ctx->material_srv_idx, ctx->srv_descriptor_size);
    ctx->command_list->SetGraphicsRootDescriptorTable(3, gpu_material);
  }

  // Bind render targets

  B32 has_depth_target = 1; // @Todo: Determine from depth target handle.

  D3D12_CPU_DESCRIPTOR_HANDLE rtv_handles[8];
  for (S32 i = 0; i < pass->color_targets_count; ++i) {
    rtv_handles[i] =
      r_d3d12_rtv_from_texture(pass->color_targets[i]);
  }

  D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle = {};
  if (has_depth_target) {
    dsv_handle = r_d3d12_dsv_from_texture(pass->depth_target);
  }

  ctx->command_list->OMSetRenderTargets(
    pass->color_targets_count,
    rtv_handles,
    FALSE,
    has_depth_target ? &dsv_handle : 0
  );

  // Clears

  if (pass->clear_flags & R_ClearFlag_Color) {
    for (S32 i = 0; i < pass->color_targets_count; i += 1) {
      ctx->command_list->ClearRenderTargetView(
        rtv_handles[i],
        &pass->clear_color.e[0], 0, 0
      );
    }
  }

  if (has_depth_target && (pass->clear_flags & R_ClearFlag_Depth)) {
    ctx->command_list->ClearDepthStencilView(
      dsv_handle,
      D3D12_CLEAR_FLAG_DEPTH,
      pass->clear_depth, 0, 0, 0
    );
  }
}

static void
r_pass_end(R_Pass *pass)
{
  (void *)pass;
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
