#include "tflite_audio.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <algorithm>
#include <cmath>

#include "esphome/core/log.h"

namespace esphome::tflite_audio {

ESPHOME_LOG_TAG(TAG, "tflite_audio");

void TFLiteAudio::set_probability_cutoff(float f) {
  if (std::isnan(f)) {
    ESP_LOGW(TAG, "probability_cutoff is NaN -- using 0.5");
    f = 0.5f;
  } else if (f < 0.0f || f > 1.0f) {
    ESP_LOGW(TAG, "probability_cutoff is outside 0-100%% -- clamping");
    f = std::clamp(f, 0.0f, 1.0f);
  }
  this->audio_config_.probability_cutoff = static_cast<uint8_t>(f * 255.0f);
}

void TFLiteAudio::set_sliding_window_size(size_t n) { this->audio_config_.sliding_window_size = n; }

void TFLiteAudio::set_features_step_size(uint8_t ms) { this->audio_config_.features_step_size = ms; }

void TFLiteAudio::set_feature_count(size_t n) { this->audio_config_.feature_count = n; }

}  // namespace esphome::tflite_audio

#endif  // USE_TFLITE_MICRO_HELPER
