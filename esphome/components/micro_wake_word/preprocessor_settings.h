#pragma once

#ifdef USE_ESP_IDF

#include <cstdint>

namespace esphome {
namespace micro_wake_word {

// The number of features the audio preprocessor generates per slice
static const uint8_t PREPROCESSOR_FEATURE_SIZE = 40;

// How frequently the preprocessor generates a new set of features
#ifdef MWW_SLIDE_20MS
static const uint8_t FEATURE_STRIDE_MS = 20;
#else
static const uint8_t FEATURE_STRIDE_MS = 10;
#endif

// Duration of each slice used as input into the preprocessor
static const uint8_t FEATURE_DURATION_MS = 30;
// Audio sample frequency in hertz
static const uint16_t AUDIO_SAMPLE_FREQUENCY = 16000;
// The number of new audio samples to receive to be included with the next feature window
static const uint16_t NEW_SAMPLES_TO_GET = (FEATURE_STRIDE_MS * (AUDIO_SAMPLE_FREQUENCY / 1000));

}  // namespace micro_wake_word
}  // namespace esphome

#endif
