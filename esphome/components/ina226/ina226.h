#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace ina226 {

enum AdcTime : uint16_t {
  ADC_TIME_140US = 0,
  ADC_TIME_204US = 1,
  ADC_TIME_332US = 2,
  ADC_TIME_588US = 3,
  ADC_TIME_1100US = 4,
  ADC_TIME_2116US = 5,
  ADC_TIME_4156US = 6,
  ADC_TIME_8244US = 7
};

enum AdcAvgSamples : uint16_t {
  ADC_AVG_SAMPLES_1 = 0,
  ADC_AVG_SAMPLES_4 = 1,
  ADC_AVG_SAMPLES_16 = 2,
  ADC_AVG_SAMPLES_64 = 3,
  ADC_AVG_SAMPLES_128 = 4,
  ADC_AVG_SAMPLES_256 = 5,
  ADC_AVG_SAMPLES_512 = 6,
  ADC_AVG_SAMPLES_1024 = 7
};

union ConfigurationRegister {
  uint16_t raw;
  struct {
    uint16_t mode : 3;
    AdcTime shunt_voltage_conversion_time : 3;
    AdcTime bus_voltage_conversion_time : 3;
    AdcAvgSamples avg_samples : 3;
    uint16_t reserved : 3;
    uint16_t reset : 1;
  } __attribute__((packed));
};

class INA226Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

  void set_shunt_resistance_ohm(float shunt_resistance_ohm) { shunt_resistance_ohm_ = shunt_resistance_ohm; }
  void set_max_current_a(float max_current_a) { max_current_a_ = max_current_a; }
  void set_adc_time_voltage(AdcTime time) { adc_time_voltage_ = time; }
  void set_adc_time_current(AdcTime time) { adc_time_current_ = time; }
  void set_adc_avg_samples(AdcAvgSamples samples) { adc_avg_samples_ = samples; }

  void set_bus_voltage_sensor(sensor::Sensor *bus_voltage_sensor) { bus_voltage_sensor_ = bus_voltage_sensor; }
  void set_shunt_voltage_sensor(sensor::Sensor *shunt_voltage_sensor) { shunt_voltage_sensor_ = shunt_voltage_sensor; }
  void set_current_sensor(sensor::Sensor *current_sensor) { current_sensor_ = current_sensor; }
  void set_power_sensor(sensor::Sensor *power_sensor) { power_sensor_ = power_sensor; }

 protected:
  float shunt_resistance_ohm_;
  float max_current_a_;
  AdcTime adc_time_voltage_{AdcTime::ADC_TIME_1100US};
  AdcTime adc_time_current_{AdcTime::ADC_TIME_1100US};
  AdcAvgSamples adc_avg_samples_{AdcAvgSamples::ADC_AVG_SAMPLES_4};
  uint32_t calibration_lsb_;
  sensor::Sensor *bus_voltage_sensor_{nullptr};
  sensor::Sensor *shunt_voltage_sensor_{nullptr};
  sensor::Sensor *current_sensor_{nullptr};
  sensor::Sensor *power_sensor_{nullptr};

  int32_t twos_complement_(int32_t val, uint8_t bits);
};

}  // namespace ina226
}  // namespace esphome
