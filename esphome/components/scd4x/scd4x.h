#pragma once
#include <vector>
#include "esphome/core/application.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sensirion_common/i2c_sensirion.h"

namespace esphome {
namespace scd4x {

enum ERRORCODE {
  COMMUNICATION_FAILED,
  SERIAL_NUMBER_IDENTIFICATION_FAILED,
  MEASUREMENT_INIT_FAILED,
  FRC_FAILED,
  UNKNOWN
};
enum MeasurementMode { PERIODIC, LOW_POWER_PERIODIC, SINGLE_SHOT, SINGLE_SHOT_RHT_ONLY };

class SCD4XComponent : public PollingComponent, public sensirion_common::SensirionI2CDevice {
 public:
  float get_setup_priority() const override { return setup_priority::DATA; }
  void setup() override;
  void dump_config() override;
  void update() override;

  void set_automatic_self_calibration(bool asc) { enable_asc_ = asc; }
  void set_altitude_compensation(uint16_t altitude) { altitude_compensation_ = altitude; }
  void set_ambient_pressure_compensation(float pressure_in_hpa);
  void set_ambient_pressure_source(sensor::Sensor *pressure) { ambient_pressure_source_ = pressure; }
  void set_temperature_offset(float offset) { temperature_offset_ = offset; };

  void set_co2_sensor(sensor::Sensor *co2) { co2_sensor_ = co2; }
  void set_temperature_sensor(sensor::Sensor *temperature) { temperature_sensor_ = temperature; };
  void set_humidity_sensor(sensor::Sensor *humidity) { humidity_sensor_ = humidity; }
  void set_measurement_mode(MeasurementMode mode) { measurement_mode_ = mode; }
  bool perform_forced_calibration(uint16_t current_co2_concentration);
  bool factory_reset();

 protected:
  bool update_ambient_pressure_compensation_(uint16_t pressure_in_hpa);
  bool start_measurement_();
  ERRORCODE error_code_;

  bool initialized_{false};

  float temperature_offset_;
  uint16_t altitude_compensation_;
  bool ambient_pressure_compensation_;
  uint16_t ambient_pressure_;
  bool enable_asc_;
  MeasurementMode measurement_mode_{PERIODIC};
  sensor::Sensor *co2_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  // used for compensation
  sensor::Sensor *ambient_pressure_source_{nullptr};
};

}  // namespace scd4x
}  // namespace esphome
