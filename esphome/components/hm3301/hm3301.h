#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "aqi_calculator_factory.h"

namespace esphome {
namespace hm3301 {

static const uint8_t SELECT_COMM_CMD = 0X88;

class HM3301Component : public PollingComponent, public i2c::I2CDevice {
 public:
  HM3301Component() = default;

  void set_pm_1_0_sensor(sensor::Sensor *pm_1_0_sensor) { pm_1_0_sensor_ = pm_1_0_sensor; }
  void set_pm_2_5_sensor(sensor::Sensor *pm_2_5_sensor) { pm_2_5_sensor_ = pm_2_5_sensor; }
  void set_pm_10_0_sensor(sensor::Sensor *pm_10_0_sensor) { pm_10_0_sensor_ = pm_10_0_sensor; }
  void set_aqi_sensor(sensor::Sensor *aqi_sensor) { aqi_sensor_ = aqi_sensor; }

  void set_aqi_calculation_type(AQICalculatorType aqi_calc_type) { aqi_calc_type_ = aqi_calc_type; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

 protected:
  enum {
    NO_ERROR = 0,
    ERROR_PARAM = -1,
    ERROR_COMM = -2,
    ERROR_OTHERS = -128,
  } error_code_{NO_ERROR};

  uint8_t data_buffer_[30];

  sensor::Sensor *pm_1_0_sensor_{nullptr};
  sensor::Sensor *pm_2_5_sensor_{nullptr};
  sensor::Sensor *pm_10_0_sensor_{nullptr};
  sensor::Sensor *aqi_sensor_{nullptr};

  AQICalculatorType aqi_calc_type_;
  AQICalculatorFactory aqi_calculator_factory_ = AQICalculatorFactory();

  bool validate_checksum_(const uint8_t *);
  uint16_t get_sensor_value_(const uint8_t *, uint8_t);
};

}  // namespace hm3301
}  // namespace esphome
