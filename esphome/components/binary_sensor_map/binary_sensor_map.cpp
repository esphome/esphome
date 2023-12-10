#include "binary_sensor_map.h"
#include "esphome/core/log.h"

namespace esphome {
namespace binary_sensor_map {

static const char *const TAG = "binary_sensor_map";

void BinarySensorMap::dump_config() { LOG_SENSOR("  ", "binary_sensor_map", this); }

void BinarySensorMap::loop() {
  switch (this->sensor_type_) {
    case BINARY_SENSOR_MAP_TYPE_GROUP:
      this->process_group_();
      break;
    case BINARY_SENSOR_MAP_TYPE_SUM:
      this->process_sum_();
      break;
    case BINARY_SENSOR_MAP_TYPE_BAYESIAN:
      this->process_bayesian_();
      break;
  }
}

void BinarySensorMap::process_group_() {
  float total_current_value = 0.0;
  uint8_t num_active_sensors = 0;
  uint64_t mask = 0x00;

  // - check all binary_sensors for its state
  //  - if active, add its value to total_current_value.
  // - creates a bitmask for the binary_sensor states on all channels
  for (size_t i = 0; i < this->channels_.size(); i++) {
    auto bs = this->channels_[i];
    if (bs.binary_sensor->state) {
      num_active_sensors++;
      total_current_value += bs.parameters.sensor_value;
      mask |= 1ULL << i;
    }
  }

  // potentially update state only if a binary_sensor is active
  if (mask != 0ULL) {
    // publish the average if the bitmask has changed
    if (this->last_mask_ != mask) {
      float publish_value = total_current_value / num_active_sensors;
      this->publish_state(publish_value);
    }
  } else if (this->last_mask_ != 0ULL) {
    // no buttons are pressed and the states have changed since last run, so publish NAN
    ESP_LOGV(TAG, "'%s' - No binary sensor active, publishing NAN", this->name_.c_str());
    this->publish_state(NAN);
  }

  this->last_mask_ = mask;
}

void BinarySensorMap::process_sum_() {
  float total_current_value = 0.0;
  uint64_t mask = 0x00;

  // - check all binary_sensor states
  // - if active, add its value to total_current_value
  // - creates a bitmask for the binary_sensor states on all channels
  for (size_t i = 0; i < this->channels_.size(); i++) {
    auto bs = this->channels_[i];
    if (bs.binary_sensor->state) {
      total_current_value += bs.parameters.sensor_value;
      mask |= 1ULL << i;
    }
  }

  // update state only if any binary_sensor states have changed or if no state has ever been sent on boot
  if ((this->last_mask_ != mask) || (!this->has_state())) {
    this->publish_state(total_current_value);
  }

  this->last_mask_ = mask;
}

void BinarySensorMap::process_bayesian_() {
  float posterior_probability = this->bayesian_prior_;
  uint64_t mask = 0x00;

  // - compute the posterior probability by taking the product of the predicate probablities for each observation
  // - create a bitmask for the binary_sensor states on all channels/observations
  for (size_t i = 0; i < this->channels_.size(); i++) {
    auto bs = this->channels_[i];

    posterior_probability *=
        this->bayesian_predicate_(bs.binary_sensor->state, posterior_probability,
                                  bs.parameters.probabilities.given_true, bs.parameters.probabilities.given_false);

    mask |= ((uint64_t) (bs.binary_sensor->state)) << i;
  }

  // update state only if any binary_sensor states have changed or if no state has ever been sent on boot
  if ((this->last_mask_ != mask) || (!this->has_state())) {
    this->publish_state(posterior_probability);
  }

  this->last_mask_ = mask;
}

float BinarySensorMap::bayesian_predicate_(bool sensor_state, float prior, float prob_given_true,
                                           float prob_given_false) {
  float prob_state_source_true = prob_given_true;
  float prob_state_source_false = prob_given_false;

  // if sensor is off, then we use the probabilities for the observation's complement
  if (!sensor_state) {
    prob_state_source_true = 1 - prob_given_true;
    prob_state_source_false = 1 - prob_given_false;
  }

  return prob_state_source_true / (prior * prob_state_source_true + (1.0 - prior) * prob_state_source_false);
}

void BinarySensorMap::add_channel(binary_sensor::BinarySensor *sensor, float value) {
  BinarySensorMapChannel sensor_channel{
      .binary_sensor = sensor,
      .parameters{
          .sensor_value = value,
      },
  };
  this->channels_.push_back(sensor_channel);
}

void BinarySensorMap::add_channel(binary_sensor::BinarySensor *sensor, float prob_given_true, float prob_given_false) {
  BinarySensorMapChannel sensor_channel{
      .binary_sensor = sensor,
      .parameters{
          .probabilities{
              .given_true = prob_given_true,
              .given_false = prob_given_false,
          },
      },
  };
  this->channels_.push_back(sensor_channel);
}
}  // namespace binary_sensor_map
}  // namespace esphome
