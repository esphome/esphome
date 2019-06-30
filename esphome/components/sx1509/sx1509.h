#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "sx1509_registers.h"
#include "sx1509_float_output.h"
#include "sx1509_gpio_pin.h"
#include "sx1509_keypad_sensor.h"

namespace esphome {
namespace sx1509 {

// These are used for clock config:
static const uint8_t INTERNAL_CLOCK_2MHZ = 2;
static const uint8_t EXTERNAL_CLOCK = 1;
static const uint8_t SOFTWARE_RESET = 0;
static const uint8_t HARDWARE_RESET = 1;

class SX1509FloatOutputChannel;
class SX1509GPIOPin;

class SX1509Component : public Component, public i2c::I2CDevice {
 public:
  SX1509Component() {}

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void pin_mode_(uint8_t pin, uint8_t in_out);
  void digital_write_(uint8_t pin, bool high_low);
  bool digital_read(uint8_t pin);
  void set_pin_value_(uint8_t pin, uint8_t i_on);
  void setup_blink(uint8_t pin, uint8_t t_on, uint8_t t_off, uint8_t on_intensity = 255, uint8_t off_intensity = 0,
                   uint8_t t_rise = 0, uint8_t t_fall = 0, bool log = false);
  void setup_keypad(uint8_t rows, uint8_t columns, uint16_t sleep_time = 0, uint8_t scan_time = 1,
                    uint8_t debounce_time = 0);
  uint16_t read_key_data();

 protected:
  friend class SX1509FloatOutputChannel;
  friend class SX1509GPIOPin;
  // variables:
  u_long clk_x_;
  uint8_t frequency_ = 0;
  uint16_t ddr_mask_{0x00};
  uint16_t input_mask_{0x00};
  uint16_t port_mask_{0x00};
  bool update_{true};

  void debounce_config_(uint8_t config_vaule);
  void debounce_time_(uint8_t time);
  void debounce_pin_(uint8_t pin);
  void debounce_enable_(uint8_t pin);  // Legacy, use debouncePin
  void debounce_keypad_(uint8_t time, uint8_t num_rows, uint8_t num_cols);
  void setup_led_driver_(uint8_t pin, uint8_t freq = 1, bool log = false);
  void clock_(uint8_t osc_source = 2, uint8_t osc_divider = 1, uint8_t osc_pin_function = 0, uint8_t osc_freq_out = 0);

  uint8_t REG_I_ON[16] = {REG_I_ON_0,  REG_I_ON_1,  REG_I_ON_2,  REG_I_ON_3, REG_I_ON_4,  REG_I_ON_5,
                          REG_I_ON_6,  REG_I_ON_7,  REG_I_ON_8,  REG_I_ON_9, REG_I_ON_10, REG_I_ON_11,
                          REG_I_ON_12, REG_I_ON_13, REG_I_ON_14, REG_I_ON_15};

  uint8_t REG_T_ON[16] = {REG_T_ON_0,  REG_T_ON_1,  REG_T_ON_2,  REG_T_ON_3, REG_T_ON_4,  REG_T_ON_5,
                          REG_T_ON_6,  REG_T_ON_7,  REG_T_ON_8,  REG_T_ON_9, REG_T_ON_10, REG_T_ON_11,
                          REG_T_ON_12, REG_T_ON_13, REG_T_ON_14, REG_T_ON_15};

  uint8_t REG_OFF[16] = {REG_OFF_0, REG_OFF_1, REG_OFF_2,  REG_OFF_3,  REG_OFF_4,  REG_OFF_5,  REG_OFF_6,  REG_OFF_7,
                         REG_OFF_8, REG_OFF_9, REG_OFF_10, REG_OFF_11, REG_OFF_12, REG_OFF_13, REG_OFF_14, REG_OFF_15};

  uint8_t REG_T_RISE[16] = {0xFF, 0xFF, 0xFF, 0xFF, REG_T_RISE_4,  REG_T_RISE_5,  REG_T_RISE_6,  REG_T_RISE_7,
                            0xFF, 0xFF, 0xFF, 0xFF, REG_T_RISE_12, REG_T_RISE_13, REG_T_RISE_14, REG_T_RISE_15};

  uint8_t REG_T_FALL[16] = {0xFF, 0xFF, 0xFF, 0xFF, REG_T_FALL_4,  REG_T_FALL_5,  REG_T_FALL_6,  REG_T_FALL_7,
                            0xFF, 0xFF, 0xFF, 0xFF, REG_T_FALL_12, REG_T_FALL_13, REG_T_FALL_14, REG_T_FALL_15};
};

}  // namespace sx1509
}  // namespace esphome
