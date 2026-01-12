#pragma once

enum OS_EventKind {
  OS_EventKind_Null,
  OS_EventKind_KeyPress,
  OS_EventKind_KeyRelease,
  // TODO: Makes no sense to use separate event kinds for mouse if mouse buttons
  // are considered keys. This discrepancy lead to confusion when writing input code!
  OS_EventKind_MousePress,
  OS_EventKind_MouseRelease,
  OS_EventKind_MouseMotion,
  OS_EventKind_MouseScroll,
  OS_EventKind_WindowClose,
};

enum OS_CursorKind {
  OS_CursorKind_Null,
  OS_CursorKind_Hidden,
  OS_CursorKind_Arrow,
  OS_CursorKind_Hand,
  OS_CursorKind_Loading,
  OS_CursorKind_Pan,
  OS_CursorKind_Blocked,
  OS_CursorKind_COUNT,
};

enum OS_Modifiers {
  OS_Modifier_Ctrl  = (1 << 0),
  OS_Modifier_Shift = (1 << 1),
  OS_Modifier_Alt   = (1 << 2),
  OS_Modifier_COUNT,
};

enum OS_Key {
  OS_Key_Null, 
  OS_Key_Esc, 
  OS_Key_F1, 
  OS_Key_F2, 
  OS_Key_F3, 
  OS_Key_F4, 
  OS_Key_F5, 
  OS_Key_F6, 
  OS_Key_F7, 
  OS_Key_F8, 
  OS_Key_F9, 
  OS_Key_F10, 
  OS_Key_F11, 
  OS_Key_F12, 
  OS_Key_F13, 
  OS_Key_F14, 
  OS_Key_F15, 
  OS_Key_F16, 
  OS_Key_F17, 
  OS_Key_F18, 
  OS_Key_F19, 
  OS_Key_F20, 
  OS_Key_F21, 
  OS_Key_F22, 
  OS_Key_F23, 
  OS_Key_F24, 
  OS_Key_GraveAccent, 
  OS_Key_0, 
  OS_Key_1, 
  OS_Key_2, 
  OS_Key_3, 
  OS_Key_4, 
  OS_Key_5, 
  OS_Key_6, 
  OS_Key_7, 
  OS_Key_8, 
  OS_Key_9, 
  OS_Key_Minus, 
  OS_Key_Equal, 
  OS_Key_Backspace, 
  OS_Key_Delete, 
  OS_Key_Tab, 
  OS_Key_A, 
  OS_Key_B, 
  OS_Key_C, 
  OS_Key_D , 
  OS_Key_E, 
  OS_Key_F, 
  OS_Key_G, 
  OS_Key_H, 
  OS_Key_I, 
  OS_Key_J, 
  OS_Key_K, 
  OS_Key_L, 
  OS_Key_M, 
  OS_Key_N, 
  OS_Key_O, 
  OS_Key_P,  
  OS_Key_Q,  
  OS_Key_R,  
  OS_Key_S,  
  OS_Key_T,  
  OS_Key_U,  
  OS_Key_V,  
  OS_Key_W,  
  OS_Key_X,  
  OS_Key_Y,  
  OS_Key_Z,  
  OS_Key_Space,  
  OS_Key_Enter,  
  OS_Key_Ctrl,  
  OS_Key_Shift,  
  OS_Key_Alt,  
  OS_Key_Up,  
  OS_Key_Left,  
  OS_Key_Down,  
  OS_Key_Right,  
  OS_Key_PageUp,  
  OS_Key_PageDown,  
  OS_Key_Home,  
  OS_Key_End,  
  OS_Key_ForwardSlash,  
  OS_Key_Period,  
  OS_Key_Comma,  
  OS_Key_Quote,  
  OS_Key_LeftBracket,  
  OS_Key_RightBracket,  
  OS_Key_Insert,  
  OS_Key_MouseLeft,  
  OS_Key_MouseMiddle,  
  OS_Key_MouseRight,  
  OS_Key_Semicolon,  
  OS_Key_COUNT,
};

struct OS_Event {
  OS_Event *next;
  OS_Event *prev; 
  
  OS_EventKind kind;
  OS_Key key;
  OS_Modifiers modifiers;
  V2S32 pos;     
  V2S32 scroll;
};

struct OS_EventList {
  OS_Event *first;
  OS_Event *last; 
};

//
// OS graphical layer init
//

function void os_gfx_init(void);

//
// Window functions 
//

function OS_Handle os_window_open(String8 title, U32 w, U32 h);
function void os_window_close(OS_Handle window);
function void os_window_minimize(OS_Handle window);
function void os_window_maximize(OS_Handle window);
function void os_window_restore(OS_Handle window);
function void os_window_fullscreen_enter(OS_Handle window);
function void os_window_fullscreen_exit(OS_Handle window);

function RectU32 os_window_rect(OS_Handle window);
function RectU32 os_window_client_rect(OS_Handle window);
function V2S32 os_window_cursor_pos(OS_Handle window); 

//
// Window events API 
//

function void os_push_event(OS_Event event); // NOTE: Used by platform-specific code
function void os_build_event_list(void); 
function OS_EventList *os_get_events(void);

//
// Global settings
//

function F32 os_refresh_rate(void);
function void os_set_resize_callback(void (*func)(void));

//
// Miscellaneous helpers
//

function void os_set_cursor(OS_CursorKind cursor);
function void os_cursor_hide(B32 hide);
function void os_cursor_clamp(B32 clamp);
