#include "adc_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace adc {

static const char *const TAG = "adc.common";

const LogString *sampling_mode_to_str(SamplingMode mode) {
  switch (mode) {
    case SamplingMode::AVG:
      return LOG_STR("average");
    case SamplingMode::MIN:
      return LOG_STR("minimum");
    case SamplingMode::MAX:
      return LOG_STR("maximum");
  }
  return LOG_STR("unknown");
}

Aggregator::Aggregator(SamplingMode mode) {
  this->mode_ = mode;
  // set to max uint if mode is "min"
  if (mode == SamplingMode::MIN) {
    this->aggr_ = UINT32_MAX;
  }
}

void Aggregator::add_sample(uint32_t value) {
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

uint32_t Aggregator::aggregate() {
  if (this->mode_ == SamplingMode::AVG) {
    if (this->samples_ == 0) {
      return this->aggr_;
    }

    return (this->aggr_ + (this->samples_ >> 1)) / this->samples_;  // NOLINT(clang-analyzer-core.DivideZero)
  }

  return this->aggr_;
}

void ADCSensor::update() {
  float value_v = this->sample();
  ESP_LOGV(TAG, "'%s': Got voltage=%.4fV", this->get_name().c_str(), value_v);
  this->publish_state(value_v);
}

void ADCSensor::set_sample_count(uint8_t sample_count) {
  if (sample_count != 0) {
    this->sample_count_ = sample_count;
  }
}

void ADCSensor::set_sampling_mode(SamplingMode sampling_mode) { this->sampling_mode_ = sampling_mode; }

float ADCSensor::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace adc
}  // namespace esphome
