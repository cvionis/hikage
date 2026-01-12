global char *os_key_string_table[] = {
  "Null", 
  "Escape", 
  "F1", 
  "F2", 
  "F3", 
  "F4", 
  "F5", 
  "F6", 
  "F7", 
  "F8", 
  "F9", 
  "F10", 
  "F11", 
  "F12", 
  "F13", 
  "F14", 
  "F15", 
  "F16", 
  "F17", 
  "F18", 
  "F19", 
  "F20", 
  "F21", 
  "F22", 
  "F23", 
  "F24", 
  "Grave Accent", 
  "0", 
  "1", 
  "2", 
  "3", 
  "4", 
  "5", 
  "6", 
  "7", 
  "8", 
  "9", 
  "Minus", 
  "Equal", 
  "Backspace", 
  "Delete", 
  "Tab", 
  "A", 
  "B", 
  "C", 
  "D", 
  "E", 
  "F", 
  "G", 
  "H", 
  "I", 
  "J", 
  "K", 
  "L", 
  "M", 
  "N", 
  "O", 
  "P", 
  "Q", 
  "R", 
  "S", 
  "T", 
  "U", 
  "V", 
  "W", 
  "X",
  "Y", 
  "Z", 
  "Space", 
  "Enter", 
  "Ctrl", 
  "Shift", 
  "Alt", 
  "Up", 
  "Left", 
  "Down", 
  "Right", 
  "Page Up", 
  "Page Down", 
  "Home", 
  "End", 
  "Forward Slash", 
  "Period", 
  "Comma", 
  "Quote", 
  "Left Bracket", 
  "Right Bracket", 
  "Insert", 
  "Left Mouse Button", 
  "Middle Mouse Button", 
  "Right Mouse Button", 
  "Semicolon"
};

struct OS_CodepointEntry {
  OS_Key key;
  U32 char_no_shift;
  U32 char_shift;
};

global OS_CodepointEntry os_key_codepoint_table[] = {
  { OS_Key_Space, ' ', ' ' },
  { OS_Key_1, '1', '!' },
  { OS_Key_2, '2', '@' },
  { OS_Key_3, '3', '#' },
  { OS_Key_4, '4', '$' },
  { OS_Key_5, '5', '%' },
  { OS_Key_6, '6', '^' },
  { OS_Key_7, '7', '&' },
  { OS_Key_8, '8', '*' },
  { OS_Key_9, '9', '(' },
  { OS_Key_0, '0', ')' },
  { OS_Key_Minus, '-', '_' },
  { OS_Key_Equal, '=', '+' },
  { OS_Key_GraveAccent, '`', '~' },
  { OS_Key_LeftBracket, '[', '{' },
  { OS_Key_RightBracket, ']', '}' },
  { OS_Key_Quote, '\'', '"' },
  { OS_Key_Comma, ',', '<' },
  { OS_Key_Period, '.', '>' },
  { OS_Key_ForwardSlash, '/', '?' },
  { OS_Key_Enter, '\n', '\n' },
  { OS_Key_Semicolon, ';', ':' },
};

struct OS_Gfx_State {
  // Events
  Arena *event_arena; 
  OS_EventList *event_list;
  
  B32 cursor_clamp;
  B32 cursor_hide;
  
  
  B32 window_fullscreen;
  RectU32 prev_window_rect;
};

global OS_Gfx_State os_gfx_state;

//
// Window events
//

// NOTE: Internal function used by platform-specific window event code.
function void
os_push_event(OS_Event event)
{
  OS_Event *e = ArenaPushStruct(os_gfx_state.event_arena, OS_Event);
  MemoryCopyStruct(e, &event); 
  DLLPushBack(os_gfx_state.event_list->first, os_gfx_state.event_list->last, e);
}

// NOTE: This function is called every frame in the program's main loop.
function OS_EventList *
os_get_events(void)
{
  // Reset the event list
  arena_clear(os_gfx_state.event_arena);
  os_gfx_state.event_list = ArenaPushStruct(os_gfx_state.event_arena, OS_EventList);
  
  // Get a fresh new set of events from the OS
  os_build_event_list();
  OS_EventList *events = os_gfx_state.event_list;
  
  return events;
}

//
// Miscellaneous helpers
//

function void
os_cursor_hide(B32 hide)
{
  os_gfx_state.cursor_hide = hide;
}

function void
os_cursor_clamp(B32 clamp)
{
  os_gfx_state.cursor_clamp = clamp;
}