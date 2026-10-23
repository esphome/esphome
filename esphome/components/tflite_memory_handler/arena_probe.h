#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <cstddef>
#include <cstdint>

namespace esphome::tflite_memory_handler {

/// @brief Probes the actual required tensor arena size by trial allocation.
/// Tries the requested size first, then 1.5x, then 2x if it fails.
/// On success, binary-searches down to the minimum and returns it (16-byte aligned + padding).
/// Uses MemoryManager::allocate_tensor_arena() (PSRAM-aware) and builds a local
/// op resolver from the model's operators. Returns 0 on total failure.
[[nodiscard]] size_t probe_arena_size(const uint8_t *model_start, size_t initial_size);

}  // namespace esphome::tflite_memory_handler

#endif  // USE_TFLITE_MICRO_HELPER
