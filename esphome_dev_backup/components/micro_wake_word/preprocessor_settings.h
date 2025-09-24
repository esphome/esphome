#pragma once

#ifdef USE_ESP_IDF

#include <cstdint>

namespace esphome {
namespace micro_wake_word {

// Settings for controlling the spectrogram feature generation by the preprocessor.
// These must match the settings used when training a particular model.
// All microWakeWord models have been trained with these specific paramters.

// The number of features the audio preprocessor generates per slice
static const uint8_t PREPROCESSOR_FEATURE_SIZE = 40;
// Duration of each slice used as input into the preprocessor
static const uint8_t FEATURE_DURATION_MS = 30;

static const float FILTERBANK_LOWER_BAND_LIMIT = 125.0;
static const float FILTERBANK_UPPER_BAND_LIMIT = 7500.0;

static const uint8_t NOISE_REDUCTION_SMOOTHING_BITS = 10;
static const float NOISE_REDUCTION_EVEN_SMOOTHING = 0.025;
static const float NOISE_REDUCTION_ODD_SMOOTHING = 0.06;
static const float NOISE_REDUCTION_MIN_SIGNAL_REMAINING = 0.05;

static const bool PCAN_GAIN_CONTROL_ENABLE_PCAN = true;
static const float PCAN_GAIN_CONTROL_STRENGTH = 0.95;
static const float PCAN_GAIN_CONTROL_OFFSET = 80.0;
static const uint8_t PCAN_GAIN_CONTROL_GAIN_BITS = 21;

static const bool LOG_SCALE_ENABLE_LOG = true;
static const uint8_t LOG_SCALE_SCALE_SHIFT = 6;
}  // namespace micro_wake_word
}  // namespace esphome

#endif
