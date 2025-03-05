#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace max6956 {

/// Modes for MAX6956 pins
enum MAX6956GPIOMode : uint8_t {
  MAX6956_LED = 0x00,
  MAX6956_OUTPUT = 0x01,
  MAX6956_INPUT = 0x02,
  MAX6956_INPUT_PULLUP = 0x03
};

/// Range for MAX6956 pins
enum MAX6956GPIORange : uint8_t {
  MAX6956_MIN = 4,
  MAX6956_MAX = 31,
};

enum MAX6956GPIORegisters {
  MAX6956_GLOBAL_CURRENT = 0x02,
  MAX6956_CONFIGURATION = 0x04,
  MAX6956_TRANSITION_DETECT_MASK = 0x06,
  MAX6956_DISPLAY_TEST = 0x07,
  MAX6956_PORT_CONFIG_START = 0x09,   // Port Configuration P7, P6, P5, P4
  MAX6956_CURRENT_START = 0x12,       // Current054
  MAX6956_1PORT_VALUE_START = 0x20,   // Port 0 only (virtual port, no action)
  MAX6956_8PORTS_VALUE_START = 0x44,  // 8 ports 4-11 (data bits D0-D7)
};

enum MAX6956GPIOFlag { FLAG_LED = 0x20 };

enum MAX6956CURRENTMODE { GLOBAL = 0x00, SEGMENT = 0x01 };

class MAX6956 : public Component, public i2c::I2CDevice {
 public:
  MAX6956() = default;

  void setup() override;

  bool digital_read(uint8_t pin);
  void digital_write(uint8_t pin, bool value);
  void pin_mode(uint8_t pin, gpio::Flags flags);
  void pin_mode(uint8_t pin, max6956::MAX6956GPIOFlag flags);

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void set_brightness_global(uint8_t current);
  void set_brightness_mode(max6956::MAX6956CURRENTMODE brightness_mode);
  void set_pin_brightness(uint8_t pin, float brightness);

  void dump_config() override;

  void write_brightness_global();
  void write_brightness_mode();

 protected:
  // read a given register
  bool read_reg_(uint8_t reg, uint8_t *value);
  // write a value to a given register
  bool write_reg_(uint8_t reg, uint8_t value);
  max6956::MAX6956CURRENTMODE brightness_mode_;
  uint8_t global_brightness_;

 private:
  int8_t prev_bright_[28] = {0};
};

class MAX6956GPIOPin : public GPIOPin {
 public:
  void setup() override;
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override;
  void digital_write(bool value) override;
  std::string dump_summary() const override;

  void set_parent(MAX6956 *parent) { parent_ = parent; }
  void set_pin(uint8_t pin) { pin_ = pin; }
  void set_inverted(bool inverted) { inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { flags_ = flags; }

  gpio::Flags get_flags() const override { return this->flags_; }

 protected:
  MAX6956 *parent_;
  uint8_t pin_;
  bool inverted_;
  gpio::Flags flags_;
};

}  // namespace max6956
}  // namespace esphome
