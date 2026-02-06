
// Pipelines (@Todo: put in render_core.h; this will be stored in context.)
// @Note: Not a fan of having a static set of pipelines like this, but it simplifies things for now (allows pass's execute procedures
// to set pipeline directly without having to pass a pipeline)

struct R_Pipelines {
  R_Handle gbuffer;
  R_Handle post;
  // ...
};

// Render passes

typedef void R_PassExecuteProc(R_Pass *pass);
#define R_PASS_EXECUTE_PROC(name) R_PassExecuteProc *name;

struct R_Pass {
  String8 name;
  R_Handle pipeline;

  // Attachments
  R_Handle color_targets[8];
  S32 color_target_count;
  R_Handle depth_target;

  // Resource dependencies
  R_Handle read_resources[16];
  S32 read_count;
  R_Handle write_resources[16];
  S32 write_count;

  // Pass-specific data blob and procedure
  void *user_data;
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

struct R_Frame {
  Arena *arena;

  // ... insert any shared resources built per-frame here (gbuffer, light buffer, etc.) .. //

  R_Pass *passes;
  R_CompiledPass *compiled_passes;
  S32 passes_count;
};

static void r_frame_begin(R_Frame *frame); // Reset arena, command lists
static R_Pass *r_frame_push_pass(R_Frame *frame); // Used internally by pass-specific builder functions (.e.g. r_pass_add_gbuffer(&frame))
static void r_frame_compile(R_Frame *frame); // Determine transitions needed for resource dependencies, create a list of barriers to issue for each pass.
static void r_frame_execute(R_Frame *frame); // Iterate over each pass, issuing its list of transition barriers, and calling pass_begin, execute, pass_end.
static void r_frame_end(R_Frame *frame); // Close and execute command lists, present

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
