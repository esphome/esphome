#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace xgzp68xx {

/// Enum listing all oversampling options for the XGZP68XX.
enum XGZP68XXOversampling : uint8_t {
  XGZP68XX_OVERSAMPLING_256X = 0b100,
  XGZP68XX_OVERSAMPLING_512X = 0b101,
  XGZP68XX_OVERSAMPLING_1024X = 0b000,
  XGZP68XX_OVERSAMPLING_2048X = 0b001,
  XGZP68XX_OVERSAMPLING_4096X = 0b010,
  XGZP68XX_OVERSAMPLING_8192X = 0b011,
  XGZP68XX_OVERSAMPLING_16384X = 0b110,
  XGZP68XX_OVERSAMPLING_32768X = 0b111,

  XGZP68XX_OVERSAMPLING_UNKNOWN = (uint8_t) -1,
};

class XGZP68XXComponent : public PollingComponent, public sensor::Sensor, public i2c::I2CDevice {
 public:
  SUB_SENSOR(temperature)
  SUB_SENSOR(pressure)
  void set_k_value(uint16_t k_value) { this->k_value_ = k_value; }
  /// Set the pressure oversampling value. Defaults to 4096X.
  void set_pressure_oversampling(XGZP68XXOversampling pressure_oversampling) {
    this->pressure_oversampling_ = pressure_oversampling;
  }

  void update() override;
  void setup() override;
  void dump_config() override;

 protected:
  /// Internal method to read the pressure from the component after it has been scheduled.
  void read_pressure_();
  uint16_t k_value_;
  XGZP68XXOversampling pressure_oversampling_{XGZP68XX_OVERSAMPLING_4096X};
  XGZP68XXOversampling last_pressure_oversampling_{XGZP68XX_OVERSAMPLING_UNKNOWN};
};

}  // namespace xgzp68xx
}  // namespace esphome
