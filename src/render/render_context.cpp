//
// User-facing rendering context
//

static R_Context
r_ctx_make(S32 screen_w, S32 screen_h)
{
  R_Context result = {
    .pass_arena = arena_alloc_default(),
    .userdata_arena = arena_alloc_default(),

    .width = screen_w,
    .height = screen_h,

    .default_viewport = {
      .rect = rect_f32(0, 0, (F32)screen_w, (F32)screen_h),
      .min_depth = 0.f,
      .max_depth = 1.f,
    },
    .default_scissor = {
      .rect = rect_s32(0, 0, screen_w, screen_h),
    },
  };

  return result;
}

static void
r_ctx_init_resources(R_Context *ctx)
{
  //
  // Pipelines
  //

  R_PipelineDesc forward_pipeline_desc = {
    .vs_path = L"../src/render/shaders/forward_basic.hlsl",
    .ps_path = L"../src/render/shaders/forward_basic.hlsl",

    .input_layout = mesh_layout,

    .raster = {
      .fill_mode = R_FillMode_Solid,
      .cull_mode = R_CullMode_Back,
      .front_ccw = 0,
      .depth_clip_enable = 1,
    },

    .depth_stencil = {
      .depth_enable = 1,
      .depth_write_enable = 1,
      .depth_compare = R_CompareOp_LessEqual,
    },

    .blend = {
      .targets = {
        { .blend_enable = 0, .write_mask = 0xF },
      },
    },

    .topology = R_TopologyKind_Triangle,

    // Backbuffer format
    .rt_formats = { R_Format_R8G8B8A8_UNorm },
    .rt_count = 1,

    .depth_format = R_Format_D32_Float,
    .sample_count = 1,
  };

  ctx->pipeline_forward = r_create_pipeline(forward_pipeline_desc);

  //
  // Textures
  //

  // @Resume: Test these

  R_TextureDesc color_desc = {
    .width  = ctx->width,
    .height = ctx->height,
    .depth  = 1,
    .mips_count = 1,
    .fmt    = R_Format_R8G8B8A8_UNorm,
    .usage  = R_TextureUsage_RenderTarget | R_TextureUsage_Sampled,
    .kind   = R_TextureKind_2D,

    .init_state = R_TextureInitState_RenderTarget,

    .has_clear_value = 1,
    .clear_color = { 0.95f, 0.9f, 0.9f, 1.0f },
  };

  R_TextureDesc depth_desc = {
    .width  = ctx->width,
    .height = ctx->height,
    .depth  = 1,
    .mips_count = 1,
    .fmt   = R_Format_D32_Float,
    .usage = R_TextureUsage_DepthStencil,
    .kind  = R_TextureKind_2D,

    .init_state = R_TextureInitState_DepthWrite,

    .has_clear_value = 1,
    .clear_ds = {
      .depth   = 1.0f,
      .stencil = 0,
    },
  };

  ctx->forward_color = r_create_texture(0, 0, color_desc);
  ctx->forward_depth = r_create_texture(0, 0, depth_desc);
}

static void
r_ctx_release(R_Context *ctx)
{
  if (ctx) {
    arena_release(ctx->pass_arena);
    arena_release(ctx->userdata_arena);
  }
  // @Todo: Release gpu resources
}

//
// Render passes
//

static void
r_pass_begin(R_Pass *pass)
{
  R_D3D12_Backend *backend = &r_ctx;

  // Bind pipeline + root signature

  R_D3D12_Pipeline *pipeline = (R_D3D12_Pipeline *)r_resource_table.slots[pass->pipeline.idx].backend_rsrc; // @Note: Temporary
  backend->command_list->SetPipelineState(pipeline->pso);
  Assert(pipeline->root_sig == backend->root_signature);
  backend->command_list->SetGraphicsRootSignature(backend->root_signature); // Use a single authoritive root signature for now

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

  backend->command_list->RSSetViewports(1, &vp);
  backend->command_list->RSSetScissorRects(1, &sc);

  // Bind unified descriptor heap
  {
    ID3D12DescriptorHeap *heaps[] = { backend->srv_heap };
    backend->command_list->SetDescriptorHeaps(1, heaps);

    D3D12_GPU_DESCRIPTOR_HANDLE gpu_base =
      backend->srv_heap->GetGPUDescriptorHandleForHeapStart();
    backend->command_list->SetGraphicsRootDescriptorTable(0, gpu_base);

    D3D12_GPU_DESCRIPTOR_HANDLE gpu_tex = gpu_base;
    gpu_tex.ptr +=
      (U64)R_D3D12_TEXTURE_TABLE_BASE * (U64)backend->srv_descriptor_size;
    backend->command_list->SetGraphicsRootDescriptorTable(2, gpu_tex);

    D3D12_GPU_DESCRIPTOR_HANDLE gpu_material =
      CD3DX12_GPU_DESCRIPTOR_HANDLE(gpu_base, backend->material_srv_idx, backend->srv_descriptor_size);
    backend->command_list->SetGraphicsRootDescriptorTable(3, gpu_material);
  }

  // Bind render targets

  B32 has_depth_target = r_texture_has_depth_stencil_view(pass->depth_target);

  R_ResourceState _state = r_resource_state(pass->color_targets[0]);

  D3D12_CPU_DESCRIPTOR_HANDLE rtv_handles[8] = {};
  for (S32 i = 0; i < pass->color_targets_count; ++i) {
    rtv_handles[i] =
      r_d3d12_rtv_from_texture(pass->color_targets[i]);
  }

  D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle = {};
  if (has_depth_target) {
    dsv_handle = r_d3d12_dsv_from_texture(pass->depth_target);
  }

  backend->command_list->OMSetRenderTargets(
    pass->color_targets_count,
    rtv_handles,
    FALSE,
    has_depth_target ? &dsv_handle : 0
  );

  // Clears

  if (pass->clear_flags & R_ClearFlag_Color) {
    for (S32 i = 0; i < pass->color_targets_count; i += 1) {
      backend->command_list->ClearRenderTargetView(
        rtv_handles[i],
        &pass->clear_color.e[0], 0, 0
      );
    }
  }

  if (has_depth_target && (pass->clear_flags & R_ClearFlag_Depth)) {
    backend->command_list->ClearDepthStencilView(
      dsv_handle,
      D3D12_CLEAR_FLAG_DEPTH,
      pass->clear_depth, 0, 0, 0
    );
  }

  // @Todo: Process barriers produced in r_frame_compile().

  // Input assembler

  D3D12_PRIMITIVE_TOPOLOGY d3d12_topology = r_d3d12_topology_from_r(pass->topology);
  backend->command_list->IASetPrimitiveTopology(d3d12_topology);
}

static void
r_pass_end(R_Pass *pass)
{
  (void *)pass;
}

//
// Per-frame rendering API
//

static void
r_frame_begin(R_Context *ctx)
{
  R_D3D12_Backend *backend = &r_ctx;
  backend->command_allocators[backend->frame_idx]->Reset();
  backend->command_list->Reset(backend->command_allocators[backend->frame_idx], 0);

  arena_clear(ctx->pass_arena);
  arena_clear(ctx->userdata_arena);

  ctx->passes = ArenaPushArray(ctx->pass_arena, R_Pass, 16);
  ctx->passes_count = 0;

  ctx->compiled_passes = ArenaPushArray(ctx->pass_arena, R_CompiledPass, 16);
  ctx->compiled_passes_count = 0;
  // ... reset cb allocator ...
}

static R_Pass *
r_frame_push_pass(R_Context *ctx)
{
  R_Pass *result = &ctx->passes[ctx->passes_count];
  ctx->passes_count += 1;
  return result;
}

// Determine transitions needed for resource dependencies, produce a list of compiled passes each with a list of
// barriers to issue before execution.
static void
r_frame_compile(R_Context *ctx)
{
  for (S32 pass_idx = 0; pass_idx < ctx->passes_count; pass_idx += 1) {
    R_Pass *pass = &ctx->passes[pass_idx];

    R_CompiledPass *compiled = &ctx->compiled_passes[ctx->compiled_passes_count];
    ctx->compiled_passes_count += 1;

    for (S32 ct_idx = 0; ct_idx < pass->color_targets_count; ct_idx += 1) {
      R_Handle color_target = pass->color_targets[ct_idx];

      R_ResourceState state_pre = r_resource_state(color_target);
      R_ResourceState state_mid = R_ResourceState_RenderTarget;
      R_ResourceState state_post = pass->color_final_state;

      if (state_pre != state_mid) {
        R_TransitionBarrier *pre = &compiled->pre_barriers[compiled->pre_barriers_count];
        compiled->pre_barriers_count += 1;

        pre->rsrc = color_target;
        pre->state_before = state_pre;
        pre->state_after = state_mid;
      }

      if (state_mid != state_post) {
        R_TransitionBarrier *post = &compiled->post_barriers[compiled->post_barriers_count];
        compiled->post_barriers_count += 1;

        post->rsrc = color_target;
        post->state_before = state_mid;
        post->state_after = state_post;
      }

      // @Todo: Read-resources transitions
      // @Todo: Depth state transition (when needed)
    }

    compiled->pass = pass;
  }
}

static void
r_d3d12_barriers_from_r(D3D12_RESOURCE_BARRIER *dst, R_TransitionBarrier *src, S32 count)
{
  for (S32 idx = 0; idx < count; idx += 1) {
    R_TransitionBarrier *src_barrier = &src[idx];
    ID3D12Resource *d3d12_rsrc = r_d3d12_rsrc(src_barrier->rsrc);

    D3D12_RESOURCE_BARRIER *b = &dst[idx];
    b->Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b->Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

    b->Transition.pResource = d3d12_rsrc;
    b->Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b->Transition.StateBefore = r_d3d12_state_from_r_state(src_barrier->state_before);
    b->Transition.StateAfter = r_d3d12_state_from_r_state(src_barrier->state_after);
  }
}

// Iterate over each pass, issuing its list of transition barriers, and calling pass_begin, execute, pass_end.
static void
r_frame_execute(R_Context *ctx)
{
  R_D3D12_Backend *backend = &r_ctx;
  TempArena tmp = arena_scratch_begin(0,0);

  for (S32 compiled_idx = 0; compiled_idx < ctx->compiled_passes_count; compiled_idx += 1) {
    R_CompiledPass *compiled = &ctx->compiled_passes[compiled_idx];
    R_Pass *pass = compiled->pass;

    if (compiled->pre_barriers_count) {
      D3D12_RESOURCE_BARRIER pre_barriers[16] = {};
      r_d3d12_barriers_from_r(pre_barriers, compiled->pre_barriers, compiled->pre_barriers_count);
      backend->command_list->ResourceBarrier((UINT)compiled->pre_barriers_count, pre_barriers);
    }

    // @Resume: when clearing rtv / dsv, state of texture is copy dest, not render target. Interesting.
    r_pass_begin(pass);
    pass->execute(pass->userdata);
    r_pass_end(pass);

    if (compiled->post_barriers_count) {
      D3D12_RESOURCE_BARRIER post_barriers[16] = {};
      r_d3d12_barriers_from_r(post_barriers, compiled->post_barriers, compiled->post_barriers_count);
      backend->command_list->ResourceBarrier((UINT)compiled->post_barriers_count, post_barriers);
    }
  }

  arena_scratch_end(tmp);
}

static void r_d3d12_wait_for_previous_frame(void);

// Close and execute command lists, present
static void
r_frame_end(R_Context *ctx)
{
  (void *)ctx;

  R_D3D12_Backend *backend = &r_ctx;

  backend->command_list->Close();
  ID3D12CommandList *lists[] = { backend->command_list };
  backend->command_queue->ExecuteCommandLists(1, lists);
  backend->swapchain->Present(1, 0);

  r_d3d12_wait_for_previous_frame();
}
