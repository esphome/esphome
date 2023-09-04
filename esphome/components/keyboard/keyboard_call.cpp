#include "keyboard_call.h"
#include "keyboard.h"
#include "esphome/core/log.h"

namespace esphome {
namespace keyboard {

static const char *const TAG = "keyboard";

KeyboardCall &KeyboardCall::key_up(std::vector<uint16_t> &&keycode) {
  keys_ = std::move(keycode);
  operation_ = KEYBOARD_OP_UP;
  return *this;
}

KeyboardCall &KeyboardCall::key_down(std::vector<uint16_t> &&keycode) {
  keys_ = std::move(keycode);
  operation_ = KEYBOARD_OP_DOWN;
  return *this;
}

KeyboardCall &KeyboardCall::set_key(std::vector<uint16_t> &&keycode) {
  keys_ = std::move(keycode);
  operation_ = KEYBOARD_OP_SET;
  return *this;
}

KeyboardCall &KeyboardCall::key_up(const std::vector<uint16_t> &keycode) {
  keys_ = keycode;
  operation_ = KEYBOARD_OP_UP;
  return *this;
}

KeyboardCall &KeyboardCall::key_down(const std::vector<uint16_t> &keycode) {
  keys_ = keycode;
  operation_ = KEYBOARD_OP_DOWN;
  return *this;
}

KeyboardCall &KeyboardCall::set_key(const std::vector<uint16_t> &keycode) {
  keys_ = keycode;
  operation_ = KEYBOARD_OP_SET;
  return *this;
}

void KeyboardCall::perform() {
  if (this->control_ == nullptr) {
    ESP_LOGW(TAG, "KeyboardCall performed with null control");
    return;
  }
  if (this->control_->keys_ == nullptr) {
    ESP_LOGW(TAG, "KeyboardCall performed without keys");
    return;
  }
  if (this->operation_ == KEYBOARD_OP_NONE) {
    ESP_LOGW(TAG, "KeyboardCall performed without selecting an operation");
    return;
  }

  if (operation_ == KEYBOARD_OP_UP) {
    std::vector<uint16_t> tmp;
    for (auto pressed : *control_->keys_) {
      bool keep = true;
      for (auto up : keys_) {
        if (up == pressed) {
          keep = false;
          break;
        }
      }
      if (keep) {
        tmp.push_back(pressed);
      }
    }
    keys_ = std::move(tmp);
  } else if (operation_ == KEYBOARD_OP_DOWN) {
    // merge key which are down
    keys_.insert(keys_.end(), (*control_->keys_).begin(), (*control_->keys_).end());
    // remove duplicates
    sort(keys_.begin(), keys_.end());
    keys_.erase(unique(keys_.begin(), keys_.end()), keys_.end());
  }
  ESP_LOGV(TAG, "Setting keys value, size: %d", keys_.size());
  control_->control(std::move(keys_));
}

}  // namespace keyboard
}  // namespace esphome
