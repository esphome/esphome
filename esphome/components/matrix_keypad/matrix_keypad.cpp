#include "matrix_keypad.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#include <memory>

namespace esphome::matrix_keypad {

ESPHOME_LOG_TAG(TAG, "matrix_keypad");

void MatrixKeypad::setup() {
  if (this->has_diodes_)
    this->key_states_ = std::make_unique<KeyState[]>(this->rows_.size() * this->columns_.size());
  for (auto *pin : this->rows_) {
    pin->setup();
    if (!this->has_diodes_) {
      pin->pin_mode(gpio::FLAG_INPUT);
    } else {
      pin->digital_write(!this->has_pulldowns_);
    }
  }
  for (auto *pin : this->columns_) {
    pin->setup();
    if (this->has_pulldowns_) {
      pin->pin_mode(gpio::FLAG_INPUT);
    } else {
      pin->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
    }
  }
}

void MatrixKeypad::loop() {
  uint32_t now = App.get_loop_component_start_time();
  int key = -1;
  bool error = false;
  int pos = 0;
  for (auto *row : this->rows_) {
    if (!this->has_diodes_)
      row->pin_mode(gpio::FLAG_OUTPUT);
    row->digital_write(this->has_pulldowns_);
    for (auto *col : this->columns_) {
      bool down = col->digital_read() == this->has_pulldowns_;
      if (this->has_diodes_) {
        auto &state = this->key_states_[pos];
        if (down != state.active) {
          // Match single-key behavior: release immediately, debounce presses only.
          if (state.pressed) {
            this->release_key_(pos);
            state.pressed = false;
          }
          state.active = down;
          state.active_start = now;
        }
        if (down && !state.pressed && now - state.active_start >= this->debounce_time_) {
          this->press_key_(pos);
          state.pressed = true;
        }
      } else if (down) {
        if (key != -1) {
          error = true;
        } else {
          key = pos;
        }
      }
      pos++;
    }
    row->digital_write(!this->has_pulldowns_);
    if (!this->has_diodes_)
      row->pin_mode(gpio::FLAG_INPUT);
  }
  if (this->has_diodes_)
    return;
  if (error)
    return;

  if (key != this->active_key_) {
    if ((this->active_key_ != -1) && (this->pressed_key_ == this->active_key_)) {
      this->release_key_(this->pressed_key_);
      this->pressed_key_ = -1;
    }

    this->active_key_ = key;
    if (key == -1)
      return;
    this->active_start_ = now;
  }

  if ((this->pressed_key_ == key) || (now - this->active_start_ < this->debounce_time_))
    return;

  this->press_key_(key);
  this->pressed_key_ = key;
}

void MatrixKeypad::press_key_(int key) {
  int row = key / this->columns_.size();
  int col = key % this->columns_.size();
  ESP_LOGD(TAG, "key @ row %d, col %d pressed", row, col);
  for (auto &listener : this->listeners_)
    listener->button_pressed(row, col);
  if (this->keys_ != nullptr) {
    uint8_t keycode = progmem_read_byte(&this->keys_[key]);
    ESP_LOGD(TAG, "key '%c' pressed", keycode);
#ifdef MATRIX_KEYPAD_ON_KEY_COUNT
    this->key_automations_.trigger(keycode);
#endif
    for (auto &listener : this->listeners_)
      listener->key_pressed(keycode);
    this->send_key_(keycode);
  }
}

void MatrixKeypad::release_key_(int key) {
  int row = key / this->columns_.size();
  int col = key % this->columns_.size();
  ESP_LOGD(TAG, "key @ row %d, col %d released", row, col);
  for (auto &listener : this->listeners_)
    listener->button_released(row, col);
  if (this->keys_ != nullptr) {
    uint8_t keycode = progmem_read_byte(&this->keys_[key]);
    ESP_LOGD(TAG, "key '%c' released", keycode);
    for (auto &listener : this->listeners_)
      listener->key_released(keycode);
  }
}

void MatrixKeypad::dump_config() {
  ESP_LOGCONFIG(TAG, "Matrix Keypad:\n"
                     " Rows:");
  for (auto &pin : this->rows_) {
    LOG_PIN("  Pin: ", pin);
  }
  ESP_LOGCONFIG(TAG, " Cols:");
  for (auto &pin : this->columns_) {
    LOG_PIN("  Pin: ", pin);
  }
}

void MatrixKeypad::register_listener(MatrixKeypadListener *listener) { this->listeners_.push_back(listener); }

}  // namespace esphome::matrix_keypad
