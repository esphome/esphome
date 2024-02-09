#pragma once
#include <vector>
#include "esphome/core/application.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace pasco2 {

enum ERRORCODE {
  COMM_FAILED,
  SOFT_RESET_FAILED,
  XENSIV_PASCO2_ICCERR,
  XENSIV_PASCO2_ORVS,
  XENSIV_PASCO2_ORTMP,
  XENSIV_PASCO2_ERR_NOT_READY,
  MEASUREMENT_INIT_FAILED,
  FRC_FAILED,
  UNKNOWN
};
enum MeasurementMode { PERIODIC, SINGLE_SHOT };

class PASCO2Component : public PollingComponent, public i2c::I2CDevice {
 public:
  float get_setup_priority() const override { return setup_priority::DATA; }
  void setup() override;
  void dump_config() override;
  void update() override;

  void set_automatic_self_calibration(bool asc) { enable_asc_ = asc; }
  void set_ambient_pressure_compensation(float pressure_in_hpa);
  void set_ambient_pressure_source(sensor::Sensor *pressure) { ambient_pressure_source_ = pressure; }

  void set_co2_sensor(sensor::Sensor *co2) { co2_sensor_ = co2; }
  void set_measurement_mode(MeasurementMode mode) { measurement_mode_ = mode; }
  bool perform_forced_calibration(uint16_t current_co2_concentration);

  void set_enable_pin(GPIOPin *enable) { this->enable_pin_ = enable; }

 protected:
  bool update_ambient_pressure_compensation_(uint16_t pressure_in_hpa);
  bool start_measurement_();
  ERRORCODE error_code_;

  bool initialized_{false};

  bool ambient_pressure_compensation_;
  uint16_t ambient_pressure_;
  bool enable_asc_;
  MeasurementMode measurement_mode_{PERIODIC};
  sensor::Sensor *co2_sensor_{nullptr};
  GPIOPin *enable_pin_{nullptr};
  // used for compensation
  sensor::Sensor *ambient_pressure_source_{nullptr};
};

}  // namespace pasco2
}  // namespace esphome
