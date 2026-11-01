#pragma once

#include "esphome/components/key_provider/key_provider.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include <cstdlib>
#include <utility>

namespace esphome::matrix_keypad {

class MatrixKeypadListener {
 public:
  virtual void button_pressed(int row, int col) {};
  virtual void button_released(int row, int col) {};
  virtual void key_pressed(uint8_t key) {};
  virtual void key_released(uint8_t key) {};
};

class MatrixKeyTrigger final : public Trigger<uint8_t> {};

class MatrixKeypad final : public key_provider::KeyProvider, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  void set_columns(std::vector<GPIOPin *> pins) { columns_ = std::move(pins); };
  void set_rows(std::vector<GPIOPin *> pins) { rows_ = std::move(pins); };
  /// `keys` is a codegen PROGMEM table with one key code per button.
  void set_keys(const uint8_t *keys) { this->keys_ = keys; }
  void set_debounce_time(uint32_t debounce_time) { debounce_time_ = debounce_time; };
  void set_has_diodes(bool has_diodes) { has_diodes_ = has_diodes; };
  void set_has_pulldowns(bool has_pulldowns) { has_pulldowns_ = has_pulldowns; };

#ifdef MATRIX_KEYPAD_LISTENER_COUNT
  SLOT_LIST(MatrixKeypadListener, listeners_, register_listener, MATRIX_KEYPAD_LISTENER_COUNT)
#else
  SLOT_LIST_EMPTY(MatrixKeypadListener, listeners_)
#endif
#ifdef MATRIX_KEYPAD_KEY_TRIGGER_COUNT
  SLOT_LIST(MatrixKeyTrigger, key_triggers_, register_key_trigger, MATRIX_KEYPAD_KEY_TRIGGER_COUNT)
#else
  SLOT_LIST_EMPTY(MatrixKeyTrigger, key_triggers_)
#endif

 protected:
  std::vector<GPIOPin *> rows_;
  std::vector<GPIOPin *> columns_;
  const uint8_t *keys_{nullptr};
  uint32_t debounce_time_ = 0;
  bool has_diodes_{false};
  bool has_pulldowns_{false};
  int pressed_key_ = -1;
  uint32_t active_start_{0};
  int active_key_{-1};
};

}  // namespace esphome::matrix_keypad
