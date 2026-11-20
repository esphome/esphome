#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::ina226 {

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

class INA226Component final : public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(bus_voltage)
  SUB_SENSOR(shunt_voltage)
  SUB_SENSOR(current)
  SUB_SENSOR(power)

 public:
  void setup() override;
  void dump_config() override;
  void update() override;

  void set_shunt_resistance_ohm(float shunt_resistance_ohm) { shunt_resistance_ohm_ = shunt_resistance_ohm; }
  void set_max_current_a(float max_current_a) { max_current_a_ = max_current_a; }
  void set_adc_time_voltage(AdcTime time) { adc_time_voltage_ = time; }
  void set_adc_time_current(AdcTime time) { adc_time_current_ = time; }
  void set_adc_avg_samples(AdcAvgSamples samples) { adc_avg_samples_ = samples; }

#ifdef USE_INA226_ALERT
  // Register values computed in Python
  void set_alert(uint16_t limit, uint16_t mask) {
    this->alert_limit_ = limit;
    this->alert_mask_ = mask;
  }
  void clear_alert_flag();
#endif

 protected:
  float shunt_resistance_ohm_;
  float max_current_a_;
  AdcTime adc_time_voltage_{AdcTime::ADC_TIME_1100US};
  AdcTime adc_time_current_{AdcTime::ADC_TIME_1100US};
  AdcAvgSamples adc_avg_samples_{AdcAvgSamples::ADC_AVG_SAMPLES_4};
  uint32_t calibration_lsb_;
#ifdef USE_INA226_ALERT
  uint16_t alert_limit_{0};
  uint16_t alert_mask_{0};
#endif
};

}  // namespace esphome::ina226
