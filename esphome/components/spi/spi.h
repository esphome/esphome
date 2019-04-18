#pragma once

#include "esphome/core/component.h"
#include "esphome/core/esphal.h"

namespace esphome {
namespace spi {

class SPIComponent : public Component {
 public:
  void set_clk(GPIOPin *clk) { clk_ = clk; }
  void set_miso(GPIOPin *miso) { miso_ = miso; }
  void set_mosi(GPIOPin *mosi) { mosi_ = mosi; }

  void setup() override;

  void dump_config() override;

  uint8_t read_byte();

  void read_array(uint8_t *data, size_t length);

  void write_byte(uint8_t data);

  void write_array(uint8_t *data, size_t length);

  void enable(GPIOPin *cs, bool msb_first, bool high_speed);

  void disable();

  float get_setup_priority() const override;

 protected:
  GPIOPin *clk_;
  GPIOPin *miso_{nullptr};
  GPIOPin *mosi_{nullptr};
  GPIOPin *active_cs_{nullptr};
  bool msb_first_{true};
  bool high_speed_{false};
};

class SPIDevice {
 public:
  SPIDevice() = default;
  SPIDevice(SPIComponent *parent, GPIOPin *cs) : parent_(parent), cs_(cs) {}

  void set_spi_parent(SPIComponent *parent) { this->parent_ = parent; }
  void set_cs_pin(GPIOPin *cs) { this->cs_ = cs; }

  void spi_setup() {
    this->cs_->setup();
    this->cs_->digital_write(true);
  }

  void enable() { this->parent_->enable(this->cs_, this->is_device_msb_first(), this->is_device_high_speed()); }

  void disable() { this->parent_->disable(); }

  uint8_t read_byte() { return this->parent_->read_byte(); }

  void read_array(uint8_t *data, size_t length) { return this->parent_->read_array(data, length); }

  void write_byte(uint8_t data) { return this->parent_->write_byte(data); }

  void write_array(uint8_t *data, size_t length) { this->parent_->write_array(data, length); }

 protected:
  virtual bool is_device_msb_first() = 0;

  virtual bool is_device_high_speed() { return false; }

  SPIComponent *parent_{nullptr};
  GPIOPin *cs_{nullptr};
};

}  // namespace spi
}  // namespace esphome
