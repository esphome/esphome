#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace cap1188 {

enum {
  CAP1188_I2CADDR = 0x29,
  CAP1188_SENINPUTSTATUS = 0x3,
  CAP1188_MULTITOUCH = 0x2A,
  CAP1188_LEDLINK = 0x72,
  CAP1188_PRODID = 0xFD,
  CAP1188_MANUID = 0xFE,
  CAP1188_STANDBYCFG = 0x41,
  CAP1188_REV = 0xFF,
  CAP1188_MAIN = 0x00,
  CAP1188_MAIN_INT = 0x01,
  CAP1188_LEDPOL = 0x73,
  CAP1188_INTERUPT_REPEAT = 0x28,
  CAP1188_SENSITVITY = 0x1f,
};

class CAP1188Channel : public binary_sensor::BinarySensor {
  friend class CAP1188Component;

 public:
  void set_channel(uint8_t channel) { channel_ = channel; }
  void process(uint8_t data) { this->publish_state(static_cast<bool>(data & (1 << this->channel_))); }
  void set_touch_threshold(uint8_t touch_threshold) { this->touch_threshold_ = touch_threshold; };

 protected:
  uint8_t channel_{0};
  optional<uint8_t> touch_threshold_{};
};

class CAP1188Component : public Component, public i2c::I2CDevice, public output::BinaryOutput {
 public:
  void set_pin(GPIOPin *pin) { pin_ = pin; }
  void register_channel(CAP1188Channel *channel) { this->channels_.push_back(channel); }
  void set_touch_threshold(uint8_t touch_threshold) { this->touch_threshold_ = touch_threshold; };
  void set_allow_multiple_touches(uint8_t allow_multiple_touches) {
    this->allow_multiple_touches_ = allow_multiple_touches;
  };
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void loop() override;

 protected:
  std::vector<CAP1188Channel *> channels_{};
  uint8_t touch_threshold_{};
  uint8_t allow_multiple_touches_{0x80};
  void write_state(bool state) override { this->pin_->digital_write(state); }

  GPIOPin *pin_;
  enum ErrorCode {
    NONE = 0,
    COMMUNICATION_FAILED,
  } error_code_{NONE};
};

}  // namespace cap1188
}  // namespace esphome
