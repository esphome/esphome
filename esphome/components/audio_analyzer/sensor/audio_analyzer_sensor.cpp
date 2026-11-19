#include "audio_analyzer_sensor.h"

#include "esphome/core/log.h"

#include <cmath>

namespace esphome::audio_analyzer {

ESPHOME_LOG_TAG(TAG, "audio_analyzer.sensor");

void AudioAnalyzerSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Audio Analyzer Sensor:");
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "RMS", this->rms_sensor_);
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
    this->rms_db_hold_ = fmaxf(this->rms_db_hold_, this->parent_->get_rms_db());
    for (uint8_t b = 0; b < NUM_BANDS; ++b) {
      this->band_db_hold_[b] = fmaxf(this->band_db_hold_[b], this->parent_->get_band_db(b));
    }
  });
}

void AudioAnalyzerSensor::update() {
  if (std::isnan(this->rms_db_hold_)) {
    // No frame since the last update. While the analyzer is active the next frame is just not there yet; once it
    // is idle, unknown, as sound_level reports a stopped microphone.
    if (!this->parent_->is_active() && !this->published_unknown_) {
      this->published_unknown_ = true;
      this->publish_(NAN, this->band_db_hold_.data());
    }
    return;
  }
  this->published_unknown_ = false;
  this->publish_(this->rms_db_hold_, this->band_db_hold_.data());
  this->rms_db_hold_ = NAN;
  this->band_db_hold_.fill(NAN);
}

void AudioAnalyzerSensor::publish_(float rms_db, const float *band_db) {
  PUBLISH_STATE(this->rms_sensor_, rms_db);
  for (uint8_t b = 0; b < NUM_BANDS; ++b) {
    PUBLISH_STATE(this->band_sensors_[b], band_db[b]);
  }
}

}  // namespace esphome::audio_analyzer
