#include "audio_analyzer_sensor.h"

#include "esphome/core/log.h"

#include <cmath>

namespace esphome::audio_analyzer {

ESPHOME_LOG_TAG(TAG, "audio_analyzer.sensor");

void AudioAnalyzerSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Audio Analyzer Sensor:");
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Power", this->power_sensor_);
  for (uint8_t b = 0; b < NUM_BANDS; ++b) {
    if (this->band_sensors_[b] != nullptr) {
      ESP_LOGCONFIG(TAG, "  Band %u:", b);
      LOG_SENSOR("    ", "Band", this->band_sensors_[b]);
    }
  }
}

void AudioAnalyzerSensor::setup() {
  this->band_db_hold_.fill(NAN);
  this->parent_->add_on_frame_callback([this]() {
    // fmaxf ignores the NAN of an empty hold
    this->power_db_hold_ = fmaxf(this->power_db_hold_, this->parent_->get_power_db());
    for (uint8_t b = 0; b < NUM_BANDS; ++b) {
      this->band_db_hold_[b] = fmaxf(this->band_db_hold_[b], this->parent_->get_band_db(b));
    }
  });
}

void AudioAnalyzerSensor::update() {
  if (std::isnan(this->power_db_hold_)) {
    // Nothing analyzed since the last update: unknown, as sound_level reports a stopped microphone
    if (!this->published_unknown_) {
      this->published_unknown_ = true;
      this->publish_(NAN, this->band_db_hold_.data());
    }
    return;
  }
  this->published_unknown_ = false;
  this->publish_(this->power_db_hold_, this->band_db_hold_.data());
  this->power_db_hold_ = NAN;
  this->band_db_hold_.fill(NAN);
}

void AudioAnalyzerSensor::publish_(float power_db, const float *band_db) {
  PUBLISH_STATE(this->power_sensor_, power_db);
  for (uint8_t b = 0; b < NUM_BANDS; ++b) {
    PUBLISH_STATE(this->band_sensors_[b], band_db[b]);
  }
}

}  // namespace esphome::audio_analyzer
