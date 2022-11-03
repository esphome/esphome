#include "keyboard.h"

namespace esphome {
namespace keyboard {

Keyboard::Keyboard(KeyboardControl *keyboard_control, KeyboardControl *media_keys_control)
    : keyboard_control_(keyboard_control), media_keys_control_(media_keys_control) {
  if (keyboard_control_) {
    keyboard_control_->keys_ = &keyboard_keys;
  }
  if (media_keys_control_) {
    media_keys_control_->keys_ = &media_keys;
  }
}
#ifdef USE_BINARY_SENSOR
void Keyboard::set_capslock_binary_sensor(binary_sensor::BinarySensor *capslock) { capslock_ = capslock; }
void Keyboard::set_numlock_binary_sensor(binary_sensor::BinarySensor *numlock) { numlock_ = numlock; }
void Keyboard::set_scrollock_binary_sensor(binary_sensor::BinarySensor *scrollock) { scrollock_ = scrollock; }
#endif
void Keyboard::add_on_state_callback(std::function<void()> &&callback) {
  // TODO
  this->state_callback_.add(std::move(callback));
}

KeyboardCall Keyboard::make_call(KeyboardType type) {
  switch (type) {
    case KEYBOARD:
      if (keyboard_control_) {
        return KeyboardCall(keyboard_control_);
      }
      break;
    case MEDIA_KEYS:
      if (media_keys_control_) {
        return KeyboardCall(media_keys_control_);
      }
      break;
  }
  return KeyboardCall(nullptr);
}
#ifdef USE_BINARY_SENSOR
void Keyboard::publish_capslock(bool state) {
  if (capslock_) {
    capslock_->publish_state(state);
  }
}

void Keyboard::publish_numlock(bool state) {
  if (numlock_) {
    numlock_->publish_state(state);
  }
}

void Keyboard::publish_scrollock(bool state) {
  if (scrollock_) {
    scrollock_->publish_state(state);
  }
}
#endif
}  // namespace keyboard
}  // namespace esphome
