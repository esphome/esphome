#pragma once

#ifdef USE_ESP_IDF

#include <cstdint>

namespace esphome {
namespace micro_wake_word {

// The number of features the audio preprocessor generates per slice
static const uint8_t PREPROCESSOR_FEATURE_SIZE = 40;
// How frequently the preprocessor generates a new set of features
static const uint8_t FEATURE_STRIDE_MS = 20;
// Duration of each slice used as input into the preprocessor
static const uint8_t FEATURE_DURATION_MS = 30;
// Audio sample frequency in hertz
static const uint16_t AUDIO_SAMPLE_FREQUENCY = 16000;
// The number of old audio samples that are saved to be part of the next feature window
static const uint16_t HISTORY_SAMPLES_TO_KEEP =
    ((FEATURE_DURATION_MS - FEATURE_STRIDE_MS) * (AUDIO_SAMPLE_FREQUENCY / 1000));
// The number of new audio samples to receive to be included with the next feature window
static const uint16_t NEW_SAMPLES_TO_GET = (FEATURE_STRIDE_MS * (AUDIO_SAMPLE_FREQUENCY / 1000));
// The total number of audio samples included in the feature window
static const uint16_t SAMPLE_DURATION_COUNT = FEATURE_DURATION_MS * AUDIO_SAMPLE_FREQUENCY / 1000;
// Number of bytes in memory needed for the preprocessor arena
static const uint32_t PREPROCESSOR_ARENA_SIZE = 9528;

}  // namespace micro_wake_word
}  // namespace esphome

#endif
