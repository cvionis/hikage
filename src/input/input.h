enum Key {
  Key_Null,
  Key_Esc,
  Key_Space,
  Key_Enter,
  Key_Up,
  Key_Left,
  Key_Down,
  Key_Right,
  Key_W,
  Key_A,
  Key_S,
  Key_D,
  Key_Q,
  Key_E,
  Key_R,
  Key_Z,
  Key_0,
  Key_1,
  Key_2,
  Key_3,
  Key_4,
  Key_5,
  Key_6,
  Key_7,
  Key_8,
  Key_9,
  Key_F1,
  Key_F2,
  Key_F3,
  Key_F4,
  Key_F5,
  Key_F6,
  Key_F7,
  Key_F8,
  Key_F9,
  Key_F10,
  Key_F11,
  Key_F12,
  Key_Minus,
  Key_COUNT,
};

enum MouseButton {
  MouseButton_Null,
  MouseButton_Left,
  MouseButton_Middle,
  MouseButton_Right,
  MouseButton_COUNT,
};

struct Input {
  B32 keys[Key_COUNT];
  struct {
    S32 x;
    S32 y;
    B32 buttons[MouseButton_COUNT];
  }mouse;
};

static void get_input(OS_Handle window, Input *input, OS_EventList *events);
static B32 key_pressed(Input *input, Key key);
static B32 key_down(Input *input, Key key);
static B32 mouse_pressed(Input *input, MouseButton btn);
static B32 mouse_down(Input *input, MouseButton btn);
