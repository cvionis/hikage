#pragma once

#include "os/core/os_core.h"
#include "os/gfx/os_gfx.h"

#if OS_WINDOWS
# define WIN32_LEAN_AND_MEAN
# pragma warning (push, 0)
# include <windows.h>
# pragma warning (pop)
# include "os/core/win32/os_core_win32.h"
# include "os/gfx/win32/os_gfx_win32.h"
#else #error OS layer not implemented on this platform.
#endif
