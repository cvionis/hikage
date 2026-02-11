#pragma once

#define R_FRAME_COUNT 2

// Render passes

struct R_Pass;
typedef void R_PassExecuteProc(R_Pass *pass, void *userdata);
#define R_PASS_EXECUTE_PROC(name) void name(R_Pass *pass, void *userdata)

struct R_Pass {
  String8 name;
  R_Handle pipeline;

  // Attachments
  R_Handle color_targets[8];
  S32 color_targets_count;
  R_Handle depth_target;

  R_ResourceState color_final_state; // @Note: All color targets share the same final state for now (covers most cases).
                                     // @Note: Tecnically writes_final_state...
  R_ResourceState depth_final_state; // @Todo: Actually use this (when needed; e.g. shadow pass)

  // Resource dependencies (@Todo: handle passes reading from outputs of previous passes)
  R_Handle read_resources[16];
  S32 read_count;
  R_Handle write_resources[16];
  S32 write_count;

  R_Viewport viewport;
  R_Scissor scissor;

  U32 clear_flags;
  V4F32 clear_color;
  F32 clear_depth;

  R_Topology topology;

  // Pass-specific data blob and procedure
  void *userdata;
  R_PassExecuteProc *execute;
};

// @Note: Not worrying about transitioning depth resources yet
struct R_CompiledPass {
  R_Pass *pass;
  S32 pre_transitions_count;
  R_ResourceTransition pre_transitions[16];
  S32 post_transitions_count;
  R_ResourceTransition post_transitions[16];
};

static void r_pass_begin(R_Pass *pass);
static void r_pass_end(R_Pass *pass);

// User-facing rendering context and per-frame drawing API

// @Todo: For persistent resources used across frames,
// consider making that application/usage dependent instead of hardcoding them here.
// I.e. the app using R_Context defines its own struct containing handles to these kinds of
// resources (pipelines, textures/render targets), and passes it as void *appdata to r_frame_X() API's.
// This allows resources to be added/removed/changed easily from main.cpp without digging into renderer files.
// An alternative would be to store a table of named {name, R_Handle} entries that the r_frame_X() API's can
// just look up by name.
struct R_Context {
  Arena *pass_arena;
  Arena *userdata_arena;

  S32 width;
  S32 height;

  // Persstent resources: pipelines
  R_Handle pipeline_forward;
  R_Handle pipeline_post;

  // Persistent resources: textures
  R_Handle forward_depth;

  R_Handle hdr_color;

  // Defaults
  R_Viewport default_viewport;
  R_Scissor default_scissor;

  // Render passes
  S32 passes_count;
  R_Pass *passes;

  S32 compiled_passes_count;
  R_CompiledPass *compiled_passes;
};

static R_Context r_ctx_make(S32 screen_w, S32 screen_h);
static void r_ctx_init_resources(R_Context *ctx);
static void r_ctx_release(R_Context *ctx);

static void r_frame_begin(R_Context *ctx); // Reset arena, command lists
static R_Pass *r_frame_push_pass(R_Context *ctx); // Used internally by pass-specific builder functions (.e.g. r_pass_add_gbuffer(&frame))
static void r_frame_compile(R_Context *ctx); // Determine transitions needed for resource dependencies, create a list of barriers to issue for each pass.
static void r_frame_execute(R_Context *ctx); // Iterate over each pass, issuing its list of transition barriers, and calling pass_begin, execute, pass_end.
static void r_frame_end(R_Context *ctx); // Close and execute command lists, present

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
