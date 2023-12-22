#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "sx1509_gpio_pin.h"
#include "sx1509_registers.h"

#include <vector>

namespace esphome {
namespace sx1509 {

// These are used for clock config:
const uint8_t INTERNAL_CLOCK_2MHZ = 2;
const uint8_t EXTERNAL_CLOCK = 1;
const uint8_t SOFTWARE_RESET = 0;
const uint8_t HARDWARE_RESET = 1;

const uint8_t REG_I_ON[16] = {REG_I_ON_0,  REG_I_ON_1,  REG_I_ON_2,  REG_I_ON_3, REG_I_ON_4,  REG_I_ON_5,
                              REG_I_ON_6,  REG_I_ON_7,  REG_I_ON_8,  REG_I_ON_9, REG_I_ON_10, REG_I_ON_11,
                              REG_I_ON_12, REG_I_ON_13, REG_I_ON_14, REG_I_ON_15};

// for all components that implement the process(uint16_t data )
class SX1509Processor {
 public:
  virtual void process(uint16_t data){};
};

class SX1509Component : public Component, public i2c::I2CDevice {
 public:
  SX1509Component() = default;

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void loop() override;

  bool digital_read(uint8_t pin);
  uint16_t read_key_data();
  void set_pin_value(uint8_t pin, uint8_t i_on) { this->write_byte(REG_I_ON[pin], i_on); };
  void pin_mode(uint8_t pin, gpio::Flags flags);
  void digital_write(uint8_t pin, bool bit_value);
  uint32_t get_clock() { return this->clk_x_; };
  void set_rows_cols(uint8_t rows, uint8_t cols) {
    this->rows_ = rows;
    this->cols_ = cols;
    this->has_keypad_ = true;
  };
  void set_sleep_time(uint16_t sleep_time) { this->sleep_time_ = sleep_time; };
  void set_scan_time(uint8_t scan_time) { this->scan_time_ = scan_time; };
  void set_debounce_time(uint8_t debounce_time = 1) { this->debounce_time_ = debounce_time; };
  void register_keypad_binary_sensor(SX1509Processor *binary_sensor) {
    this->keypad_binary_sensors_.push_back(binary_sensor);
  }
  void setup_led_driver(uint8_t pin);

 protected:
  uint32_t clk_x_ = 2000000;
  uint8_t frequency_ = 0;
  uint16_t ddr_mask_ = 0x00;
  uint16_t input_mask_ = 0x00;
  uint16_t port_mask_ = 0x00;
  uint16_t output_state_ = 0x00;
  bool has_keypad_ = false;
  uint8_t rows_ = 0;
  uint8_t cols_ = 0;
  uint16_t sleep_time_ = 128;
  uint8_t scan_time_ = 1;
  uint8_t debounce_time_ = 1;
  std::vector<SX1509Processor *> keypad_binary_sensors_;

  uint32_t last_loop_timestamp_ = 0;
  const uint32_t min_loop_period_ = 15;  // ms

  void setup_keypad_();
  void set_debounce_config_(uint8_t config_value);
  void set_debounce_time_(uint8_t time);
  void set_debounce_pin_(uint8_t pin);
  void set_debounce_enable_(uint8_t pin);
  void set_debounce_keypad_(uint8_t time, uint8_t num_rows, uint8_t num_cols);
  void clock_(uint8_t osc_source = 2, uint8_t osc_pin_function = 1, uint8_t osc_freq_out = 0, uint8_t osc_divider = 0);
};

}  // namespace sx1509
}  // namespace esphome
