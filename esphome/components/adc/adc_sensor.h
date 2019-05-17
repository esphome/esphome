#pragma once

#include "esphome/core/component.h"
#include "esphome/core/esphal.h"
#include "esphome/core/defines.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"

namespace esphome {
namespace adc {

class ADCSensor : public sensor::Sensor, public PollingComponent, public voltage_sampler::VoltageSampler {
 public:
#ifdef ARDUINO_ARCH_ESP32
  /// Set the attenuation for this pin. Only available on the ESP32.
  void set_attenuation(adc_attenuation_t attenuation);
#endif

  /// Update adc values.
  void update() override;
  /// Setup ADc
  void setup() override;
  void dump_config() override;
  /// `HARDWARE_LATE` setup priority.
  float get_setup_priority() const override;
  void set_pin(uint8_t pin) { this->pin_ = pin; }
  float sample() override;

#ifdef ARDUINO_ARCH_ESP8266
  std::string unique_id() override;
#endif

 protected:
  uint8_t pin_;

#ifdef ARDUINO_ARCH_ESP32
  adc_attenuation_t attenuation_{ADC_0db};
#endif
};

}  // namespace adc
}  // namespace esphome
