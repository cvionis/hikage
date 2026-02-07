static void
get_input(OS_Handle window, Input *input, OS_EventList *events)
{
  for (OS_Event *e = events->first; e != 0; e = e->next) {
    Key slot = Key_Null;
    MouseButton mouse_slot = MouseButton_Null;

    switch (e->key) {
      case OS_Key_Esc:   { slot = Key_Esc;   }break;
      case OS_Key_Space: { slot = Key_Space; }break;
      case OS_Key_Enter: { slot = Key_Enter; }break;
      case OS_Key_Up:    { slot = Key_Up;    }break;
      case OS_Key_Down:  { slot = Key_Down;  }break;
      case OS_Key_Left:  { slot = Key_Left;  }break;
      case OS_Key_Right: { slot = Key_Right; }break;
      case OS_Key_W:     { slot = Key_W;     }break;
      case OS_Key_A:     { slot = Key_A;     }break;
      case OS_Key_S:     { slot = Key_S;     }break;
      case OS_Key_D:     { slot = Key_D;     }break;
      case OS_Key_Q:     { slot = Key_Q;     }break;
      case OS_Key_E:     { slot = Key_E;     }break;
      case OS_Key_R:     { slot = Key_R;     }break;
      case OS_Key_Z:     { slot = Key_Z;     }break;
      case OS_Key_0:     { slot = Key_0;     }break;
      case OS_Key_1:     { slot = Key_1;     }break;
      case OS_Key_2:     { slot = Key_2;     }break;
      case OS_Key_3:     { slot = Key_3;     }break;
      case OS_Key_4:     { slot = Key_4;     }break;
      case OS_Key_5:     { slot = Key_5;     }break;
      case OS_Key_6:     { slot = Key_6;     }break;
      case OS_Key_7:     { slot = Key_7;     }break;
      case OS_Key_8:     { slot = Key_8;     }break;
      case OS_Key_9:     { slot = Key_9;     }break;
      case OS_Key_F1:    { slot = Key_F1;    }break;
      case OS_Key_F2:    { slot = Key_F2;    }break;
      case OS_Key_F3:    { slot = Key_F3;    }break;
      case OS_Key_F4:    { slot = Key_F4;    }break;
      case OS_Key_F5:    { slot = Key_F5;    }break;
      case OS_Key_F6:    { slot = Key_F6;    }break;
      case OS_Key_F7:    { slot = Key_F7;    }break;
      case OS_Key_F8:    { slot = Key_F8;    }break;
      case OS_Key_F9:    { slot = Key_F9;    }break;
      case OS_Key_F10:   { slot = Key_F10;   }break;
      case OS_Key_F11:   { slot = Key_F11;   }break;
      case OS_Key_F12:   { slot = Key_F12;   }break;
      case OS_Key_Minus: { slot = Key_Minus; }break;

      case OS_Key_MouseLeft:   { mouse_slot = MouseButton_Left;   }break;
      case OS_Key_MouseMiddle: { mouse_slot = MouseButton_Middle; }break;
      case OS_Key_MouseRight:  { mouse_slot = MouseButton_Right;  }break;
    }

    switch (e->kind) {
      case OS_EventKind_KeyPress:   { input->keys[slot] = 1; }break;
      case OS_EventKind_KeyRelease: { input->keys[slot] = 0; }break;

      case OS_EventKind_MousePress:   { input->mouse.buttons[mouse_slot] = 1; }break;
      case OS_EventKind_MouseRelease: { input->mouse.buttons[mouse_slot] = 0; }break;
    }
  }

  V2S32 mouse_pos = os_window_cursor_pos(window);
  input->mouse.x = mouse_pos.x;
  input->mouse.y = mouse_pos.y;
}

static B32
key_pressed(Input *input, Key key)
{
  B32 result = input->keys[key];
  input->keys[key] = 0;
  return result;
}

static B32
key_down(Input *input, Key key)
{
  return input->keys[key];
}

static B32
mouse_pressed(Input *input, MouseButton btn)
{
  B32 result = input->mouse.buttons[btn];
  input->mouse.buttons[btn] = 0;
  return result;
}

static B32
mouse_down(Input *input, MouseButton btn)
{
  return input->mouse.buttons[btn];
}
