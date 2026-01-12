#define OS_GFX_WIN32_WINCLASS_NAME "GraphicalWindowClass"
#define OS_GFX_WIN32_KEYS_MAX 0xFF

struct OS_Win32_Window {
  HWND hwnd;
  V2U32 size;  
};

struct OS_Gfx_Win32_State {
  Arena *arena;
  OS_Win32_Window *window;
  
  F32 refresh_rate;
  OS_CursorKind cursor;
  void (*resize_callback)(void);
  
  /*
    TODO: Multiple windows
  
    OS_Win32_Window *windows_first;
    OS_Win32_Window *windows_last; 
  
    OS_Win32_Window *first_free_window;
  */
};

// 
// Globals 
//

global OS_Gfx_Win32_State os_gfx_win32_state; 
global OS_Key os_gfx_win32_key_table[OS_GFX_WIN32_KEYS_MAX];
global HCURSOR os_gfx_win32_cursor_table[OS_CursorKind_COUNT];

//
// Internal helpers
//

function OS_Modifiers 
os_win32_get_modifiers(void)
{
  U32 modifiers = 0;
  
  if (GetKeyState(VK_CONTROL) & (1 << 15)) {
    modifiers |= OS_Modifier_Ctrl;
  }
  if (GetKeyState(VK_SHIFT) & (1 << 15)) {
    modifiers |= OS_Modifier_Shift;
  }
  if (GetKeyState(VK_MENU) & (1 << 15)) {
    modifiers |= OS_Modifier_Alt;
  }
  
  return (OS_Modifiers)modifiers;
}

function OS_Handle 
os_win32_handle_from_window(OS_Win32_Window *window) 
{
  OS_Handle handle;
  handle.h[0] = (U64)window; 
  return handle;
}

function OS_Win32_Window *
os_win32_window_from_handle(OS_Handle handle) 
{
  OS_Win32_Window *window = (OS_Win32_Window *)handle.h[0];
  return window;
}

//
// Window events
//

LRESULT 
os_win32_window_proc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param)
{
  LRESULT result = 0;
  OS_Event event = {0};
  
  static B32 window_inactive = 0;
  
  switch (message) {
    case WM_DESTROY: {
      PostQuitMessage(0);
    }break;
    case WM_CLOSE: {
      event.kind = OS_EventKind_WindowClose; 
    }break;
    case WM_SIZE: {
      if (os_gfx_win32_state.resize_callback) {
        os_gfx_win32_state.resize_callback();
      }
    }break;
    case WM_PAINT: {
      PAINTSTRUCT ps = {0};
      BeginPaint(hwnd, &ps);
      if (os_gfx_win32_state.resize_callback) {
        os_gfx_win32_state.resize_callback();
      }
      EndPaint(hwnd, &ps);
      ShowWindow(os_gfx_win32_state.window->hwnd, SW_SHOW);
    }break;
    case WM_SETCURSOR: {
      B32 cursor_hide = os_gfx_state.cursor_hide;
      B32 cursor_clamp = os_gfx_state.cursor_clamp;
      
      if (LOWORD(l_param) == HTCLIENT) {
        if (window_inactive) {
          while (ShowCursor(TRUE) < 0);
          ClipCursor(0);
        }
        else {
          if (cursor_hide) {
            while (ShowCursor(FALSE) >= 0);
          }
          else {
            while (ShowCursor(TRUE) < 0);
          }
          if (cursor_clamp) {
            RECT client_rect;
            GetClientRect(hwnd, &client_rect);
            POINT tl = { client_rect.left, client_rect.top };
            POINT br = { client_rect.right, client_rect.bottom };
            ClientToScreen(hwnd, &tl);
            ClientToScreen(hwnd, &br);
            RECT clip_rect = { tl.x, tl.y, br.x, br.y };
            ClipCursor(&clip_rect);
          }
          else {
            ClipCursor(0);
          }
          
        }
      }
      else {
        result = DefWindowProcW(hwnd, message, w_param, l_param);
      }
    }break;
    case WM_SYSKEYDOWN: 
    case WM_SYSKEYUP: {
      result = DefWindowProcW(hwnd, message, w_param, l_param);
    } 
    case WM_KEYDOWN: 
    case WM_KEYUP: {
      B32 was_down = !!(l_param & (1 << 30));
      B32 is_down  =  !(l_param & (1 << 31));
      
      OS_EventKind kind = is_down ? OS_EventKind_KeyPress : OS_EventKind_KeyRelease; 
      
      OS_Key key = OS_Key_Null;
      if (w_param < OS_GFX_WIN32_KEYS_MAX){
        key = os_gfx_win32_key_table[w_param];
      }
      event.kind = kind;
      event.key  = key;
      
      (void)was_down;
    }break;
    case WM_MOUSEMOVE: {
      event.kind = OS_EventKind_MouseMotion;
      event.key  = OS_Key_Null;
      event.pos.x = (S32)(l_param & 0xFFFF);
      event.pos.y = (S32)(l_param >> 16);
    }break;
    case WM_LBUTTONDOWN: {
      event.kind = OS_EventKind_MousePress;
      event.key  = OS_Key_MouseLeft;
      event.pos.x = (S32)(l_param & 0xFFFF);
      event.pos.y = (S32)(l_param >> 16);
    }break;
    case WM_RBUTTONDOWN: {
      event.kind = OS_EventKind_MousePress;
      event.key  = OS_Key_MouseRight;
      event.pos.x = (S32)(l_param & 0xFFFF);
      event.pos.y = (S32)(l_param >> 16);
    }break;
    case WM_MBUTTONDOWN: {
      event.kind = OS_EventKind_MousePress;
      event.key  = OS_Key_MouseMiddle;
      event.pos.x = (S32)(l_param & 0xFFFF);
      event.pos.y = (S32)(l_param >> 16);
    }break;
    case WM_LBUTTONUP: {
      event.kind = OS_EventKind_MouseRelease;
      event.key  = OS_Key_MouseLeft;
      event.pos.x = (S32)(l_param & 0xFFFF);
      event.pos.y = (S32)(l_param >> 16);
    }break;
    case WM_RBUTTONUP: {
      event.kind = OS_EventKind_MouseRelease;
      event.key  = OS_Key_MouseRight;
      event.pos.x = (S32)(l_param & 0xFFFF);
      event.pos.y = (S32)(l_param >> 16);
    }break;
    case WM_MBUTTONUP: {
      event.kind = OS_EventKind_MouseRelease;
      event.key  = OS_Key_MouseMiddle;
      event.pos.x = (S32)(l_param & 0xFFFF);
      event.pos.y = (S32)(l_param >> 16);
    }break;
    case WM_MOUSEWHEEL: {
      event.kind = OS_EventKind_MouseScroll;
      S16 scroll = HIWORD(w_param);
      event.scroll.y = (S32)scroll;
    }break;
    default: {
      event.kind = OS_EventKind_Null;
      result = DefWindowProc(hwnd, message, w_param, l_param);
    }break;
  }
  
  if (event.kind != OS_EventKind_Null) {
    event.modifiers = os_win32_get_modifiers();
    os_push_event(event);
  }
  
  return result;
}

function void 
os_build_event_list(void)
{
  for (MSG message; PeekMessage(&message, 0, 0, 0, PM_REMOVE);) {
    TranslateMessage(&message);
    DispatchMessage(&message);
  }
}

//
// OS Graphical layer init
//

function void 
os_gfx_init(void)
{
  Arena *arena = arena_alloc_default();
  os_gfx_win32_state.arena = arena; 
  
  // Register the global window class
  WNDCLASS window_class      = {0};
  window_class.style         = CS_HREDRAW | CS_VREDRAW;
  window_class.lpfnWndProc   = os_win32_window_proc;
  window_class.hInstance     = os_win32_state.hinstance;
  window_class.lpszClassName = OS_GFX_WIN32_WINCLASS_NAME;
  RegisterClass(&window_class);
  
  // Get refresh rate of calling thread's display
  os_gfx_win32_state.refresh_rate = 60.f;
  DEVMODEA device_mode = {0};
  if (EnumDisplaySettings(0, ENUM_CURRENT_SETTINGS, &device_mode)) {
    os_gfx_win32_state.refresh_rate = (F32)device_mode.dmDisplayFrequency;
  }
  
  // Initialize the table that maps Windows virtual key codes to our key codes
  // w: Windows' virtual keycodes; k: our keycodes. 
  for (U32 w = 'A', k = OS_Key_A; w <= 'Z'; ++w, ++k) {
    os_gfx_win32_key_table[w] = (OS_Key)k;
  }
  for (U32 w = '0', k = OS_Key_0; w <= '9'; ++w, ++k) {
    os_gfx_win32_key_table[w] = (OS_Key)k;
  }
  for (U32 w = VK_F1, k = OS_Key_F1; w <= VK_F24; ++w, ++k) {
    os_gfx_win32_key_table[w] = (OS_Key)k;
  }
  os_gfx_win32_key_table[VK_ESCAPE]     = OS_Key_Esc;
  os_gfx_win32_key_table[VK_OEM_3]      = OS_Key_GraveAccent;
  os_gfx_win32_key_table[VK_OEM_MINUS]  = OS_Key_Minus;
  os_gfx_win32_key_table[VK_OEM_PLUS]   = OS_Key_Equal;
  os_gfx_win32_key_table[VK_BACK]       = OS_Key_Backspace;
  os_gfx_win32_key_table[VK_TAB]        = OS_Key_Tab;
  os_gfx_win32_key_table[VK_SPACE]      = OS_Key_Space;
  os_gfx_win32_key_table[VK_RETURN]     = OS_Key_Enter;
  os_gfx_win32_key_table[VK_CONTROL]    = OS_Key_Ctrl;
  os_gfx_win32_key_table[VK_SHIFT]      = OS_Key_Shift;
  os_gfx_win32_key_table[VK_MENU]       = OS_Key_Alt;
  os_gfx_win32_key_table[VK_UP]         = OS_Key_Up;
  os_gfx_win32_key_table[VK_LEFT]       = OS_Key_Left;
  os_gfx_win32_key_table[VK_DOWN]       = OS_Key_Down;
  os_gfx_win32_key_table[VK_RIGHT]      = OS_Key_Right;
  os_gfx_win32_key_table[VK_DELETE]     = OS_Key_Delete;
  os_gfx_win32_key_table[VK_PRIOR]      = OS_Key_PageUp;
  os_gfx_win32_key_table[VK_NEXT]       = OS_Key_PageDown;
  os_gfx_win32_key_table[VK_HOME]       = OS_Key_Home;
  os_gfx_win32_key_table[VK_END]        = OS_Key_End;
  os_gfx_win32_key_table[VK_OEM_2]      = OS_Key_ForwardSlash;
  os_gfx_win32_key_table[VK_OEM_PERIOD] = OS_Key_Period;
  os_gfx_win32_key_table[VK_OEM_COMMA]  = OS_Key_Comma;
  os_gfx_win32_key_table[VK_OEM_7]      = OS_Key_Quote;
  os_gfx_win32_key_table[VK_OEM_4]      = OS_Key_LeftBracket;
  os_gfx_win32_key_table[VK_OEM_6]      = OS_Key_RightBracket;
  os_gfx_win32_key_table[VK_INSERT]     = OS_Key_Insert;
  os_gfx_win32_key_table[VK_OEM_1]      = OS_Key_Semicolon;
  
  // Initialize the table that maps Windows cursors to our cursors
  os_gfx_win32_cursor_table[OS_CursorKind_Arrow]   = LoadCursorA(0, IDC_ARROW);
  os_gfx_win32_cursor_table[OS_CursorKind_Hand]    = LoadCursorA(0, IDC_HAND);
  os_gfx_win32_cursor_table[OS_CursorKind_Loading] = LoadCursorA(0, IDC_WAIT);
  os_gfx_win32_cursor_table[OS_CursorKind_Pan]     = LoadCursorA(0, IDC_SIZEALL);
  os_gfx_win32_cursor_table[OS_CursorKind_Blocked] = LoadCursorA(0, IDC_NO);
  
  os_gfx_win32_state.cursor = OS_CursorKind_Arrow;
  
  // Allocate and initialize the OS event list 
  os_gfx_state.event_arena = arena_alloc_default();
  ArenaPushStruct(os_gfx_state.event_arena, OS_EventList);
}

//
// Window functions
//

function OS_Handle 
os_window_open(String8 title, U32 width, U32 height)
{
  OS_Handle window = {0};
  
  os_gfx_win32_state.window = ArenaPushStruct(os_gfx_win32_state.arena, OS_Win32_Window);
  OS_Win32_Window *win32_window = os_gfx_win32_state.window;
  
  HWND hwnd = CreateWindow(OS_GFX_WIN32_WINCLASS_NAME, (LPCSTR)title.data, 
                           WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, 
                           os_win32_state.hinstance, 0);
  ShowWindow(hwnd, SW_SHOW);
  
  SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)win32_window);
  win32_window->hwnd = hwnd; 
  
  window = os_win32_handle_from_window(win32_window);
  return window;
}

function void 
os_window_close(OS_Handle window)
{
  OS_Win32_Window *win32_window = os_win32_window_from_handle(window);
  DestroyWindow(win32_window->hwnd);
}

function void 
os_window_minimize(OS_Handle window)
{
  OS_Win32_Window *win32_window = os_win32_window_from_handle(window);
  ShowWindow(win32_window->hwnd, SW_MINIMIZE);
}

function void 
os_window_maximize(OS_Handle window)
{
  OS_Win32_Window *win32_window = os_win32_window_from_handle(window);
  ShowWindow(win32_window->hwnd, SW_MAXIMIZE);
}

function void 
os_window_restore(OS_Handle window)
{
  OS_Win32_Window *win32_window = os_win32_window_from_handle(window);
  ShowWindow(win32_window->hwnd, SW_RESTORE);
}

function void 
os_window_fullscreen_enter(OS_Handle window)
{
  if (!os_gfx_state.window_fullscreen) {
    OS_Win32_Window *win32_window = os_win32_window_from_handle(window);
    HWND hwnd = win32_window->hwnd;
    
    RectU32 window_rect = os_window_rect(window);
    os_gfx_state.prev_window_rect = window_rect;
    
    MONITORINFO mi = { sizeof(mi) };
    B32 result = GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);
    if (result) {
      // Remove window decorations
      LONG old_style = GetWindowLong(hwnd, GWL_STYLE);
      LONG new_style = old_style & ~(WS_OVERLAPPEDWINDOW);
      SetWindowLong(hwnd, GWL_STYLE, new_style);
      
      // Resize and reposition window
      SetWindowPos(hwnd, HWND_TOP, 
                   mi.rcMonitor.left, mi.rcMonitor.top,
                   mi.rcMonitor.right - mi.rcMonitor.left,
                   mi.rcMonitor.bottom - mi.rcMonitor.top,
                   SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
    
    os_gfx_state.window_fullscreen = 1;
  }
}

function void 
os_window_fullscreen_exit(OS_Handle window)
{
  if (os_gfx_state.window_fullscreen) {
    OS_Win32_Window *win32_window = os_win32_window_from_handle(window);
    HWND hwnd = win32_window->hwnd;
    
    // Restore window style
    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    style |= WS_OVERLAPPEDWINDOW;
    SetWindowLong(hwnd, GWL_STYLE, style);
    
    // Restore previous windowed position and size
    RectU32 prev_rect = os_gfx_state.prev_window_rect;
    SetWindowPos(hwnd, NULL, 
                 prev_rect.x0, prev_rect.y0,
                 prev_rect.x1 - prev_rect.x0, 
                 prev_rect.y1 - prev_rect.y0, 
                 SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    
    os_gfx_state.window_fullscreen = 0;
  }
}


function RectU32
os_window_rect(OS_Handle window)
{
  RectU32 result = {0};
  
  OS_Win32_Window *win32_window = os_win32_window_from_handle(window);
  HWND hwnd = win32_window->hwnd;
  
  RECT rect;
  if (GetWindowRect(hwnd, &rect)) {
    result.x0 = rect.left;
    result.y0 = rect.top;
    result.x1 = rect.right;
    result.y1 = rect.bottom;
  }
  
  return result;
}

function RectU32
os_window_client_rect(OS_Handle window)
{
  RectU32 rect = {0};
  
  OS_Win32_Window *win32_window  = os_win32_window_from_handle(window);
  if (win32_window) {
    RECT win32_rect = {0};
    if (GetClientRect(win32_window->hwnd, &win32_rect)) {
      rect.x0 = (U32)win32_rect.left;
      rect.y0 = (U32)win32_rect.top;
      rect.x1 = (U32)win32_rect.right;
      rect.y1 = (U32)win32_rect.bottom;
    }
  }
  
  return rect;
}

function V2S32
os_window_cursor_pos(OS_Handle window)
{
  V2S32 pos = {0};
  
  OS_Win32_Window *win32_window = os_win32_window_from_handle(window);
  POINT p;
  GetCursorPos(&p);
  ScreenToClient(win32_window->hwnd, &p);
  
  pos.x = (S32)p.x;
  pos.y = (S32)p.y;
  
  return pos; 
}

//
// Global settings
//

function F32 
os_refresh_rate(void)
{
  return os_gfx_win32_state.refresh_rate;
}

function void 
os_set_resize_callback(void (*func)(void))
{
  os_gfx_win32_state.resize_callback = func;
}

//
// Miscellaneous helpers
//

function void 
os_set_cursor(OS_CursorKind cursor)
{
  os_gfx_win32_state.cursor = cursor;
}