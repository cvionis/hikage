#pragma once

// TODO: It might be a good idea to give each job an input and output buffer 
// rather than a single input buffer (`data`); right now you have to save the
// output to some global data structure that is accessible by both the job and
// the main thread.

// TODO: Make sure threads are stopped gracefully when releasing the async layer.

typedef void ASYNC_JobProc(void *);

struct ASYNC_Job {
  ASYNC_JobProc *proc;
  void *data;
  U64 size;
};

struct ASYNC_Context {
  Arena *arena;
  
  OS_Handle semaphore;
  OS_Handle *threads;
  U32 threads_count;
  
  ASYNC_Job *queue;
  U32 queue_max;
  volatile U32 queue_count;
  volatile U32 next_read;
  volatile U32 next_write;
};

global ASYNC_Context *async_ctx;

function void async_thread_proc(void *param);

function void async_init(U32 thread_count, U32 queue_max);
function void async_release(void);
function void async_job_push(ASYNC_JobProc *proc, void *data, U64 size); 