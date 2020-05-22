#pragma once

#include "esphome/core/component.h"
#include "esphome/core/esphal.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace sn74hc595 {

class SN74HC595 : public Component,
                  public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                        spi::DATA_RATE_4MHZ> {
 public:
  SN74HC595() = default;
  void setup() override;
  float get_setup_priority() const override;

  void digital_write(uint8_t pin, bool value);
  int olat_{0x00};
};

class SN74HC595GpioPin : public GPIOPin {
 public:
  SN74HC595GpioPin(SN74HC595 *parent, uint8_t pin, bool inverted = false);
  void digital_write(bool value) override;

 protected:
  SN74HC595 *parent_;
};

}  // namespace sn74hc595
}  // namespace esphome
