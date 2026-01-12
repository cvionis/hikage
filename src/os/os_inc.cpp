#include "os/core/os_core.cpp"
#include "os/gfx/os_gfx.cpp"

#if OS_WINDOWS
# pragma comment(lib, "gdi32")
# pragma comment(lib, "user32")
# include <stdlib.h> // NOTE: for __argc, __argv globals
# include "os/core/win32/os_core_win32.cpp"
# include "os/gfx/win32/os_gfx_win32.cpp"
#else
# error OS layer not implemented on this platform.
#endif
