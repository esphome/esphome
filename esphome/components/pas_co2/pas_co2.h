#pragma once
#include <vector>
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace pas_co2 {

// Infineon XENSIV PAS CO2 sensor - basic functionality
// https://www.infineon.com/cms/en/product/sensor/co2-sensors/pasco2v01/
class PASCO2Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;

  void set_ambient_pressure_compensation(float pressure_in_bar);
  void set_ambient_pressure_source(sensor::Sensor *pressure) { ambient_pressure_source_ = pressure; }

  void set_co2_sensor(sensor::Sensor *co2) { co2_sensor_ = co2; }

 protected:
  enum ErrorCode {
    NO_ERROR,
    COMMUNICATION_FAILED,
    SOFT_RESET_FAILED,
    MEASUREMENT_INIT_FAILED,
    UNKNOWN
  };

  enum SensorMode {
    SENSOR_MODE_IDLE = 0,
    SENSOR_MODE_SINGLE = 1,
    SENSOR_MODE_CONTINUOUS = 2
  };

  void init_();
  bool test_scratch_register_();
  bool check_sensor_status_();
  bool clear_status_();
  bool set_mode_(SensorMode mode);
  bool set_rate_(uint16_t);
  bool read_measurement_();
  void try_read_measurement_();
  bool update_ambient_pressure_compensation_(uint16_t pressure_in_hpa);

  ErrorCode error_code_{NO_ERROR};

  bool initialized_{false};
  int retry_count_{0};

  bool ambient_pressure_compensation_;
  uint16_t ambient_pressure_;
  sensor::Sensor *co2_sensor_{nullptr};
  // used for compensation
  sensor::Sensor *ambient_pressure_source_{nullptr};
};

}  // namespace pas_co2
}  // namespace esphome
