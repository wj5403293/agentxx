#include "agentxx/tools/ui_control.h"
#include "agentxx/util/log.h"
#include "fmt/format.h"
#include <asio/awaitable.hpp>
#include <asio/steady_timer.hpp>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#if XX_IS_WIN_D
#include <windows.h>
#endif

namespace agentxx {
namespace tools {

#if XX_IS_WIN_D

static WORD uiControlCharToVk(char ch) {
  if (ch >= 'A' && ch <= 'Z')
    return static_cast<WORD>(ch);
  if (ch >= 'a' && ch <= 'z')
    return static_cast<WORD>(toupper(ch));
  if (ch >= '0' && ch <= '9')
    return static_cast<WORD>(ch);
  switch (ch) {
  case ' ':
    return VK_SPACE;
  case '\n':
    return VK_RETURN;
  case '\t':
    return VK_TAB;
  case '\b':
    return VK_BACK;
  case '\x1b':
    return VK_ESCAPE;
  case '!':
    return VK_NUMPAD1;
  case '@':
    return VK_NUMPAD2;
  case '#':
    return VK_NUMPAD3;
  case '$':
    return VK_NUMPAD4;
  case '%':
    return VK_NUMPAD5;
  case '^':
    return VK_NUMPAD6;
  case '&':
    return VK_NUMPAD7;
  case '*':
    return VK_NUMPAD8;
  case '(':
    return VK_NUMPAD9;
  case ')':
    return VK_NUMPAD0;
  case '-':
    return VK_OEM_MINUS;
  case '=':
    return VK_OEM_PLUS;
  case '[':
    return VK_OEM_4;
  case ']':
    return VK_OEM_6;
  case '\\':
    return VK_OEM_5;
  case ';':
    return VK_OEM_1;
  case '\'':
    return VK_OEM_7;
  case ',':
    return VK_OEM_COMMA;
  case '.':
    return VK_OEM_PERIOD;
  case '/':
    return VK_OEM_2;
  case '`':
    return VK_OEM_3;
  case '_':
    return VK_OEM_MINUS;
  case '+':
    return VK_OEM_PLUS;
  case '{':
    return VK_OEM_4;
  case '}':
    return VK_OEM_6;
  case '|':
    return VK_OEM_5;
  case ':':
    return VK_OEM_1;
  case '"':
    return VK_OEM_7;
  case '<':
    return VK_OEM_COMMA;
  case '>':
    return VK_OEM_PERIOD;
  case '?':
    return VK_OEM_2;
  case '~':
    return VK_OEM_3;
  default:
    return VkKeyScanA(ch) & 0xFF;
  }
}

static bool uiControlNeedsShift(char ch) {
  switch (ch) {
  case '!':
  case '@':
  case '#':
  case '$':
  case '%':
  case '^':
  case '&':
  case '*':
  case '(':
  case ')':
  case '_':
  case '+':
  case '{':
  case '}':
  case '|':
  case ':':
  case '"':
  case '<':
  case '>':
  case '?':
  case '~':
    return true;
  default:
    return false;
  }
}

static WORD uiControlKeyNameToVk(const std::string &key) {
  if (key.size() == 1) {
    return uiControlCharToVk(key[0]);
  }

  std::string upper = key;
  for (auto &c : upper) {
    c = static_cast<char>(toupper(c));
  }

  if (upper == "ENTER" || upper == "RETURN")
    return VK_RETURN;
  if (upper == "TAB")
    return VK_TAB;
  if (upper == "ESCAPE" || upper == "ESC")
    return VK_ESCAPE;
  if (upper == "BACKSPACE" || upper == "BACK")
    return VK_BACK;
  if (upper == "DELETE" || upper == "DEL")
    return VK_DELETE;
  if (upper == "INSERT" || upper == "INS")
    return VK_INSERT;
  if (upper == "HOME")
    return VK_HOME;
  if (upper == "END")
    return VK_END;
  if (upper == "PAGEUP" || upper == "PAGE_UP")
    return VK_PRIOR;
  if (upper == "PAGEDOWN" || upper == "PAGE_DOWN")
    return VK_NEXT;
  if (upper == "UP" || upper == "ARROWUP" || upper == "ARROW_UP")
    return VK_UP;
  if (upper == "DOWN" || upper == "ARROWDOWN" || upper == "ARROW_DOWN")
    return VK_DOWN;
  if (upper == "LEFT" || upper == "ARROWLEFT" || upper == "ARROW_LEFT")
    return VK_LEFT;
  if (upper == "RIGHT" || upper == "ARROWRIGHT" || upper == "ARROW_RIGHT")
    return VK_RIGHT;
  if (upper == "SPACE")
    return VK_SPACE;
  if (upper == "F1")
    return VK_F1;
  if (upper == "F2")
    return VK_F2;
  if (upper == "F3")
    return VK_F3;
  if (upper == "F4")
    return VK_F4;
  if (upper == "F5")
    return VK_F5;
  if (upper == "F6")
    return VK_F6;
  if (upper == "F7")
    return VK_F7;
  if (upper == "F8")
    return VK_F8;
  if (upper == "F9")
    return VK_F9;
  if (upper == "F10")
    return VK_F10;
  if (upper == "F11")
    return VK_F11;
  if (upper == "F12")
    return VK_F12;
  if (upper == "SHIFT" || upper == "LSHIFT")
    return VK_LSHIFT;
  if (upper == "RSHIFT")
    return VK_RSHIFT;
  if (upper == "CTRL" || upper == "CONTROL" || upper == "LCTRL")
    return VK_LCONTROL;
  if (upper == "RCTRL")
    return VK_RCONTROL;
  if (upper == "ALT" || upper == "LALT")
    return VK_LMENU;
  if (upper == "RALT")
    return VK_RMENU;
  if (upper == "WIN" || upper == "LWIN" || upper == "META")
    return VK_LWIN;
  if (upper == "RWIN")
    return VK_RWIN;
  if (upper == "APPS" || upper == "MENU")
    return VK_APPS;
  if (upper == "CAPSLOCK" || upper == "CAPS")
    return VK_CAPITAL;
  if (upper == "NUMLOCK")
    return VK_NUMLOCK;
  if (upper == "SCROLLLOCK")
    return VK_SCROLL;
  if (upper == "PRINTSCREEN" || upper == "PRTSC")
    return VK_SNAPSHOT;
  if (upper == "PAUSE" || upper == "BREAK")
    return VK_PAUSE;
  if (upper == "NUMPAD0")
    return VK_NUMPAD0;
  if (upper == "NUMPAD1")
    return VK_NUMPAD1;
  if (upper == "NUMPAD2")
    return VK_NUMPAD2;
  if (upper == "NUMPAD3")
    return VK_NUMPAD3;
  if (upper == "NUMPAD4")
    return VK_NUMPAD4;
  if (upper == "NUMPAD5")
    return VK_NUMPAD5;
  if (upper == "NUMPAD6")
    return VK_NUMPAD6;
  if (upper == "NUMPAD7")
    return VK_NUMPAD7;
  if (upper == "NUMPAD8")
    return VK_NUMPAD8;
  if (upper == "NUMPAD9")
    return VK_NUMPAD9;
  if (upper == "VOLUME_UP" || upper == "VOLUMEUP")
    return VK_VOLUME_UP;
  if (upper == "VOLUME_DOWN" || upper == "VOLUMEDOWN")
    return VK_VOLUME_DOWN;
  if (upper == "VOLUME_MUTE" || upper == "VOLUMEMUTE")
    return VK_VOLUME_MUTE;

  return 0;
}

static std::string uiControlVkToKeyName(WORD vk) {
  switch (vk) {
  case VK_RETURN:
    return "Enter";
  case VK_TAB:
    return "Tab";
  case VK_ESCAPE:
    return "Escape";
  case VK_BACK:
    return "Backspace";
  case VK_DELETE:
    return "Delete";
  case VK_INSERT:
    return "Insert";
  case VK_HOME:
    return "Home";
  case VK_END:
    return "End";
  case VK_PRIOR:
    return "PageUp";
  case VK_NEXT:
    return "PageDown";
  case VK_UP:
    return "Up";
  case VK_DOWN:
    return "Down";
  case VK_LEFT:
    return "Left";
  case VK_RIGHT:
    return "Right";
  case VK_SPACE:
    return "Space";
  case VK_LSHIFT:
  case VK_RSHIFT:
    return "Shift";
  case VK_LCONTROL:
  case VK_RCONTROL:
    return "Ctrl";
  case VK_LMENU:
  case VK_RMENU:
    return "Alt";
  case VK_LWIN:
  case VK_RWIN:
    return "Win";
  case VK_CAPITAL:
    return "CapsLock";
  default:
    if (vk >= VK_F1 && vk <= VK_F12)
      return fmt::format("F{}", vk - VK_F1 + 1);
    if (vk >= 'A' && vk <= 'Z')
      return std::string(1, static_cast<char>(vk));
    if (vk >= '0' && vk <= '9')
      return std::string(1, static_cast<char>(vk));
    return fmt::format("Vk({})", vk);
  }
}

struct UICmdResult {
  bool ok = true;
  std::string msg;
};

static UICmdResult uiControlMouseMove(int x, int y) {
  SetCursorPos(x, y);
  return {true, fmt::format("mouse_move -> ({}, {})", x, y)};
}

static UICmdResult uiControlMouseClick(const std::string &button, int x, int y,
                                       bool at, int click_count) {
  if (at) {
    SetCursorPos(x, y);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  DWORD downFlag = 0, upFlag = 0;
  DWORD dataVal = 0;
  std::string btnName;

  if (button == "right") {
    downFlag = MOUSEEVENTF_RIGHTDOWN;
    upFlag = MOUSEEVENTF_RIGHTUP;
    btnName = "right";
  } else if (button == "middle") {
    downFlag = MOUSEEVENTF_MIDDLEDOWN;
    upFlag = MOUSEEVENTF_MIDDLEUP;
    btnName = "middle";
  } else {
    downFlag = MOUSEEVENTF_LEFTDOWN;
    upFlag = MOUSEEVENTF_LEFTUP;
    btnName = "left";
  }

  POINT pt;
  GetCursorPos(&pt);

  for (int i = 0; i < click_count; i++) {
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dx = 0;
    inputs[0].mi.dy = 0;
    inputs[0].mi.mouseData = dataVal;
    inputs[0].mi.dwFlags = downFlag;
    inputs[0].mi.time = 0;
    inputs[0].mi.dwExtraInfo = 0;

    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dx = 0;
    inputs[1].mi.dy = 0;
    inputs[1].mi.mouseData = dataVal;
    inputs[1].mi.dwFlags = upFlag;
    inputs[1].mi.time = 0;
    inputs[1].mi.dwExtraInfo = 0;

    SendInput(2, inputs, sizeof(INPUT));

    if (i < click_count - 1) {
      std::this_thread::sleep_for(std::chrono::milliseconds(80));
    }
  }

  std::string action = click_count > 1 ? "double_click" : "click";
  if (at) {
    return {true,
            fmt::format("mouse_{} @ ({}, {}) [{}]", action, x, y, btnName)};
  }
  return {true,
          fmt::format("mouse_{} @ ({}, {}) [{}]", action, pt.x, pt.y, btnName)};
}

static UICmdResult uiControlMouseScroll(int delta, int x, int y, bool at) {
  if (at) {
    SetCursorPos(x, y);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  INPUT input = {};
  input.type = INPUT_MOUSE;
  input.mi.dx = 0;
  input.mi.dy = 0;
  input.mi.mouseData = static_cast<DWORD>(delta);
  input.mi.dwFlags = MOUSEEVENTF_WHEEL;
  input.mi.time = 0;
  input.mi.dwExtraInfo = 0;

  SendInput(1, &input, sizeof(INPUT));

  POINT pt;
  GetCursorPos(&pt);
  return {true,
          fmt::format("mouse_scroll delta={} @ ({}, {})", delta, pt.x, pt.y)};
}

static UICmdResult uiControlMouseDrag(int x1, int y1, int x2, int y2,
                                      const std::string &button,
                                      int duration_ms) {
  SetCursorPos(x1, y1);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  DWORD downFlag = 0, upFlag = 0;
  if (button == "right") {
    downFlag = MOUSEEVENTF_RIGHTDOWN;
    upFlag = MOUSEEVENTF_RIGHTUP;
  } else if (button == "middle") {
    downFlag = MOUSEEVENTF_MIDDLEDOWN;
    upFlag = MOUSEEVENTF_MIDDLEUP;
  } else {
    downFlag = MOUSEEVENTF_LEFTDOWN;
    upFlag = MOUSEEVENTF_LEFTUP;
  }

  INPUT inputDown = {};
  inputDown.type = INPUT_MOUSE;
  inputDown.mi.dx = 0;
  inputDown.mi.dy = 0;
  inputDown.mi.dwFlags = downFlag;
  inputDown.mi.time = 0;
  inputDown.mi.dwExtraInfo = 0;
  SendInput(1, &inputDown, sizeof(INPUT));

  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  int steps = std::max(duration_ms / 10, 1);
  for (int i = 1; i <= steps; i++) {
    int cx = x1 + (x2 - x1) * i / steps;
    int cy = y1 + (y2 - y1) * i / steps;
    SetCursorPos(cx, cy);
    std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms / steps));
  }

  SetCursorPos(x2, y2);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  INPUT inputUp = {};
  inputUp.type = INPUT_MOUSE;
  inputUp.mi.dx = 0;
  inputUp.mi.dy = 0;
  inputUp.mi.dwFlags = upFlag;
  inputUp.mi.time = 0;
  inputUp.mi.dwExtraInfo = 0;
  SendInput(1, &inputUp, sizeof(INPUT));

  return {true, fmt::format("mouse_drag ({}, {}) -> ({}, {}) [{}]", x1, y1, x2,
                            y2, button)};
}

static UICmdResult uiControlKeyDown(WORD vk) {
  INPUT input = {};
  input.type = INPUT_KEYBOARD;
  input.ki.wVk = vk;
  input.ki.wScan = MapVirtualKey(vk, MAPVK_VK_TO_VSC);
  input.ki.dwFlags = 0;
  input.ki.time = 0;
  input.ki.dwExtraInfo = 0;
  SendInput(1, &input, sizeof(INPUT));
  return {true, fmt::format("key_down [{}]", uiControlVkToKeyName(vk))};
}

static UICmdResult uiControlKeyUp(WORD vk) {
  INPUT input = {};
  input.type = INPUT_KEYBOARD;
  input.ki.wVk = vk;
  input.ki.wScan = MapVirtualKey(vk, MAPVK_VK_TO_VSC);
  input.ki.dwFlags = KEYEVENTF_KEYUP;
  input.ki.time = 0;
  input.ki.dwExtraInfo = 0;
  SendInput(1, &input, sizeof(INPUT));
  return {true, fmt::format("key_up [{}]", uiControlVkToKeyName(vk))};
}

static UICmdResult uiControlKeyPress(WORD vk) {
  INPUT inputs[2] = {};
  inputs[0].type = INPUT_KEYBOARD;
  inputs[0].ki.wVk = vk;
  inputs[0].ki.wScan = MapVirtualKey(vk, MAPVK_VK_TO_VSC);
  inputs[0].ki.dwFlags = 0;
  inputs[0].ki.time = 0;
  inputs[0].ki.dwExtraInfo = 0;

  inputs[1].type = INPUT_KEYBOARD;
  inputs[1].ki.wVk = vk;
  inputs[1].ki.wScan = MapVirtualKey(vk, MAPVK_VK_TO_VSC);
  inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
  inputs[1].ki.time = 0;
  inputs[1].ki.dwExtraInfo = 0;

  SendInput(2, inputs, sizeof(INPUT));
  return {true, fmt::format("key_press [{}]", uiControlVkToKeyName(vk))};
}

static UICmdResult uiControlKeyCombo(const std::vector<WORD> &vks) {
  std::vector<INPUT> inputs;
  inputs.reserve(vks.size() * 2);

  std::string comboStr;
  for (auto vk : vks) {
    INPUT down = {};
    down.type = INPUT_KEYBOARD;
    down.ki.wVk = vk;
    down.ki.wScan = MapVirtualKey(vk, MAPVK_VK_TO_VSC);
    down.ki.dwFlags = 0;
    down.ki.time = 0;
    down.ki.dwExtraInfo = 0;
    inputs.push_back(down);

    if (!comboStr.empty()) {
      comboStr += "+";
    }
    comboStr += uiControlVkToKeyName(vk);
  }

  for (auto it = vks.rbegin(); it != vks.rend(); ++it) {
    INPUT up = {};
    up.type = INPUT_KEYBOARD;
    up.ki.wVk = *it;
    up.ki.wScan = MapVirtualKey(*it, MAPVK_VK_TO_VSC);
    up.ki.dwFlags = KEYEVENTF_KEYUP;
    up.ki.time = 0;
    up.ki.dwExtraInfo = 0;
    inputs.push_back(up);
  }

  SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
  return {true, fmt::format("key_combo [{}]", comboStr)};
}

static UICmdResult uiControlKeyType(const std::string &text) {
  for (char ch : text) {
    WORD vk = uiControlCharToVk(ch);
    if (vk == 0)
      continue;

    bool needsShift = uiControlNeedsShift(ch) || (ch >= 'A' && ch <= 'Z');

    if (needsShift) {
      INPUT shiftDown = {};
      shiftDown.type = INPUT_KEYBOARD;
      shiftDown.ki.wVk = VK_LSHIFT;
      shiftDown.ki.wScan = MapVirtualKey(VK_LSHIFT, MAPVK_VK_TO_VSC);
      shiftDown.ki.dwFlags = 0;
      shiftDown.ki.time = 0;
      shiftDown.ki.dwExtraInfo = 0;
      SendInput(1, &shiftDown, sizeof(INPUT));

      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = vk;
    inputs[0].ki.wScan = MapVirtualKey(vk, MAPVK_VK_TO_VSC);
    inputs[0].ki.dwFlags = 0;
    inputs[0].ki.time = 0;
    inputs[0].ki.dwExtraInfo = 0;

    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = vk;
    inputs[1].ki.wScan = MapVirtualKey(vk, MAPVK_VK_TO_VSC);
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[1].ki.time = 0;
    inputs[1].ki.dwExtraInfo = 0;

    SendInput(2, inputs, sizeof(INPUT));

    if (needsShift) {
      INPUT shiftUp = {};
      shiftUp.type = INPUT_KEYBOARD;
      shiftUp.ki.wVk = VK_LSHIFT;
      shiftUp.ki.wScan = MapVirtualKey(VK_LSHIFT, MAPVK_VK_TO_VSC);
      shiftUp.ki.dwFlags = KEYEVENTF_KEYUP;
      shiftUp.ki.time = 0;
      shiftUp.ki.dwExtraInfo = 0;
      SendInput(1, &shiftUp, sizeof(INPUT));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return {true, fmt::format("key_type [{} chars]", text.size())};
}

static UICmdResult uiControlGetCursorPos() {
  POINT pt;
  GetCursorPos(&pt);
  return {true, fmt::format("cursor_pos: ({}, {})", pt.x, pt.y)};
}

static UICmdResult uiControlGetScreenSize() {
  int w = GetSystemMetrics(SM_CXSCREEN);
  int h = GetSystemMetrics(SM_CYSCREEN);
  return {true, fmt::format("screen_size: {}x{}", w, h)};
}

static UICmdResult uiControlExecuteOne(const neograph::json &cmd) {
  if (!cmd.is_object() || !cmd.contains("action")) {
    return {false, "missing `action` field"};
  }

  auto action = cmd.value("action", std::string{});
  if (action.empty()) {
    return {false, "`action` is empty"};
  }

  if (action == "mouse_move") {
    if (!cmd.contains("x") || !cmd.contains("y")) {
      return {false, "mouse_move requires `x` and `y`"};
    }
    return uiControlMouseMove(cmd.value<int>("x", 0), cmd.value<int>("y", 0));
  }

  if (action == "mouse_click" || action == "mouse_double_click") {
    auto button = cmd.value("button", std::string{"left"});
    int x = cmd.value<int>("x", 0);
    int y = cmd.value<int>("y", 0);
    bool at = cmd.contains("x") && cmd.contains("y");
    int count = (action == "mouse_double_click") ? 2 : 1;
    return uiControlMouseClick(button, x, y, at, count);
  }

  if (action == "mouse_scroll") {
    if (!cmd.contains("delta")) {
      return {false, "mouse_scroll requires `delta`"};
    }
    int delta = cmd.value<int>("delta", 0);
    int x = cmd.value<int>("x", 0);
    int y = cmd.value<int>("y", 0);
    bool at = cmd.contains("x") && cmd.contains("y");
    return uiControlMouseScroll(delta, x, y, at);
  }

  if (action == "mouse_drag") {
    if (!cmd.contains("x1") || !cmd.contains("y1") || !cmd.contains("x2") ||
        !cmd.contains("y2")) {
      return {false, "mouse_drag requires `x1`, `y1`, `x2`, `y2`"};
    }
    auto button = cmd.value("button", std::string{"left"});
    int duration = cmd.value<int>("duration_ms", 200);
    return uiControlMouseDrag(cmd.value<int>("x1", 0), cmd.value<int>("y1", 0),
                              cmd.value<int>("x2", 0), cmd.value<int>("y2", 0),
                              button, duration);
  }

  if (action == "key_press") {
    if (!cmd.contains("key")) {
      return {false, "key_press requires `key`"};
    }
    auto key = cmd.value("key", std::string{});
    WORD vk = uiControlKeyNameToVk(key);
    if (vk == 0) {
      return {false, fmt::format("unknown key: {}", key)};
    }
    return uiControlKeyPress(vk);
  }

  if (action == "key_down") {
    if (!cmd.contains("key")) {
      return {false, "key_down requires `key`"};
    }
    auto key = cmd.value("key", std::string{});
    WORD vk = uiControlKeyNameToVk(key);
    if (vk == 0) {
      return {false, fmt::format("unknown key: {}", key)};
    }
    return uiControlKeyDown(vk);
  }

  if (action == "key_up") {
    if (!cmd.contains("key")) {
      return {false, "key_up requires `key`"};
    }
    auto key = cmd.value("key", std::string{});
    WORD vk = uiControlKeyNameToVk(key);
    if (vk == 0) {
      return {false, fmt::format("unknown key: {}", key)};
    }
    return uiControlKeyUp(vk);
  }

  if (action == "key_combo") {
    if (!cmd.contains("keys") || !cmd["keys"].is_array()) {
      return {false, "key_combo requires `keys` array"};
    }
    std::vector<WORD> vks;
    for (const auto &k : cmd["keys"]) {
      auto keyStr = k.get<std::string>();
      WORD vk = uiControlKeyNameToVk(keyStr);
      if (vk == 0) {
        return {false, fmt::format("unknown key in combo: {}", keyStr)};
      }
      vks.push_back(vk);
    }
    return uiControlKeyCombo(vks);
  }

  if (action == "key_type") {
    if (!cmd.contains("text")) {
      return {false, "key_type requires `text`"};
    }
    return uiControlKeyType(cmd.value("text", std::string{}));
  }

  if (action == "wait") {
    int ms = cmd.value<int>("ms", 100);
    ms = std::clamp(ms, 0, 30000);
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    return {true, fmt::format("wait {}ms", ms)};
  }

  if (action == "get_cursor_pos") {
    return uiControlGetCursorPos();
  }

  if (action == "get_screen_size") {
    return uiControlGetScreenSize();
  }

  return {false, fmt::format("unknown action: {}", action)};
}

#endif // XX_IS_WIN_D

neograph::ChatTool UIControlKeyboardMouseTool::get_definition() const {
  return {
      "ui_control_keyboard_mouse",
      R"(Control mouse and keyboard on Windows. Accepts a list of UI commands and executes them sequentially.

## Actions

### Mouse
- `mouse_move`: Move cursor. Params: `x`, `y`
- `mouse_click`: Click. Params: `button`("left"/"right"/"middle", default "left"), `x`, `y`(optional, move then click)
- `mouse_double_click`: Double click. Params: same as mouse_click
- `mouse_scroll`: Scroll wheel. Params: `delta`(positive=up, negative=down, ±120 per notch), `x`, `y`(optional)
- `mouse_drag`: Drag. Params: `x1`, `y1`, `x2`, `y2`, `button`(default "left"), `duration_ms`(default 200)

### Keyboard
- `key_press`: Press and release a key. Params: `key`
- `key_down`: Hold a key down. Params: `key`
- `key_up`: Release a held key. Params: `key`
- `key_combo`: Press key combination (e.g. Ctrl+C). Params: `keys`(array of key names)
- `key_type`: Type a text string. Params: `text`

### Utility
- `wait`: Pause execution. Params: `ms`(milliseconds, max 30000)
- `get_cursor_pos`: Get current cursor position. No params
- `get_screen_size`: Get screen resolution. No params

### Key Names
Single characters: "a"-"z", "0"-"9"
Special keys: "enter", "tab", "escape", "backspace", "delete", "insert", "home", "end", "pageup", "pagedown", "up", "down", "left", "right", "space"
Modifiers: "shift", "ctrl", "alt", "win"
F-keys: "f1"-"f12"
Lock keys: "capslock", "numlock", "scrolllock"
Other: "printscreen", "pause", "apps"

### Examples
```json
{"action": "mouse_click", "button": "left", "x": 100, "y": 200}
{"action": "key_combo", "keys": ["ctrl", "c"]}
{"action": "key_type", "text": "Hello World"}
{"action": "mouse_drag", "x1": 100, "y1": 100, "x2": 300, "y2": 300}
```)",
      {
          {"type", "object"},
          {
              "properties",
              {
                  {
                      "commands",
                      {
                          {"type", "array"},
                          {"description", "Ordered list of UI commands to "
                                          "execute sequentially."},
                          {
                              "items",
                              {
                                  {"type", "object"},
                                  {"properties",
                                   {{
                                        "action",
                                        {{"type", "string"},
                                         {"description",
                                          "Action to perform. One of: "
                                          "mouse_move, "
                                          "mouse_click, "
                                          "mouse_double_click, "
                                          "mouse_scroll, mouse_drag, "
                                          "key_press, "
                                          "key_down, key_up, key_combo, "
                                          "key_type, "
                                          "wait, get_cursor_pos, "
                                          "get_screen_size"}},
                                    },
                                    {"x", {{"type", "number"}}},
                                    {"y", {{"type", "number"}}},
                                    {"x1", {{"type", "number"}}},
                                    {"y1", {{"type", "number"}}},
                                    {"x2", {{"type", "number"}}},
                                    {"y2", {{"type", "number"}}},
                                    {
                                        "button",
                                        {
                                            {"type", "string"},
                                            {"enum",
                                             {"left", "right", "middle"}},
                                        },
                                    },
                                    {"delta", {{"type", "number"}}},
                                    {
                                        "key",
                                        {{"type", "string"},
                                         {"description",
                                          "Key name for "
                                          "key_press/key_down/key_up"}},
                                    },
                                    {
                                        "keys",
                                        {
                                            {"type", "array"},
                                            {"items", {{"type", "string"}}},
                                            {"description",
                                             "Key names for key_combo, "
                                             "e.g. "
                                             "[\"ctrl\",\"c\"]"},
                                        },
                                    },
                                    {
                                        "text",
                                        {{"type", "string"},
                                         {"description",
                                          "Text string for key_type"}},
                                    },
                                    {
                                        "ms",
                                        {{"type", "number"},
                                         {"description", "Wait duration in "
                                                         "milliseconds"}},
                                    },
                                    {
                                        "duration_ms",
                                        {{"type", "number"},
                                         {"description", "Drag duration in "
                                                         "milliseconds"}},
                                    }}},
                                  {
                                      "required",
                                      neograph::json::array({"action"}),
                                  },
                              },
                          },
                      },
                  },
                  {
                      "interval_ms",
                      {
                          {"type", "number"},
                          {
                              "description",
                              "Default interval between commands in "
                              "milliseconds. "
                              "Default 50. Set 0 for no delay.",
                          },
                      },
                  },
              },
          },
          {"required", neograph::json::array({"commands"})},
      },
  };
}

asio::awaitable<std::string>
UIControlKeyboardMouseTool::execute_async(const neograph::json &arguments) {
  if (!arguments.contains("commands") || !arguments["commands"].is_array()) {
    co_return R"({"error":"Arg `commands` is required and must be an array"})";
  }

  auto cmds = arguments["commands"].get<neograph::json>();
  if (cmds.empty()) {
    co_return R"({"error":"`commands` is empty"})";
  }

#if XX_IS_WIN_D
  int interval_ms = arguments.value<int>("interval_ms", 50);
  auto timer = asio::steady_timer{co_await asio::this_coro::executor};
  timer.expires_after(std::chrono::milliseconds(interval_ms));

  neograph::json results = neograph::json::array();
  int ok_count = 0;
  int fail_count = 0;

  for (size_t i = 0; i < cmds.size(); i++) {
    auto r = uiControlExecuteOne(cmds[i]);
    if (r.ok) {
      ok_count++;
    } else {
      fail_count++;
    }
    results.push_back(neograph::json{
        {"index", i},
        {"action", cmds[i].value("action", std::string{})},
        {"ok", r.ok},
        {"msg", r.msg},
    });

    if (!r.ok) {
      break;
    }

    if (interval_ms > 0 && i < cmds.size() - 1) {
      co_await timer.async_wait();
      std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }
  }

  co_return results.dump();
#else
  co_return R"({"error":"ui_control_keyboard_mouse is not available on current system"})";
#endif
}

} // namespace tools
} // namespace agentxx