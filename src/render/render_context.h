#pragma once

// @Todo: Put elsewhere.

enum R_ClearFlags {
  R_ClearFlag_None  = 0,
  R_ClearFlag_Color = (1 << 0),
  R_ClearFlag_Depth = (1 << 1),
};

struct R_Viewport {
  RectF32 rect;
  F32 min_depth;
  F32 max_depth;
};

struct R_Scissor {
  RectS32 rect;
};

// Render passes

typedef void R_PassExecuteProc(void *userdata);
#define R_PASS_EXECUTE_PROC(name) void name(void *userdata)

struct R_Pass {
  String8 name;
  R_Handle pipeline;

  // Attachments
  R_Handle color_targets[8];
  S32 color_targets_count;
  R_Handle depth_target;

  // Resource dependencies
  R_Handle read_resources[16];
  S32 read_count;
  R_Handle write_resources[16];
  S32 write_count;

  R_Viewport viewport;
  R_Scissor scissor;

  U32 clear_flags;
  V4F32 clear_color;
  F32 clear_depth;

  // Pass-specific data blob and procedure
  void *userdata;
  R_PassExecuteProc *execute;
};

static void r_pass_begin(R_Pass *pass);
static void r_pass_end(R_Pass *pass);

// Render frames

struct R_CompiledPass {
  R_Pass *pass;
  R_Handle barriers[16];
  S32 barriers_count;
};

// @Note: per-frame cb allocator can be stored in here as well potentially.
struct R_FrameData {
  Arena *pass_arena;
  Arena *userdata_arena;

  S32 passes_count;
  R_Pass *passes;
  S32 compiled_passes_count;
  R_CompiledPass *compiled_passes;

  // Defaults that can be used by passes
  R_Viewport default_viewport;
  R_Scissor default_scissor;

  // Resources built per-frame and shared by passes
  R_Handle forward_color;
  R_Handle forward_depth;
};

static void r_frame_begin(R_FrameData *frame); // Reset arena, command lists
static R_Pass *r_frame_push_pass(R_FrameData *frame); // Used internally by pass-specific builder functions (.e.g. r_pass_add_gbuffer(&frame))
static void r_frame_compile(R_FrameData *frame); // Determine transitions needed for resource dependencies, create a list of barriers to issue for each pass.
static void r_frame_execute(R_FrameData *frame); // Iterate over each pass, issuing its list of transition barriers, and calling pass_begin, execute, pass_end.
static void r_frame_end(R_FrameData *frame); // Close and execute command lists, present

/*
Example per-frame usage:

r_frame_begin(&frame);

r_pass_add_gbuffer(&frame);
r_pass_add_shadows(&frame);
r_pass_add_lighting(&frame);
r_pass_add_postprocess(&frame);

r_frame_compile(&frame);
r_frame_execute(&frame);

r_frame_end(&frame);
 */
