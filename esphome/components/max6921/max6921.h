#pragma once

#include "esphome/core/component.h"
#include "esphome/core/time.h"

#include "esphome/components/spi/spi.h"

namespace esphome {
namespace max6921 {

class MAX6921 : public Component,
                public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                      spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1MHZ> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void write_data(uint8_t *ptr, size_t length);
  void set_load_pin(GPIOPin *load) { this->load_pin_ = load; }

 protected:
  GPIOPin *load_pin_{};
  void enable_load() { this->load_pin_->digital_write(true); }
  void disable_load() { this->load_pin_->digital_write(false); }
};

}  // namespace max6921
}  // namespace esphome
