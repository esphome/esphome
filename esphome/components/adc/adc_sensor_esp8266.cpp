#ifdef USE_ESP8266

#include "adc_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ADC_SENSOR_VCC
#include <Esp.h>
ADC_MODE(ADC_VCC)
#else
#include <Arduino.h>
#endif  // USE_ADC_SENSOR_VCC

namespace esphome {
namespace adc {

static const char *const TAG = "adc.esp8266";

void ADCSensor::setup() {
#ifndef USE_ADC_SENSOR_VCC
  this->pin_->setup();
#endif
}

void ADCSensor::dump_config() {
  LOG_SENSOR("", "ADC Sensor", this);
#ifdef USE_ADC_SENSOR_VCC
  ESP_LOGCONFIG(TAG, "  Pin: VCC");
#else
  LOG_PIN("  Pin: ", this->pin_);
#endif  // USE_ADC_SENSOR_VCC
  ESP_LOGCONFIG(TAG,
                "  Samples: %i\n"
                "  Sampling mode: %s",
                this->sample_count_, LOG_STR_ARG(sampling_mode_to_str(this->sampling_mode_)));
  LOG_UPDATE_INTERVAL(this);
}

float ADCSensor::sample() {
  auto aggr = Aggregator<uint32_t>(this->sampling_mode_);

  for (uint8_t sample = 0; sample < this->sample_count_; sample++) {
    uint32_t raw = 0;
#ifdef USE_ADC_SENSOR_VCC
    raw = ESP.getVcc();  // NOLINT(readability-static-accessed-through-instance)
#else
    raw = analogRead(this->pin_->get_pin());  // NOLINT
#endif  // USE_ADC_SENSOR_VCC
    aggr.add_sample(raw);
  }

  if (this->output_raw_) {
    return aggr.aggregate();
  }
  return aggr.aggregate() / 1024.0f;
}

}  // namespace adc
}  // namespace esphome

#endif  // USE_ESP8266
