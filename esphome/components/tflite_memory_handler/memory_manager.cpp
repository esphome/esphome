#include "memory_manager.h"

#ifdef USE_TFLITE_MICRO_HELPER
#include "esphome/core/log.h"

#include "esp_heap_caps.h"

namespace esphome::tflite_memory_handler {

ESPHOME_LOG_TAG(TAG, "tflite_memory_handler");

// arena_location in __init__.py picks exactly one of these, so they cannot both
// be set by code generation. A raw -D build flag could still set both, and then
// neither the source nor the reported location would be trustworthy.
#if defined(TFLITE_ARENA_INTERNAL) && defined(TFLITE_ARENA_PSRAM)
#error "tflite_memory_handler: arena_location cannot be both internal and psram"
#endif

namespace {

/// Heap flags for the arena, from the validated arena_location option.
uint8_t arena_allocator_flags() {
#if defined(TFLITE_ARENA_INTERNAL)
  return RAMAllocator<uint8_t>::ALLOC_INTERNAL;
#elif defined(TFLITE_ARENA_PSRAM)
  return RAMAllocator<uint8_t>::ALLOC_EXTERNAL;
#else
  // Automatic. PSRAM when the board has it, internal RAM otherwise, and never a
  // crossover: a 50-200KB arena does not fit in the internal RAM left over next
  // to Wi-Fi and BLE, so a silent fallback would exhaust the heap and surface
  // later as a crash somewhere unrelated.
  return MemoryManager::has_psram() ? RAMAllocator<uint8_t>::ALLOC_EXTERNAL : RAMAllocator<uint8_t>::ALLOC_INTERNAL;
#endif
}

}  // namespace

bool MemoryManager::has_psram() {
  size_t total_psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
  return total_psram > 0;
}

MemoryManager::AllocationResult MemoryManager::allocate_tensor_arena(size_t requested_size) {
  AllocationResult result;
  result.actual_size = requested_size;

  const uint8_t flags = arena_allocator_flags();
  const bool from_psram = (flags & RAMAllocator<uint8_t>::ALLOC_EXTERNAL) != 0;
  RAMAllocator<uint8_t> allocator(flags);

  result.data = allocator.make_unique_array_for_overwrite(requested_size);
  if (!result.data) {
    ESP_LOGE(TAG, "Failed to allocate %zu bytes of tensor arena from %s (%zu bytes free)", requested_size,
             from_psram ? LOG_STR_LITERAL("PSRAM") : LOG_STR_LITERAL("internal RAM"), allocator.get_free_heap_size());
    if (from_psram) {
      ESP_LOGE(TAG, "  PSRAM is fragmented (try a smaller tensor_arena_size) or used by other components");
    } else {
      ESP_LOGE(TAG, "  The arena is too large for the available internal RAM");
      ESP_LOGE(TAG, "  Use a smaller tensor_arena_size, or a board with PSRAM");
    }
    ESP_LOGE(TAG, "  arena_location: internal or psram sets the heap explicitly");
    result.actual_size = 0;
    return result;
  }

  result.from_psram = from_psram;
  ESP_LOGI(TAG, "Allocated tensor arena: %zu bytes @ %p (%s)", requested_size, result.data.get(),
           from_psram ? LOG_STR_LITERAL("PSRAM") : LOG_STR_LITERAL("SRAM"));
  return result;
}

void MemoryManager::report_memory_status(size_t requested_size, size_t allocated_size, size_t peak_usage,
                                         size_t model_size) {
  size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  size_t total_psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
  size_t free_psram = 0;

  if (total_psram > 0) {
    free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "PSRAM: %zuB free of %zuB total (%.1fKB / %.1fKB)", free_psram, total_psram, free_psram / 1024.0f,
             total_psram / 1024.0f);
  }

  // Calculate total memory usage
  size_t total_memory_usage = model_size + peak_usage;

  ESP_LOGI(TAG, "Memory Status (Complete Picture):");
  ESP_LOGI(TAG, "  Model Size: %zuB (%.1fKB)", model_size, model_size / 1024.0f);
  ESP_LOGI(TAG, "  Tensor Arena (Requested): %zuB (%.1fKB)", requested_size, requested_size / 1024.0f);
  ESP_LOGI(TAG, "  Tensor Arena (Allocated): %zuB (%.1fKB)", allocated_size, allocated_size / 1024.0f);
  ESP_LOGI(TAG, "  Arena Peak Usage: %zuB (%.1fKB)", peak_usage, peak_usage / 1024.0f);
  ESP_LOGI(TAG, "  TOTAL Memory Usage: %zuB (%.1fKB)", total_memory_usage, total_memory_usage / 1024.0f);
  ESP_LOGI(TAG, "  Free Internal Heap: %zuB (%.1fKB)", free_internal, free_internal / 1024.0f);

  if (model_size > 0) {
    float arena_model_ratio = static_cast<float>(allocated_size) / model_size;
    float total_model_ratio = static_cast<float>(total_memory_usage) / model_size;
    ESP_LOGI(TAG, "  Arena/Model Ratio: %.1fx", arena_model_ratio);
    ESP_LOGI(TAG, "  Total/Model Ratio: %.1fx", total_model_ratio);
  }
}

}  // namespace esphome::tflite_memory_handler

#endif  // USE_TFLITE_MICRO_HELPER
