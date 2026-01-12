//
// Win32-specific helpers
//

function HANDLE
os_win32_handle_from_handle(OS_Handle handle)
{
  HANDLE result = (HANDLE)handle.h[0];
  return result;
}

// 
// OS subsystem init
//

function void 
os_init(void)
{
  QueryPerformanceFrequency(&os_win32_state.hrpc);
  os_win32_state.arena = arena_alloc_default();
}

//
// File IO
//

#if 0
// NOTE: Adapted from Mr4thDimension's codebase
// (https://git.mr4th.com/mr4th-public/mr4th/src/branch/main/src/os)
function String8  
os_file_read(Arena *arena, String8 path)
{
  HANDLE file = CreateFile((char*)path.data, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
  String result = {0};
  
  if (file != INVALID_HANDLE_VALUE) {
    DWORD size_hi = 0;
    DWORD size_lo = GetFileSize(file, &size_hi);
    U64 file_size = (((U64)size_hi) << 32) | (U64)size_lo;
    
    U8 *buffer = ArenaPushArray(arena, U8, file_size);
    
    U8 *pos = buffer;
    U8 *opl = buffer + file_size; 
    B32 success = 1;
    
    while (pos < opl) {
      U64 total_to_read = (U64)(opl - pos);
      DWORD to_read = (DWORD)Min(total_to_read, MAX_U32);
      
      DWORD bytes_read = 0; 
      if (!ReadFile(file, pos, to_read, &bytes_read, 0)) {
        success = 0;
        break;
      }
      pos += bytes_read;
    }
    if (success) {
      result.data = buffer;
      result.count = file_size; 
    }
    
    CloseHandle(file);
  }
  
  return result; 
}
#endif

function String8 
os_file_read(Arena *arena, String8 path)
{
  U8 *data     = 0;
  U32 filesize = 0;
  
  FILE *file = fopen((char *)path.data, "rb");
  if (file) {
    fseek(file, 0, SEEK_END);
    filesize = ftell(file);
    rewind(file);
    data = ArenaPushArray(arena, U8, filesize + 1);
    fread(data, 1, filesize, file);
    data[filesize] = '\0';
    fclose(file);
  }
  
  String8 str = {0};
  str.data = data;
  str.count = filesize;
  return str;
}

//
// System info
//

function U64 
os_page_size(void) 
{
  SYSTEM_INFO sysinfo;
  GetSystemInfo(&sysinfo);
  return sysinfo.dwPageSize;
}

function U64 
os_logical_processor_count(void)
{
  SYSTEM_INFO sysinfo;
  GetSystemInfo(&sysinfo);
  return sysinfo.dwNumberOfProcessors;
}

//
// Memory
//

function void *
os_reserve(U64 size)
{
  U64 size_round_next_gb = AlignPow2(size, GiB(1));
  void *memory = VirtualAlloc(0, size_round_next_gb, MEM_RESERVE, PAGE_NOACCESS);
  return memory; 
}

function void 
os_commit(void *mem, U64 size) 
{
  U64 page_size = os_page_size();
  U64 size_round_next_page_size = AlignPow2(size, page_size); 
  VirtualAlloc(mem, size_round_next_page_size, MEM_COMMIT, PAGE_READWRITE);
}

function void 
os_decommit(void *mem, U64 size)
{
  VirtualFree(mem, size, MEM_DECOMMIT);
}

function void 
os_release(void *mem)
{
  VirtualFree(mem, 0, MEM_RELEASE);
}

//
// Processes
//

function void 
os_exit_process(S32 exit_code)
{
  ExitProcess(exit_code);
}

//
// High-resolution performance counter
//

function F64
os_get_ticks(void) 
{
  LARGE_INTEGER counter;
  QueryPerformanceCounter(&counter);
  return (F64)counter.QuadPart;
}

function F64
os_get_ticks_frequency(void)
{
  F64 freq = (F64)os_win32_state.hrpc.QuadPart;
  return freq;
}

//
// Multithreading and synchronization (NOTE: Incomplete)
//

// Threads

struct OS_Win32_ThreadEntryInfo {
  os_thread_entry_point *func;
  void *param;
};

function DWORD WINAPI
os_win32_thread_entry_point(void *param)
{
  OS_Win32_ThreadEntryInfo *entry = (OS_Win32_ThreadEntryInfo *)param;
  entry->func(entry->param);
  return 0;
}

function OS_Handle
os_thread_launch(os_thread_entry_point *entry_point, void *param, U32 *id)
{
  OS_Handle result = {0};
  
  Arena *arena = os_win32_state.arena;
  OS_Win32_ThreadEntryInfo *info = ArenaPushStruct(arena, OS_Win32_ThreadEntryInfo);
  
  info->func = entry_point;
  info->param = param;
  
  HANDLE handle = CreateThread(0, 0, os_win32_thread_entry_point, info, 0, (DWORD *)id);
  result.h[0] = (U64)handle;
  
  return result;
}

function void 
os_thread_delete(OS_Handle handle)
{
  HANDLE h = os_win32_handle_from_handle(handle);
  CloseHandle(h);
}

// Semaphores

function OS_Handle
os_semaphore_create(U32 init_count, U32 max_count)
{
  OS_Handle result = {0};
  
  HANDLE h = CreateSemaphore(0, init_count, max_count, 0);
  result.h[0] = (U64)h;
  
  return result;
}

function void
os_semaphore_delete(OS_Handle handle)
{
  HANDLE h = os_win32_handle_from_handle(handle);
  CloseHandle(h);
}

// TODO: Macro evaluating to INFINITE
function B32
os_semaphore_wait(OS_Handle handle, U32 duration_ms)
{
  HANDLE h = os_win32_handle_from_handle(handle);
  DWORD result = WaitForSingleObject(h, duration_ms);
  B32 signaled = (result == WAIT_OBJECT_0);
  return signaled;
}

function void
os_semaphore_post(OS_Handle handle)
{
  HANDLE h = os_win32_handle_from_handle(handle);
  ReleaseSemaphore(h, 1, 0);
}

// Atomic operations

function U32
os_interlocked_compare_exchange_32(volatile U32 *dst, U32 exchange, U32 cmp)
{
  U32 latest = _InterlockedCompareExchange((volatile long *)dst, exchange, cmp);
  return latest;
}

function U32 
os_interlocked_increment_32(volatile U32 *v)
{
  U32 prev = _InterlockedIncrement((volatile long *)v);
  return prev;
}

function U32 
os_interlocked_decrement_32(volatile U32 *v)
{
  U32 prev = _InterlockedDecrement((volatile long *)v);
  return prev;
}

//
// Program entry point
//


#if BUILD_CLI
int main(int argcount, char **arguments)
{
  (void)argcount;
  (void)arguments;
  
  os_win32_state.hinstance = GetModuleHandle(0);
  entry_point();
  
  return 0;
}
#else
int WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR lp_cmd_line, int n_show_cmd)
{
  os_win32_state.hinstance = instance;
  int argc = __argc;
  char **argv = __argv;
  
  (void)argc;
  (void)argv;
  (void)prev_instance;
  (void)lp_cmd_line;
  (void)n_show_cmd;
  
  entry_point();
  
  return 0;
}
#endif 
