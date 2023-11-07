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
  void set_channel1(adc1_channel_t channel) {
    channel1_ = channel;
    channel2_ = ADC2_CHANNEL_MAX;
  }
  void set_channel2(adc2_channel_t channel) {
    channel2_ = channel;
    channel1_ = ADC1_CHANNEL_MAX;
  }
  void set_autorange(bool autorange) { autorange_ = autorange; }
#endif

  /// Update ADC values
  void update() override;
  /// Setup ADC
  void setup() override;
  void dump_config() override;
  /// `HARDWARE_LATE` setup priority
  float get_setup_priority() const override;
  void set_pin(InternalGPIOPin *pin) { this->pin_ = pin; }
  void set_output_raw(bool output_raw) { output_raw_ = output_raw; }
  float sample() override;

#ifdef USE_ESP8266
  std::string unique_id() override;
#endif

#ifdef USE_RP2040
  void set_is_temperature() { is_temperature_ = true; }
#endif

 protected:
  InternalGPIOPin *pin_;
  bool output_raw_{false};

#ifdef USE_RP2040
  bool is_temperature_{false};
#endif

#ifdef USE_ESP32
  adc_atten_t attenuation_{ADC_ATTEN_DB_0};
  adc1_channel_t channel1_{ADC1_CHANNEL_MAX};
  adc2_channel_t channel2_{ADC2_CHANNEL_MAX};
  bool autorange_{false};
#if ESP_IDF_VERSION_MAJOR >= 5
  esp_adc_cal_characteristics_t cal_characteristics_[SOC_ADC_ATTEN_NUM] = {};
#else
  esp_adc_cal_characteristics_t cal_characteristics_[ADC_ATTEN_MAX] = {};
#endif
#endif
};

}  // namespace adc
}  // namespace esphome
