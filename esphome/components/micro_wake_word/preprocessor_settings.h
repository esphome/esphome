#pragma once

#ifdef USE_ESP_IDF

#include <cstdint>

namespace esphome {
namespace micro_wake_word {

// The number of features the audio preprocessor generates per slice
static const uint8_t PREPROCESSOR_FEATURE_SIZE = 40;
// Duration of each slice used as input into the preprocessor
static const uint8_t FEATURE_DURATION_MS = 30;
// Audio sample frequency in hertz
static const uint16_t AUDIO_SAMPLE_FREQUENCY = 16000;

}  // namespace micro_wake_word
}  // namespace esphome

#endif
