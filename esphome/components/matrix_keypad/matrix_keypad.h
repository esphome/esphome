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

  void register_listener(MatrixKeypadListener *listener);
#ifdef MATRIX_KEYPAD_ON_KEY_COUNT
  void add_on_key_automation(Automation<uint8_t> *automation) { this->key_automations_.push_back(automation); }
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

  std::vector<MatrixKeypadListener *> listeners_{};
#ifdef MATRIX_KEYPAD_ON_KEY_COUNT
  StaticVector<Automation<uint8_t> *, MATRIX_KEYPAD_ON_KEY_COUNT> key_automations_;
#endif
};

}  // namespace esphome::matrix_keypad
