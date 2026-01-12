#pragma once

struct OS_Win32_State {
  Arena *arena;
  HINSTANCE hinstance; // NOTE: Used by os/gfx/win32
  LARGE_INTEGER hrpc;
};

global OS_Win32_State os_win32_state;
