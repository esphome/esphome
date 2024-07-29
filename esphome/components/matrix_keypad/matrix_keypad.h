#pragma once

#include "esphome/components/key_provider/key_provider.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include <cstdlib>
#include <utility>

namespace esphome {
namespace matrix_keypad {

class MatrixKeypadListener {
 public:
  virtual void button_pressed(int row, int col){};
  virtual void button_released(int row, int col){};
  virtual void key_pressed(uint8_t key){};
  virtual void key_released(uint8_t key){};
};

class MatrixKeypad : public key_provider::KeyProvider, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  void set_columns(std::vector<GPIOPin *> pins) { columns_ = std::move(pins); };
  void set_rows(std::vector<GPIOPin *> pins) { rows_ = std::move(pins); };
  void set_keys(std::string keys) { keys_ = std::move(keys); };
  void set_debounce_time(int debounce_time) { debounce_time_ = debounce_time; };
  void set_has_diodes(int has_diodes) { has_diodes_ = has_diodes; };
  void set_has_pulldowns(int has_pulldowns) { has_pulldowns_ = has_pulldowns; };

  void register_listener(MatrixKeypadListener *listener);

 protected:
  std::vector<GPIOPin *> rows_;
  std::vector<GPIOPin *> columns_;
  std::string keys_;
  int debounce_time_ = 0;
  bool has_diodes_{false};
  bool has_pulldowns_{false};
  int pressed_key_ = -1;

  std::vector<MatrixKeypadListener *> listeners_{};
};

}  // namespace matrix_keypad
}  // namespace esphome
