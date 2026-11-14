#pragma once

#include <cstdint>

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::xgzp68xx {

/// XGZP6899DC: the XGZP6899D revision at I2C address 0x58 (datasheet V3.2), with a new register map.
class XGZP6899DCComponent final : public PollingComponent, public i2c::I2CDevice {
 public:
  /// @param pressure_span Rated range of the part in Pa (PMAX - PMIN), for example 1000 for a +/-500 Pa part.
  explicit XGZP6899DCComponent(float pressure_span);

  SUB_SENSOR(temperature)
  SUB_SENSOR(pressure)
  /// Set the CFG_OSR pressure oversampling code (0 = 256X ... 7 = 32768X). Unset keeps the factory value.
  void set_pressure_oversampling(uint8_t pressure_oversampling) {
    this->pressure_oversampling_ = pressure_oversampling;
  }

  void update() override;
  void setup() override;
  void dump_config() override;

 protected:
  void read_data_();

  // Pa per raw count
  float pressure_scale_;
  // Temperature = raw * temperature_scale_ + temperature_bias_, from the calibration registers
  float temperature_scale_{0.0f};
  float temperature_bias_{0.0f};
  uint8_t pressure_oversampling_{UINT8_MAX};
};

}  // namespace esphome::xgzp68xx
