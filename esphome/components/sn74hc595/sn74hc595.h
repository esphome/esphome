#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

#ifdef USE_SPI
#include "esphome/components/spi/spi.h"
#endif

#include <vector>

namespace esphome {
namespace sn74hc595 {

class SN74HC595Component : public Component {
 public:
  SN74HC595Component() = default;

  void setup() override = 0;
  float get_setup_priority() const override;
  void dump_config() override;

  void set_latch_pin(GPIOPin *pin) { this->latch_pin_ = pin; }
  void set_oe_pin(GPIOPin *pin) {
    this->oe_pin_ = pin;
    this->have_oe_pin_ = true;
  }
  void set_sr_count(uint8_t count) {
    this->sr_count_ = count;
    this->output_bytes_.resize(count);
  }

 protected:
  friend class SN74HC595GPIOPin;
  void digital_write_(uint16_t pin, bool value);
  virtual void write_gpio();

  void pre_setup_();
  void post_setup_();

  GPIOPin *latch_pin_;
  GPIOPin *oe_pin_;
  uint8_t sr_count_;
  bool have_oe_pin_{false};
  std::vector<uint8_t> output_bytes_;
};

/// Helper class to expose a SC74HC595 pin as an internal output GPIO pin.
class SN74HC595GPIOPin : public GPIOPin, public Parented<SN74HC595Component> {
 public:
  void setup() override {}
  void pin_mode(gpio::Flags flags) override {}
  bool digital_read() override { return false; }
  void digital_write(bool value) override;
  std::string dump_summary() const override;

  void set_pin(uint16_t pin) { pin_ = pin; }
  void set_inverted(bool inverted) { inverted_ = inverted; }

 protected:
  uint16_t pin_;
  bool inverted_;
};

class SN74HC595GPIOComponent : public SN74HC595Component {
 public:
  void setup() override;
  void set_data_pin(GPIOPin *pin) { data_pin_ = pin; }
  void set_clock_pin(GPIOPin *pin) { clock_pin_ = pin; }

 protected:
  void write_gpio() override;

  GPIOPin *data_pin_;
  GPIOPin *clock_pin_;
};

#ifdef USE_SPI
class SN74HC595SPIComponent : public SN74HC595Component,
                              public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                                    spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_4MHZ> {
 public:
  void setup() override;

 protected:
  void write_gpio() override;
};

#endif

}  // namespace sn74hc595
}  // namespace esphome
