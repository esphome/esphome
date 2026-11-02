#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/aqi/aqi_calculator_factory.h"

namespace esphome::hm3301 {

static const uint8_t SELECT_COMM_CMD = 0x88;

class HM3301Component final : public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(pm_1_0)
  SUB_SENSOR(pm_2_5)
  SUB_SENSOR(pm_10_0)
  SUB_SENSOR(aqi)

 public:
  HM3301Component() = default;

  void set_aqi_calculation_type(aqi::AQICalculatorType aqi_calc_type) { aqi_calc_type_ = aqi_calc_type; }

  void setup() override;
  void dump_config() override;
  void update() override;

 protected:
  enum {
    NO_ERROR = 0,
    ERROR_PARAM = -1,
    ERROR_COMM = -2,
    ERROR_OTHERS = -128,
  } error_code_{NO_ERROR};

  uint8_t data_buffer_[30];

  aqi::AQICalculatorType aqi_calc_type_;
  aqi::AQICalculatorFactory aqi_calculator_factory_ = aqi::AQICalculatorFactory();

  bool validate_checksum_(const uint8_t *data);
  uint16_t get_sensor_value_(const uint8_t *data, uint8_t i);
};

}  // namespace esphome::hm3301
