#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome::bh1745 {

enum class MeasurementTime : uint8_t {
  MEASUREMENT_TIME_160MS = 0b000,
  MEASUREMENT_TIME_320MS = 0b001,
  MEASUREMENT_TIME_640MS = 0b010,
  MEASUREMENT_TIME_1280MS = 0b011,
  MEASUREMENT_TIME_2560MS = 0b100,
  MEASUREMENT_TIME_5120MS = 0b101,
};

enum class AdcGain : uint8_t {
  ADC_GAIN_1X = 0,
  ADC_GAIN_2X = 1,
  ADC_GAIN_16X = 2,
};

class BH1745Component : public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(red_counts)
  SUB_SENSOR(green_counts)
  SUB_SENSOR(blue_counts)
  SUB_SENSOR(clear_counts)
  SUB_SENSOR(illuminance)
  SUB_SENSOR(color_temperature)

 public:
  void setup() override;
  void dump_config() override;
  void update() override;

  void set_measurement_time(MeasurementTime measurement_time) { this->measurement_time_ = measurement_time; }
  void set_adc_gain(AdcGain adc_gain) { this->adc_gain_ = adc_gain; }
  void set_glass_attenuation_factor(float factor) { this->glass_attenuation_factor_ = factor; }

  /// Called when an output platform uses the INT pin, so setup() pins the interrupt thresholds.
  void enable_interrupt_output() { this->interrupt_output_ = true; }
  void set_interrupt_state(bool on);

 protected:
  float glass_attenuation_factor_{1.0f};
  float lux_scale_{1.0f};  // 160 / (gain * integration time * glass attenuation), set in setup()
  MeasurementTime measurement_time_{MeasurementTime::MEASUREMENT_TIME_160MS};
  AdcGain adc_gain_{AdcGain::ADC_GAIN_1X};
  bool busy_{true};  // cleared once the chip is configured after the reset
  bool interrupt_output_{false};
};

}  // namespace esphome::bh1745
