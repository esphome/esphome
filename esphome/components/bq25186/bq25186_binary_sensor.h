#pragma once

#include "esphome/core/defines.h"
#ifdef USE_BINARY_SENSOR

#include "bq25186.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome::bq25186 {

/// One field of a status register; `match` has a bit set for each field value that reads as on
class BQ25186BinarySensor final : public binary_sensor::BinarySensor {
 public:
  BQ25186BinarySensor(uint8_t reg, uint8_t shift, uint8_t mask, uint8_t match)
      : reg_(reg), shift_(shift), mask_(mask), match_(match) {}

  void on_status(const BQ25186StatusRegisters &status) {
    const uint8_t value = (status[this->reg_] >> this->shift_) & this->mask_;
    this->publish_state((this->match_ >> value) & 1);
  }

 protected:
  const uint8_t reg_;
  const uint8_t shift_;
  const uint8_t mask_;
  const uint8_t match_;
};

}  // namespace esphome::bq25186

#endif  // USE_BINARY_SENSOR
