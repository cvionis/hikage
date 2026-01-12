function void
async_thread_proc(void *param)
{
  (void *)param;
  
  ASYNC_Context *ctx = async_ctx;
  
  for (;;) {
    U32 queue_max = ctx->queue_max;
    U32 write = ctx->next_write;
    U32 read = ctx->next_read;
    
    if (read != write) {
      U32 next_read = read + 1;
      U32 latest_read = os_interlocked_compare_exchange_32(&ctx->next_read, next_read, read);
      
      if (latest_read == read) {
        ASYNC_Job *job = &ctx->queue[latest_read % queue_max];
        job->proc(job->data);
        
        os_interlocked_increment_32(&ctx->queue_count);
      }
    }
    else {
      B32 signaled = os_semaphore_wait(ctx->semaphore, OS_WAIT_INFINITE); 
      if (!signaled) {
        break;
      }
    }
  }
}

function void
async_init(U32 threads_count, U32 queue_max) 
{
  Arena *arena = arena_alloc_default();
  async_ctx = ArenaPushStruct(arena, ASYNC_Context);
  async_ctx->arena = arena;
  
  async_ctx->queue = ArenaPushArray(arena, ASYNC_Job, queue_max);
  async_ctx->queue_max = queue_max;
  
  async_ctx->semaphore = os_semaphore_create(0, threads_count);
  
  OS_Handle *threads = ArenaPushArray(arena, OS_Handle, threads_count);
  for (U32 idx = 0; idx < threads_count; idx += 1) {
    threads[idx] = os_thread_launch(async_thread_proc, 0, 0);
  }
  async_ctx->threads = threads;
  async_ctx->threads_count = threads_count;
}

function void
async_release(void)
{
  U32 threads_count = async_ctx->threads_count;
  OS_Handle *threads = async_ctx->threads;
  for (U32 idx = 0; idx < threads_count; idx += 1) {
    os_thread_delete(threads[idx]);
  }
  os_semaphore_delete(async_ctx->semaphore);
  
  arena_release(async_ctx->arena);
}

// TODO: Should copy data to job's data (which should be a static array with a good size limit in that case); this is 
// where size parameter would come in handy.
function void 
async_job_push(ASYNC_JobProc *proc, void *data, U64 size)
{
  ASYNC_Context *ctx = async_ctx;
  
  U32 queue_max = ctx->queue_max;
  U32 write = ctx->next_write;
  U32 next_write = write + 1;
  
  ASYNC_Job *job = &ctx->queue[write % queue_max];
  job->proc = proc;
  job->data = data; 
  job->size = size;
  
  ctx->next_write = next_write;
  os_interlocked_increment_32(&ctx->queue_count);
  
  os_semaphore_post(ctx->semaphore);
}