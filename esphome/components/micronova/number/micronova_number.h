#pragma once

#include "esphome/components/micronova/micronova.h"
#include "esphome/components/number/number.h"

namespace esphome::micronova {

class MicroNovaNumber final : public number::Number, public MicroNovaListener {
 public:
  MicroNovaNumber(MicroNova *m) : MicroNovaListener(m) {}
  void dump_config() override;
  void control(float value) override;
  void process_value_from_stove(int value_from_stove) override;

  void set_use_step_scaling(bool v) { this->use_step_scaling_ = v; }
  void set_value_offset(uint8_t offset) { this->value_offset_ = offset; }
  /// Also write the same raw value to a second location/address (e.g. the RAM copy of an EEPROM setting)
  void set_mirror_write(uint8_t location, uint8_t address) {
    this->mirror_location_ = location;
    this->mirror_address_ = address;
    this->has_mirror_write_ = true;
  }

 protected:
  bool use_step_scaling_ = false;
  bool has_mirror_write_ = false;
  uint8_t value_offset_ = 0;
  uint8_t mirror_location_ = 0;
  uint8_t mirror_address_ = 0;
};

}  // namespace esphome::micronova
