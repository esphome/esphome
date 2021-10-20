#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/defines.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"

#ifdef USE_ESP32
#include "driver/adc.h"
#endif

namespace esphome {
namespace adc {

class ADCSensor : public sensor::Sensor, public PollingComponent, public voltage_sampler::VoltageSampler {
 public:
#ifdef USE_ESP32
  /// Set the attenuation for this pin. Only available on the ESP32.
  void set_attenuation(adc_atten_t attenuation);
  void set_autorange(bool autorange);
#endif

  /// Update adc values.
  void update() override;
  /// Setup ADc
  void setup() override;
  void dump_config() override;
  /// `HARDWARE_LATE` setup priority.
  float get_setup_priority() const override;
  void set_pin(InternalGPIOPin *pin) { this->pin_ = pin; }
  float sample() override;

#ifdef USE_ESP8266
  std::string unique_id() override;
#endif

 protected:
  InternalGPIOPin *pin_;
  uint16_t read_raw_();
  uint32_t raw_to_microvolts_(uint16_t raw);

#ifdef USE_ESP32
  adc_atten_t attenuation_{ADC_ATTEN_DB_0};
  bool autorange_{false};
#endif
};

}  // namespace adc
}  // namespace esphome
