// Define render pass inputs/helpers/behavior here!

struct R_GBufferPassData {
};

struct R_ShadowPassData {
};
// ...

// Each of these needs to allocate a pass in the frame's pass list; set the pass's pipeline, render targets, and reads/writes; and
// define the pass's input data and its execution procedure.

static void r_pass_add_gbuffer(R_Frame *frame);
static void r_pass_add_shadows(R_Frame *frame);
// ...

R_PASS_EXECUTE_PROC(r_pass_execute_gbuffer);
R_PASS_EXECUTE_PROC(r_pass_execute_shadows);
// ...
