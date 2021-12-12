#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/defines.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"

#ifdef USE_ESP32
#include "driver/adc.h"
#include <esp_adc_cal.h>
#endif

namespace esphome {
namespace adc {

class ADCSensor : public sensor::Sensor, public PollingComponent, public voltage_sampler::VoltageSampler {
 public:
#ifdef USE_ESP32
  /// Set the attenuation for this pin. Only available on the ESP32.
  void set_attenuation(adc_atten_t attenuation) { attenuation_ = attenuation; }
  void set_channel(adc1_channel_t channel) { channel_ = channel; }
  void set_autorange(bool autorange) { autorange_ = autorange; }
#endif

  /// Update adc values.
  void update() override;
  /// Setup ADc
  void setup() override;
  void dump_config() override;
  /// `HARDWARE_LATE` setup priority.
  float get_setup_priority() const override;
  void set_pin(InternalGPIOPin *pin) { this->pin_ = pin; }
  void set_output_raw(bool output_raw) { output_raw_ = output_raw; }
  float sample() override;

#ifdef USE_ESP8266
  std::string unique_id() override;
#endif

 protected:
  InternalGPIOPin *pin_;
  bool output_raw_{false};

#ifdef USE_ESP32
  adc_atten_t attenuation_{ADC_ATTEN_DB_0};
  adc1_channel_t channel_{};
  bool autorange_{false};
  esp_adc_cal_characteristics_t cal_characteristics_[(int) ADC_ATTEN_MAX] = {};
#endif
};

}  // namespace adc
}  // namespace esphome
