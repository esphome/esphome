#pragma once

#include "esphome/core/log.h"
#include "esphome/core/gpio.h"

namespace esphome {
namespace byte_bus {

/**
 * A pin to replace those that don't exist.
 */
class NullPin : public GPIOPin {
 public:
  void setup() override {}

  void pin_mode(gpio::Flags _) override {}

  bool digital_read() override { return false; }

  void digital_write(bool _) override {}

  std::string dump_summary() const override { return {"Not used"}; }
};

// https://bugs.llvm.org/show_bug.cgi?id=48040
static GPIOPin *const NULL_PIN = new NullPin();  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

/**
 * A class that can write a byte-oriented bus interface with CS and DC controls, typically used for
 * LCD display controller interfacing.
 */
class ByteBus {
 public:
  virtual void bus_setup() = 0;

  virtual void bus_teardown() = 0;

  /**
   * Write a data array. Will not control dc or cs pins.
   * @param data
   * @param length
   */
  virtual void write_array(const uint8_t *data, size_t length) = 0;

  /**
   * Write a command followed by data. CS and DC will be controlled automatically.
   * On return, the DC pin will be in DATA mode.
   * @param cmd
   * @param data
   * @param length
   */
  virtual void write_cmd_data(int cmd, const uint8_t *data, size_t length) = 0;

  virtual void dump_config(){};

  virtual void begin_transaction() = 0;
  virtual void end_transaction() = 0;

  void set_dc_pin(GPIOPin *dc_pin) { this->dc_pin_ = dc_pin; }

  /**
   * Select data (true) or control mode.
   * @param data True for data mode, false for control
   */
  void set_dc_data(bool data) { this->dc_pin_->digital_write(data); }

 protected:
  GPIOPin *dc_pin_{byte_bus::NULL_PIN};
};
}  // namespace byte_bus
}  // namespace esphome
