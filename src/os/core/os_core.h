#pragma once

struct OS_Handle {
  U64 h[1]; 
};

// 
// OS subsystem init
//

function void os_init(void);

//
// File IO
//

function String8 os_file_read(Arena *arena, String8 path);

// 
// System info
//

function U64 os_page_size(void);
function U64 os_logical_processor_count(void);

//
// Memory 
//

function void *os_reserve(U64 size);
function void os_commit(void *mem, U64 size);
function void os_decommit(void *mem, U64 size);
function void os_release(void *mem);

//
// Processes
//

function void os_exit_process(S32 exit_code);

//
// High-resolution performance counter
//

function F64 os_get_ticks(void);
function F64 os_get_ticks_frequency(void);

//
// Multithreading and synchronization (NOTE: Incomplete)
//

#define OS_WAIT_INFINITE 0xFFFFFFFF

typedef void os_thread_entry_point(void *);

function OS_Handle os_thread_launch(os_thread_entry_point *entry_point, void *param, U32 *id);
function void os_thread_delete(OS_Handle handle);

// Semaphores
function OS_Handle os_semaphore_create(U32 init_count, U32 max_count);
function void os_semaphore_delete(OS_Handle handle);
function B32 os_semaphore_wait(OS_Handle handle, U32 duration_ms);
function void os_semaphore_post(OS_Handle handle);

// Atomic operations
function U32 os_interlocked_compare_exchange_32(volatile U32 *dst, U32 exchange, U32 cmp);
function U32 os_interlocked_increment_32(volatile U32 *v);
function U32 os_interlocked_decrement_32(volatile U32 *v);

//
// Program entry point
//

void entry_point(void); 