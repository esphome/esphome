#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "sx1509_registers.h"
#include "sx1509_float_output.h"
#include "sx1509_gpio_pin.h"
#include "sx1509_keypad_sensor.h"

namespace esphome {
namespace sx1509 {

// These are used for setting LED driver to linear or log mode:
#define LINEAR 0
#define LOGARITHMIC 1

// These are used for clock config:
#define INTERNAL_CLOCK_2MHZ 2
#define EXTERNAL_CLOCK 1

#define SOFTWARE_RESET 0
#define HARDWARE_RESET 1

#define ANALOG_OUTPUT 0x03  // To set a pin mode for PWM output

class SX1509FloatOutputChannel;

class SX1509Component : public Component, public i2c::I2CDevice {
 public:
  SX1509Component() {}

  // SX1509FloatOutputChannel *create_float_output_channel(uint8_t pin);

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void loop() override;
  void pin_mode(uint8_t pin, uint8_t inOut);
  void led_driver_init(uint8_t pin, uint8_t freq = 1, bool log = false);
  void clock(uint8_t oscSource = 2, uint8_t oscDivider = 1, uint8_t oscPinFunction = 0, uint8_t oscFreqOut = 0);
  void digital_write(uint8_t pin, uint8_t highLow);
  uint8_t digital_read(uint8_t pin);
  void set_pin_value_(uint8_t pin, uint8_t iOn);

  void setup_blink(uint8_t pin, uint8_t tOn, uint8_t toff, uint8_t onIntensity = 255, uint8_t offIntensity = 0,
                   uint8_t tRise = 0, uint8_t tFall = 0, bool log = false);
  void blink(uint8_t pin, unsigned long tOn, unsigned long tOff, uint8_t onIntensity = 255, uint8_t offIntensity = 0);
  void breathe(uint8_t pin, unsigned long tOn, unsigned long tOff, unsigned long rise, unsigned long fall,
               uint8_t onInt = 255, uint8_t offInt = 0, bool log = LINEAR);
  uint8_t calculate_led_t_register(uint16_t ms);
  uint8_t calculate_slope_register(uint16_t ms, uint8_t onIntensity, uint8_t offIntensity);
  void setup_keypad(uint8_t rows, uint8_t columns, uint16_t sleepTime = 0, uint8_t scanTime = 1,
                    uint8_t debounceTime = 0);
  void debounce_config(uint8_t configVaule);
  void debounce_time(uint8_t time);
  void debounce_pin(uint8_t pin);
  void debounce_enable(uint8_t pin);  // Legacy, use debouncePin
  void debounce_keypad(uint8_t time, uint8_t numRows, uint8_t numCols);

 protected:
  friend class SX1509FloatOutputChannel;
  // std::vector<SX1509FloatOutputChannel *> float_output_channels_{};

  // Pin definitions:
  uint8_t pinInterrupt_;
  uint8_t pinOscillator_;
  uint8_t pinReset_;
  // variables:
  u_long _clkX;
  uint8_t frequency_ = 0;

  bool update_{true};

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
