#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/one_wire/one_wire.h"

namespace esphome {
namespace gpio {

class GPIOOneWireBus : public one_wire::OneWireBus, public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  void set_pin(InternalGPIOPin *pin) {
    this->t_pin_ = pin;
    this->pin_ = pin->to_isr();
  }

  bool reset() override;
  void write8(uint8_t val) override;
  void write64(uint64_t val) override;
  uint8_t read8() override;
  uint64_t read64() override;

 protected:
  InternalGPIOPin *t_pin_;
  ISRInternalGPIOPin pin_;
  uint8_t last_discrepancy_{0};
  bool last_device_flag_{false};
  uint64_t address_;

  void reset_search() override;
  uint64_t search_int() override;
  void write_bit_(bool bit);
  bool read_bit_();
};

}  // namespace gpio
}  // namespace esphome
