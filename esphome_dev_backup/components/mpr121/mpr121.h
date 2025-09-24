#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"

#include "esphome/components/i2c/i2c.h"

#include <vector>

namespace esphome {
namespace mpr121 {

enum {
  MPR121_TOUCHSTATUS_L = 0x00,
  MPR121_TOUCHSTATUS_H = 0x01,
  MPR121_FILTDATA_0L = 0x04,
  MPR121_FILTDATA_0H = 0x05,
  MPR121_BASELINE_0 = 0x1E,
  MPR121_MHDR = 0x2B,
  MPR121_NHDR = 0x2C,
  MPR121_NCLR = 0x2D,
  MPR121_FDLR = 0x2E,
  MPR121_MHDF = 0x2F,
  MPR121_NHDF = 0x30,
  MPR121_NCLF = 0x31,
  MPR121_FDLF = 0x32,
  MPR121_NHDT = 0x33,
  MPR121_NCLT = 0x34,
  MPR121_FDLT = 0x35,
  MPR121_TOUCHTH_0 = 0x41,
  MPR121_RELEASETH_0 = 0x42,
  MPR121_DEBOUNCE = 0x5B,
  MPR121_CONFIG1 = 0x5C,
  MPR121_CONFIG2 = 0x5D,
  MPR121_CHARGECURR_0 = 0x5F,
  MPR121_CHARGETIME_1 = 0x6C,
  MPR121_ECR = 0x5E,
  MPR121_AUTOCONFIG0 = 0x7B,
  MPR121_AUTOCONFIG1 = 0x7C,
  MPR121_UPLIMIT = 0x7D,
  MPR121_LOWLIMIT = 0x7E,
  MPR121_TARGETLIMIT = 0x7F,
  MPR121_GPIOCTL0 = 0x73,
  MPR121_GPIOCTL1 = 0x74,
  MPR121_GPIODATA = 0x75,
  MPR121_GPIODIR = 0x76,
  MPR121_GPIOEN = 0x77,
  MPR121_GPIOSET = 0x78,
  MPR121_GPIOCLR = 0x79,
  MPR121_GPIOTOGGLE = 0x7A,
  MPR121_SOFTRESET = 0x80,
};

class MPR121Channel {
 public:
  virtual void setup() = 0;
  virtual void process(uint16_t data) = 0;
};

class MPR121Component : public Component, public i2c::I2CDevice {
 public:
  void register_channel(MPR121Channel *channel) { this->channels_.push_back(channel); }
  void set_touch_debounce(uint8_t debounce);
  void set_release_debounce(uint8_t debounce);
  void set_touch_threshold(uint8_t touch_threshold) { this->touch_threshold_ = touch_threshold; };
  void set_release_threshold(uint8_t release_threshold) { this->release_threshold_ = release_threshold; };
  uint8_t get_touch_threshold() const { return this->touch_threshold_; };
  uint8_t get_release_threshold() const { return this->release_threshold_; };
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::IO; }
  void loop() override;

  void set_max_touch_channel(uint8_t max_touch_channel) { this->max_touch_channel_ = max_touch_channel; }

  // GPIO helper functions.
  bool digital_read(uint8_t ionum);
  void digital_write(uint8_t ionum, bool value);
  void pin_mode(uint8_t ionum, gpio::Flags flags);

 protected:
  std::vector<MPR121Channel *> channels_{};
  uint8_t debounce_{0};
  uint8_t touch_threshold_{};
  uint8_t release_threshold_{};
  uint8_t max_touch_channel_{3};
  enum ErrorCode {
    NONE = 0,
    COMMUNICATION_FAILED,
    WRONG_CHIP_STATE,
  } error_code_{NONE};

  bool flush_gpio_();

  /// The enable mask - zero means high Z, 1 means GPIO usage
  uint8_t gpio_enable_{0x00};
  /// Mask for the pin mode - 1 means output, 0 means input
  uint8_t gpio_direction_{0x00};
  /// The mask to write as output state - 1 means HIGH, 0 means LOW
  uint8_t gpio_output_{0x00};
  /// The mask to read as input state - 1 means HIGH, 0 means LOW
  uint8_t gpio_input_{0x00};
};

/// Helper class to expose a MPR121 pin as an internal input GPIO pin.
class MPR121GPIOPin : public GPIOPin {
 public:
  void setup() override;
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override;
  void digital_write(bool value) override;
  std::string dump_summary() const override;

  void set_parent(MPR121Component *parent) { this->parent_ = parent; }
  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_inverted(bool inverted) { this->inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { this->flags_ = flags; }

  gpio::Flags get_flags() const override { return this->flags_; }

 protected:
  MPR121Component *parent_;
  uint8_t pin_;
  bool inverted_;
  gpio::Flags flags_;
};

}  // namespace mpr121
}  // namespace esphome
