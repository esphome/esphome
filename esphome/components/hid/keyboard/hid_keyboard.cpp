#include "hid_keyboard.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome {
namespace hid {

static const char *const TAG = "hid";

void Keyboard::control(std::vector<uint16_t> &&keys) {
  modifier_ = 0;
  std::memset(hidcode_, 0, ROLLOVER);
  uint8_t i = 0;
  auto code = std::begin(keys);
  while (code != std::end(keys)) {
    if (*code >= 0xE0 && *code <= 0xE7) {
      modifier_ |= 1 << (*code - 0xE0);
    } else {
      if (i >= ROLLOVER) {
        code = keys.erase(code);
        continue;
      }
      hidcode_[i++] = *code;
    }
    ++code;
  }
  report();
  // TODO public, callbacks???
  *this->keys_ = std::move(keys);
}

void Keyboard::set_parrent(Keyboard *thiz, keyboard::LEDControl *led_control) {
  if (thiz) {
    thiz->set_parrent(led_control);
  }
}

}  // namespace hid
}  // namespace esphome
