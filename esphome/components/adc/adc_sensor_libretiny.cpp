#ifdef USE_LIBRETINY

#include "adc_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace adc {

static const char *const TAG = "adc.libretiny";

void ADCSensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ADC '%s'...", this->get_name().c_str());
#ifndef USE_ADC_SENSOR_VCC
  this->pin_->setup();
#endif  // !USE_ADC_SENSOR_VCC
}

void ADCSensor::dump_config() {
  LOG_SENSOR("", "ADC Sensor", this);
#ifdef USE_ADC_SENSOR_VCC
  ESP_LOGCONFIG(TAG, "  Pin: VCC");
#else   // USE_ADC_SENSOR_VCC
  LOG_PIN("  Pin: ", this->pin_);
#endif  // USE_ADC_SENSOR_VCC
  ESP_LOGCONFIG(TAG, "  Samples: %i", this->sample_count_);
  ESP_LOGCONFIG(TAG, "  Sampling mode: %s", LOG_STR_ARG(sampling_mode_to_str(this->sampling_mode_)));
  LOG_UPDATE_INTERVAL(this);
}

float ADCSensor::sample() {
  uint32_t raw = 0;
  auto aggr = Aggregator(this->sampling_mode_);

  if (this->output_raw_) {
    for (uint8_t sample = 0; sample < this->sample_count_; sample++) {
      raw = analogRead(this->pin_->get_pin());  // NOLINT
      aggr.add_sample(raw);
    }
    return aggr.aggregate();
  }

  for (uint8_t sample = 0; sample < this->sample_count_; sample++) {
    raw = analogReadVoltage(this->pin_->get_pin());  // NOLINT
    aggr.add_sample(raw);
  }

  return aggr.aggregate() / 1000.0f;
}

}  // namespace adc
}  // namespace esphome

#endif  // USE_LIBRETINY
