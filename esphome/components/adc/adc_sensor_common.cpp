#include "adc_sensor.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::adc {

ESPHOME_LOG_TAG(TAG, "adc");

// Indexed by SamplingMode
PROGMEM_STRING_TABLE(SamplingModeStrings, "average", "minimum", "maximum", "unknown");

const LogString *sampling_mode_to_str(SamplingMode mode) {
  return SamplingModeStrings::get_log_str(static_cast<uint8_t>(mode), SamplingModeStrings::LAST_INDEX);
}

template<typename T> Aggregator<T>::Aggregator(SamplingMode mode) {
  this->mode_ = mode;
  // set to max uint if mode is "min"
  if (mode == SamplingMode::MIN) {
    this->aggr_ = std::numeric_limits<T>::max();
  }
}

template<typename T> void Aggregator<T>::add_sample(T value) {
  this->samples_ += 1;

  switch (this->mode_) {
    case SamplingMode::AVG:
      this->aggr_ += value;
      break;

    case SamplingMode::MIN:
      if (value < this->aggr_) {
        this->aggr_ = value;
      }
      break;

    case SamplingMode::MAX:
      if (value > this->aggr_) {
        this->aggr_ = value;
      }
  }
}

template<typename T> T Aggregator<T>::aggregate() {
  if (this->mode_ == SamplingMode::AVG) {
    if (this->samples_ == 0) {
      return this->aggr_;
    }

    return (this->aggr_ + (this->samples_ >> 1)) / this->samples_;  // NOLINT(clang-analyzer-core.DivideZero)
  }

  return this->aggr_;
}

#ifdef USE_ZEPHYR
template class Aggregator<int32_t>;
#else
template class Aggregator<uint32_t>;
#endif

void ADCSensor::update() {
  float value_v = this->sample();
  ESP_LOGV(TAG, "'%s': Voltage=%.4fV", LOG_STR_ARG(this->get_log_name()), value_v);
  this->publish_state(value_v);
}

void ADCSensor::set_sample_count(uint8_t sample_count) {
  if (sample_count != 0) {
    this->sample_count_ = sample_count;
  }
}

}  // namespace esphome::adc
