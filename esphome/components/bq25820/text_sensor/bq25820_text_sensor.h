#pragma once

#include "esphome/core/defines.h"
#ifdef USE_TEXT_SENSOR

#include "../bq25820.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::bq25820 {

/// A three-bit status field; `first` is the string table index of the field's value 0
class BQ25820TextSensor final : public text_sensor::TextSensor {
 public:
  BQ25820TextSensor(uint8_t reg, uint8_t shift, uint8_t first) : reg_(reg), shift_(shift), first_(first) {}
  void on_status(const BQ25820StatusRegisters &status);

 protected:
  const uint8_t reg_;
  const uint8_t shift_;
  const uint8_t first_;
  uint8_t last_{0xFF};  // the field value last published
};

}  // namespace esphome::bq25820

#endif  // USE_TEXT_SENSOR
