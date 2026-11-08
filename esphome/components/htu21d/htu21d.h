#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::htu21d {

enum HTU21DSensorModels { HTU21D_SENSOR_MODEL_HTU21D = 0, HTU21D_SENSOR_MODEL_SI7021, HTU21D_SENSOR_MODEL_SHT21 };

class HTU21DComponent final : public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(temperature)
  SUB_SENSOR(humidity)
  SUB_SENSOR(heater)

 public:
  /// Setup (reset) the sensor and check connection.
  void setup() override;
  void dump_config() override;
  void set_sensor_model(HTU21DSensorModels sensor_model) { sensor_model_ = sensor_model; }
  /// Update the sensor values (temperature+humidity).
  void update() override;

  bool is_heater_enabled();
  void set_heater(bool status);
  void set_heater_level(uint8_t level);
  uint8_t get_heater_level();

 protected:
  HTU21DSensorModels sensor_model_{HTU21D_SENSOR_MODEL_HTU21D};
};

}  // namespace esphome::htu21d
