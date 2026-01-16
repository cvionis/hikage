#pragma once

struct OS_Handle {
  U64 h[1];
};

//
// OS subsystem init
//

static void os_init(void);

//
// File IO
//

static String8 os_file_read(Arena *arena, String8 path);

//
// System info
//

static U64 os_page_size(void);
static U64 os_logical_processor_count(void);

//
// Memory
//

static void *os_reserve(U64 size);
static void os_commit(void *mem, U64 size);
static void os_decommit(void *mem, U64 size);
static void os_release(void *mem);

//
// Processes
//

static void os_exit_process(S32 exit_code);

//
// High-resolution performance counter
//

static F64 os_get_ticks(void);
static F64 os_get_ticks_frequency(void);

//
// Multithreading and synchronization (NOTE: Incomplete)
//

#define OS_WAIT_INFINITE 0xFFFFFFFF

typedef void os_thread_entry_point(void *);

static OS_Handle os_thread_launch(os_thread_entry_point *entry_point, void *param, U32 *id);
static void os_thread_delete(OS_Handle handle);

// Semaphores
static OS_Handle os_semaphore_create(U32 init_count, U32 max_count);
static void os_semaphore_delete(OS_Handle handle);
static B32 os_semaphore_wait(OS_Handle handle, U32 duration_ms);
static void os_semaphore_post(OS_Handle handle);

// Atomic operations
static U32 os_interlocked_compare_exchange_32(volatile U32 *dst, U32 exchange, U32 cmp);
static U32 os_interlocked_increment_32(volatile U32 *v);
static U32 os_interlocked_decrement_32(volatile U32 *v);

//
// Program entry point
//

void entry_point(void);
