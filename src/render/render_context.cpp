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

// @Todo: Move
static R_Handle
r_current_back_buffer(void)
{
  R_Handle result = {};
  // @Note: Assumes that [0, frame_idx-1] of resource table consists of the back buffer textures.
  result.idx = r_ctx.frame_idx;
  return result;
}

static void
r_ctx_init_resources(R_Context *ctx)
{
  //
  // Pipelines
  //

  R_GraphicsPipelineDesc shadow_pipeline_desc = {
    .vs_path = L"../src/render/shaders/shadow.hlsl",
    .ps_path = 0,

    .input_layout = &r_mesh_layout,

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

    .rt_count = 0,

    .depth_format = R_Format_D32_Float,
    .sample_count = 1,
  };

  R_GraphicsPipelineDesc lighting_pipeline_desc = {
    .vs_path = L"../src/render/shaders/lighting.hlsl",
    .ps_path = L"../src/render/shaders/lighting.hlsl",

    .input_layout = &r_mesh_layout,

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

    .rt_formats = { R_Format_R16G16B16A16_Float },
    .rt_count = 1,

    .depth_format = R_Format_D32_Float,
    .sample_count = 1,
  };

  R_GraphicsPipelineDesc bloom_prefilter_pipeline_desc = {
    .vs_path = L"../src/render/shaders/fullscreen.hlsl",
    .ps_path = L"../src/render/shaders/bloom_0_prefilter.hlsl",
    .input_layout = 0,

    .raster = {
      .fill_mode = R_FillMode_Solid,
      .cull_mode = R_CullMode_Back,
      .front_ccw = 0,
      .depth_clip_enable = 1,
    },

    .depth_stencil = {
      .depth_enable = 0,
      .depth_write_enable = 0,
    },

    .blend = {
      .targets = {
        { .blend_enable = 0, .write_mask = 0xF },
      },
    },

    .topology = R_TopologyKind_Triangle,

    .rt_formats = {  R_Format_R16G16B16A16_Float },
    .rt_count = 1,

    .sample_count = 1,
  };

  R_GraphicsPipelineDesc bloom_downsample_pipeline_desc = {
    .vs_path = L"../src/render/shaders/fullscreen.hlsl",
    .ps_path = L"../src/render/shaders/bloom_1_downsample.hlsl",
    .input_layout = 0,

    .raster = {
      .fill_mode = R_FillMode_Solid,
      .cull_mode = R_CullMode_Back,
      .front_ccw = 0,
      .depth_clip_enable = 1,
    },

    .depth_stencil = {
      .depth_enable = 0,
      .depth_write_enable = 0,
    },

    .blend = {
      .targets = {
        { .blend_enable = 0, .write_mask = 0xF },
      },
    },

    .topology = R_TopologyKind_Triangle,

    .rt_formats = {  R_Format_R16G16B16A16_Float },
    .rt_count = 1,

    .sample_count = 1,
  };

  R_GraphicsPipelineDesc bloom_accumulate_pipeline_desc = {
    .vs_path = L"../src/render/shaders/fullscreen.hlsl",
    .ps_path = L"../src/render/shaders/bloom_2_accumulate.hlsl",
    .input_layout = 0,

    .raster = {
      .fill_mode = R_FillMode_Solid,
      .cull_mode = R_CullMode_Back,
      .front_ccw = 0,
      .depth_clip_enable = 1,
    },

    .depth_stencil = {
      .depth_enable = 0,
      .depth_write_enable = 0,
    },

    .blend = {
      .targets = {
        { .blend_enable = 0, .write_mask = 0xF },
      },
    },

    .topology = R_TopologyKind_Triangle,

    .rt_formats = {  R_Format_R16G16B16A16_Float },
    .rt_count = 1,

    .sample_count = 1,
  };

  R_GraphicsPipelineDesc composite_pipeline_desc = {
    .vs_path = L"../src/render/shaders/fullscreen.hlsl",
    .ps_path = L"../src/render/shaders/composite.hlsl",

    .input_layout = 0,

    .raster = {
      .fill_mode = R_FillMode_Solid,
      .cull_mode = R_CullMode_Back,
      .front_ccw = 0,
      .depth_clip_enable = 1,
    },

    .depth_stencil = {
      .depth_enable = 0,
      .depth_write_enable = 0,
    },

    .blend = {
      .targets = {
        { .blend_enable = 0, .write_mask = 0xF },
      },
    },

    .topology = R_TopologyKind_Triangle,

    .rt_formats = {  R_Format_R8G8B8A8_UNorm },
    .rt_count = 1,

    .sample_count = 1,
  };

  ctx->pipeline_lighting = r_create_graphics_pipeline(lighting_pipeline_desc);
  ctx->pipeline_shadow = r_create_graphics_pipeline(shadow_pipeline_desc);

  // @Todo: Re-include when shaders are written to prevent break on shader compilation error.
  #if 0
  ctx->pipeline_bloom_prefilter = r_create_graphics_pipeline(bloom_prefilter_pipeline_desc);
  ctx->pipeline_bloom_downsample = r_create_graphics_pipeline(bloom_downsample_pipeline_desc);
  ctx->pipeline_bloom_accumulate = r_create_graphics_pipeline(bloom_accumulate_pipeline_desc);
  #endif

  ctx->pipeline_composite = r_create_graphics_pipeline(composite_pipeline_desc);

  //
  // Textures
  //

  R_TextureDesc hdr_color_desc = {
    .width  = ctx->width,
    .height = ctx->height,
    .depth  = 1,
    .mips_count = 1,
    .fmt    = R_Format_R16G16B16A16_Float,
    .usage  = R_TextureUsage_RenderTarget|R_TextureUsage_Sampled,
    .kind   = R_TextureKind_2D,

    .init_state = R_ResourceState_RenderTarget,

    .has_clear_value = 1,
    .clear_color = { 0.4f, 0.5f, 1.1f, 1.0f },
  };

  R_TextureDesc lighting_depth_desc = {
    .width  = ctx->width,
    .height = ctx->height,
    .depth  = 1,
    .mips_count = 1,
    .fmt   = R_Format_D32_Float,
    .usage = R_TextureUsage_DepthStencil,
    .kind  = R_TextureKind_2D,

    .init_state = R_ResourceState_DepthWrite,

    .has_clear_value = 1,
    .clear_ds = {
      .depth   = 1.0f,
      .stencil = 0,
    },
  };

  S32 shadow_map_res = R_SHADOW_MAP_RESOLUTION;
  S32 shadow_cascades_count = R_SHADOW_CASCADE_COUNT;

  R_TextureDesc shadow_cascades_depth_desc = {
    .width  = shadow_map_res,
    .height = shadow_map_res,
    .depth  = shadow_cascades_count,
    .mips_count = 1,
    .fmt   = R_Format_D32_Float,
    .usage = R_TextureUsage_DepthStencil|R_TextureUsage_Sampled,
    .kind  = R_TextureKind_2D_Array,

    .init_state = R_ResourceState_DepthWrite,

    .has_clear_value = 1,
    .clear_ds = {
      .depth   = 1.0f,
      .stencil = 0,
    },
  };

  S32 bloom_tex_mips_count = 0;
  S32 bloom_tex_width = ctx->width / 2;
  S32 bloom_tex_height = ctx->height / 2;

  S32 w = bloom_tex_width;
  S32 h = bloom_tex_height;
  S32 bloom_tex_mip_dim_min = 4; // Stop at 4x4
  for (;;) {
    w = w >> 1;
    h = h >> 1;
    if (w < bloom_tex_mip_dim_min || h < bloom_tex_mip_dim_min) {
      break;
    }
    bloom_tex_mips_count += 1;
  }

  R_TextureDesc bloom_tex_desc = {
    .width = bloom_tex_width,
    .height = bloom_tex_height,
    .depth = 1,
    .mips_count = bloom_tex_mips_count,
    .fmt = R_Format_R16G16B16A16_Float,
    .usage = R_TextureUsage_RenderTarget|R_TextureUsage_Sampled,
    .kind = R_TextureKind_2D,

    .init_state = R_ResourceState_RenderTarget,

    .has_clear_value = 1,
    .clear_color = { 0.4f, 0.5f, 1.1f, 1.0f },
  };

  ctx->hdr_color = r_create_texture(0, 0, hdr_color_desc);
  ctx->lighting_depth = r_create_texture(0, 0, lighting_depth_desc);
  ctx->shadow_cascades_depth = r_create_texture(0, 0, shadow_cascades_depth_desc);

  #if 0
  ctx->bloom_tex_down = r_create_texture(0, 0, bloom_tex_desc);
  ctx->bloom_tex_up  = r_create_texture(0, 0, bloom_tex_desc);
  #endif
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

  R_D3D12_GraphicsPipeline *pipeline = (R_D3D12_GraphicsPipeline *)r_resource_table.slots[pass->pipeline.idx].backend_rsrc; // @Note: Temporary
  backend->command_list->SetPipelineState(pipeline->pso);
  Assert(pipeline->root_sig == backend->root_signature);
  backend->command_list->SetGraphicsRootSignature(pipeline->root_sig); // Use a single authoritive root signature for now

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
    ID3D12DescriptorHeap *heaps[] = { backend->srv_uav_heap };
    backend->command_list->SetDescriptorHeaps(1, heaps);

    D3D12_GPU_DESCRIPTOR_HANDLE gpu_base =
      backend->srv_uav_heap->GetGPUDescriptorHandleForHeapStart();

    D3D12_GPU_DESCRIPTOR_HANDLE gpu_tex_table = gpu_base;
    gpu_tex_table.ptr +=
      (U64)R_D3D12_TEXTURE_TABLE_BASE * (U64)backend->srv_uav_descriptor_size;
    backend->command_list->SetGraphicsRootDescriptorTable(2, gpu_tex_table);

    D3D12_GPU_DESCRIPTOR_HANDLE gpu_tex_2d_array_table = gpu_base;
    gpu_tex_2d_array_table.ptr +=
      (U64)R_D3D12_TEXTURE_TABLE_2D_ARRAY_BASE * (U64)backend->srv_uav_descriptor_size;
    backend->command_list->SetGraphicsRootDescriptorTable(3, gpu_tex_2d_array_table);

    D3D12_GPU_DESCRIPTOR_HANDLE gpu_uav_table = gpu_base;
    gpu_uav_table.ptr +=
      (U64)R_D3D12_UAV_BASE * (U64)backend->srv_uav_descriptor_size;
    backend->command_list->SetGraphicsRootDescriptorTable(4, gpu_uav_table);

    D3D12_GPU_DESCRIPTOR_HANDLE gpu_material =
    // @Todo: Replace material_srv_idx with R_D3D12_MATERIAL_TABLE_BASE
      CD3DX12_GPU_DESCRIPTOR_HANDLE(gpu_base, backend->material_srv_idx, backend->srv_uav_descriptor_size);
    backend->command_list->SetGraphicsRootDescriptorTable(5, gpu_material);
  }

  // Bind render targets

  B32 has_depth_target = r_resource_valid(pass->depth_target);

  D3D12_CPU_DESCRIPTOR_HANDLE rtv_handles[8] = {};
  for (S32 i = 0; i < pass->render_targets_count; ++i) {
    R_Handle render_target = pass->render_targets[i];
    R_ViewDesc desc = {
      .kind = R_ViewKind_RenderTarget,
      .fmt = r_texture_get_fmt(render_target),
      .range = {
        .mip_start = 0,
        .mip_count = 1,
        .slice_start = 0,
        .slice_count = 0,
      },
    };
    R_Handle view = r_view_from_texture(render_target, desc);
    rtv_handles[i] = r_d3d12_rtv_from_view(view);
  }

  D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle = {};
  if (has_depth_target) {
    R_ViewDesc desc = {
      .kind = R_ViewKind_DepthStencil,
      .fmt = R_Format_D32_Float,
      .range = {
        .mip_start = 0,
        .mip_count = 1,
        .slice_start = 0,
        .slice_count = 0,
      },
    };
    R_Handle view = r_view_from_texture(pass->depth_target, desc);
    dsv_handle = r_d3d12_dsv_from_view(view);
  }

  backend->command_list->OMSetRenderTargets(
    pass->render_targets_count,
    rtv_handles,
    FALSE,
    has_depth_target ? &dsv_handle : 0
  );

  // Clears

  if (pass->clear_flags & R_ClearFlag_Color) {
    for (S32 i = 0; i < pass->render_targets_count; i += 1) {
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
      pass->clear_depth, 0, 0, 0 // @Todo: Am I even using the depth texture's clear value? lol
    );
  }

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

  r_alloc_reset(&r_allocator);
}

static R_Pass *
r_frame_push_pass(R_Context *ctx)
{
  R_Pass *result = &ctx->passes[ctx->passes_count];
  ctx->passes_count += 1;
  return result;
}

// Determine transitions needed for resource dependencies, produce a list of compiled passes each with a list of
// transitions to issue before execution.
static void
r_frame_compile(R_Context *ctx)
{
  for (S32 pass_idx = 0; pass_idx < ctx->passes_count; pass_idx += 1) {
    R_Pass *pass = &ctx->passes[pass_idx];

    R_CompiledPass *compiled = &ctx->compiled_passes[ctx->compiled_passes_count];
    ctx->compiled_passes_count += 1;

    for (S32 ct_idx = 0; ct_idx < pass->render_targets_count; ct_idx += 1) {
      R_Handle render_target = pass->render_targets[ct_idx];

      U32 state_pre = r_resource_state(render_target);
      U32 state_mid = R_ResourceState_RenderTarget;
      U32 state_post = pass->color_final_state;

      if (state_pre != state_mid) {
        R_ResourceTransition *pre = &compiled->pre_transitions[compiled->pre_transitions_count];
        compiled->pre_transitions_count += 1;

        pre->rsrc = render_target;
        pre->state_before = state_pre;
        pre->state_after = state_mid;
      }

      if (state_mid != state_post) {
        R_ResourceTransition *post = &compiled->post_transitions[compiled->post_transitions_count];
        compiled->post_transitions_count += 1;

        post->rsrc = render_target;
        post->state_before = state_mid;
        post->state_after = state_post;
      }
    }

    // @Note: Treating uninitialized (zeroed) to mean "don't want to transition at all." Should do this check for render targets as well
    // for consistency sake.
    if (pass->depth_final_state) {
      R_Handle depth_target = pass->depth_target;

      U32 state_pre = r_resource_state(depth_target);
      U32 state_mid = R_ResourceState_DepthWrite;
      U32 state_post = pass->depth_final_state;

      if (state_pre != state_mid) {
        R_ResourceTransition *pre = &compiled->pre_transitions[compiled->pre_transitions_count];
        compiled->pre_transitions_count += 1;

        pre->rsrc = depth_target;
        pre->state_before = state_pre;
        pre->state_after = state_mid;
      }

      if (state_mid != state_post) {
        R_ResourceTransition *post = &compiled->post_transitions[compiled->post_transitions_count];
        compiled->post_transitions_count += 1;

        post->rsrc = depth_target;
        post->state_before = state_mid;
        post->state_after = state_post;
      }
    }

    compiled->pass = pass;
  }
}

// @Todo: Move
static D3D12_RESOURCE_BARRIER
r_d3d12_barrier_from_r_transition(R_ResourceTransition tr)
{
  D3D12_RESOURCE_BARRIER result = {};

  ID3D12Resource *d3d12_rsrc = r_d3d12_rsrc(tr.rsrc);
  result.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  result.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

  result.Transition.pResource = d3d12_rsrc;
  result.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  result.Transition.StateBefore = r_d3d12_state_from_r_state(tr.state_before);
  result.Transition.StateAfter = r_d3d12_state_from_r_state(tr.state_after);

  return result;
}

// @Todo: Move
static void
r_transition_resource(R_ResourceTransition tr)
{
  // Update CPU-side representation of resource state
  R_Handle h = tr.rsrc;
  R_ResourceSlot *slot = &r_resource_table.slots[h.idx];
  slot->state = tr.state_after;
  // Push transition barrier to the GPU command list
  R_D3D12_Backend *backend = &r_ctx;
  D3D12_RESOURCE_BARRIER barrier = r_d3d12_barrier_from_r_transition(tr);
  backend->command_list->ResourceBarrier(1, &barrier);
}

static void
r_frame_execute(R_Context *ctx)
{
  for (S32 compiled_idx = 0; compiled_idx < ctx->compiled_passes_count; compiled_idx += 1) {
    R_CompiledPass *compiled = &ctx->compiled_passes[compiled_idx];

    for (S32 pre_transition_idx = 0; pre_transition_idx < compiled->pre_transitions_count; pre_transition_idx += 1) {
      R_ResourceTransition tr = compiled->pre_transitions[pre_transition_idx];
      r_transition_resource(tr);
    }

    // @Todo: if you're going to pass the pass, then don't also pass a member. REDUNDANT.
    R_Pass *pass = compiled->pass;
    r_pass_begin(pass);
    pass->execute(pass, pass->userdata);
    r_pass_end(pass);

    for (S32 post_transition_idx = 0; post_transition_idx < compiled->post_transitions_count; post_transition_idx += 1) {
      R_ResourceTransition tr = compiled->post_transitions[post_transition_idx];
      r_transition_resource(tr);
    }
  }
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
