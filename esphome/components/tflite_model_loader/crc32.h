#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <cstddef>
#include <cstdint>

namespace esphome::tflite_model_loader {

/// @brief Compute the CRC32 (IEEE 802.3, polynomial 0xEDB88320) of a buffer.
uint32_t calculate_crc32(const uint8_t *data, size_t length);

/// @brief Verify that the CRC32 of model data matches the expected value.
/// An expected_crc of 0 skips verification and returns true.
bool verify_model_crc(const uint8_t *model_data, size_t length, uint32_t expected_crc = 0);

}  // namespace esphome::tflite_model_loader

#endif  // USE_TFLITE_MICRO_HELPER
