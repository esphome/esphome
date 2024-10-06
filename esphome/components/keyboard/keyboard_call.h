#pragma once

#include <cstdint>
#include <vector>
#include <functional>

namespace esphome {
namespace keyboard {

class KeyboardControl;

enum KeyboardOperation : uint8_t {
  KEYBOARD_OP_NONE,
  KEYBOARD_OP_UP,
  KEYBOARD_OP_DOWN,
  KEYBOARD_OP_SET,
};

class KeyboardCall {
 public:
  explicit KeyboardCall(KeyboardControl *control) : control_(control) {}

  KeyboardCall &key_up(std::vector<uint16_t> &&keycode);
  KeyboardCall &key_down(std::vector<uint16_t> &&keycode);
  KeyboardCall &set_key(std::vector<uint16_t> &&keycode);
  KeyboardCall &key_up(const std::vector<uint16_t> &keycode);
  KeyboardCall &key_down(const std::vector<uint16_t> &keycode);
  KeyboardCall &set_key(const std::vector<uint16_t> &keycode);

  void perform();

 protected:
  KeyboardOperation operation_{KEYBOARD_OP_NONE};
  std::vector<uint16_t> keys_;
  KeyboardControl *control_;
};

}  // namespace keyboard
}  // namespace esphome
