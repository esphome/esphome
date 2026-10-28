#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <cstddef>
#include <cstdint>

namespace esphome::tflite_memory_handler {

/// @brief Probes the actual required tensor arena size by trial allocation.
/// Tries the requested size first, then 1.5x, then 2x if it fails. The size that
/// works is probed down to what the model reported as used; only if that smaller
/// arena fails is a binary search run between the two.
/// Uses MemoryManager::allocate_tensor_arena() (PSRAM-aware) and builds a local
/// op resolver from the model's operators. Returns 0 when the model cannot run or
/// the memory for the arena cannot be allocated; a zero @p initial_size is
/// rejected the same way.
[[nodiscard]] size_t probe_arena_size(const uint8_t *model_start, size_t initial_size);

}  // namespace esphome::tflite_memory_handler

#endif  // USE_TFLITE_MICRO_HELPER
