#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sensirion_common/i2c_sensirion.h"

namespace esphome::stcc4 {

enum class MeasurementMode : uint8_t {
  CONTINUOUS = 0,
  SINGLE_SHOT,
};

class STCC4Component final : public PollingComponent, public sensirion_common::SensirionI2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;

  void set_co2_sensor(sensor::Sensor *co2) { this->co2_sensor_ = co2; }
  void set_temperature_sensor(sensor::Sensor *temperature) { this->temperature_sensor_ = temperature; }
  void set_humidity_sensor(sensor::Sensor *humidity) { this->humidity_sensor_ = humidity; }
  void set_temperature_source(sensor::Sensor *temperature) { this->temperature_source_ = temperature; }
  void set_humidity_source(sensor::Sensor *humidity) { this->humidity_source_ = humidity; }
  void set_ambient_pressure_compensation(float pressure_in_hpa);
  void set_ambient_pressure_source(sensor::Sensor *pressure) { this->ambient_pressure_source_ = pressure; }
  void set_measurement_mode(MeasurementMode mode) { this->measurement_mode_ = mode; }

 protected:
  void finish_setup_();
  void read_measurement_();
  void update_rht_compensation_from_source_();
  void update_ambient_pressure_compensation_from_source_();
  bool write_ambient_pressure_compensation_();

  sensor::Sensor *co2_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *temperature_source_{nullptr};
  sensor::Sensor *humidity_source_{nullptr};
  sensor::Sensor *ambient_pressure_source_{nullptr};

  MeasurementMode measurement_mode_{MeasurementMode::CONTINUOUS};

  bool ready_{false};
  uint16_t temperature_ticks_{0};
  uint16_t humidity_ticks_{0};
  uint16_t ambient_pressure_in_pa_2_{0};
};

}  // namespace esphome::stcc4
