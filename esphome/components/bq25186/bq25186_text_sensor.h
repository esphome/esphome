#pragma once

#include "esphome/core/defines.h"
#ifdef USE_TEXT_SENSOR

#include "bq25186.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::bq25186 {

enum class BQ25186TextSensorKind : uint8_t { CHARGE_STATUS, TS_STATUS };

class BQ25186TextSensor final : public text_sensor::TextSensor, public BQ25186Listener {
 public:
  explicit BQ25186TextSensor(BQ25186TextSensorKind kind) : kind_(kind) {}
  void on_status(const BQ25186StatusRegisters &status) override;

 protected:
  const BQ25186TextSensorKind kind_;
  uint8_t last_{0xFF};  // the two-bit field last published
};

}  // namespace esphome::bq25186

#endif  // USE_TEXT_SENSOR
