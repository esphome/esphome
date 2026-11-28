#include "tflite_audio.h"

#ifdef USE_TFLITE_MICRO_HELPER

namespace esphome::tflite_audio {

void TFLiteAudio::set_sliding_window_size(size_t n) { this->audio_config_.sliding_window_size = n; }

void TFLiteAudio::set_features_step_size(uint8_t ms) { this->audio_config_.features_step_size = ms; }

void TFLiteAudio::set_feature_count(size_t n) { this->audio_config_.feature_count = n; }

}  // namespace esphome::tflite_audio

#endif  // USE_TFLITE_MICRO_HELPER
