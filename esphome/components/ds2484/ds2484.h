#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/preferences.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/one_wire/one_wire.h"

namespace esphome {
namespace ds2484 {

class DS2484OneWireBus : public one_wire::OneWireBus, public i2c::I2CDevice, public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS - 1.0; }

  bool reset_device();
  int reset_int() override;
  void write8(uint8_t) override;
  void write64(uint64_t) override;
  uint8_t read8() override;
  uint64_t read64() override;

  void set_active_pullup(bool value) { this->active_pullup_ = value; }
  void set_strong_pullup(bool value) { this->strong_pullup_ = value; }

 protected:
  void reset_search() override;
  uint64_t search_int() override;
  bool read_status_(uint8_t *);
  bool wait_for_completion_();
  void write8_(uint8_t);
  bool one_wire_triple_(bool *branch, bool *id_bit, bool *cmp_id_bit);

  uint64_t address_;
  uint8_t last_discrepancy_{0};
  bool last_device_flag_{false};
  bool active_pullup_{false};
  bool strong_pullup_{false};
};
}  // namespace ds2484
}  // namespace esphome
