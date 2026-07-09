#include "memory_manager.h"
#include "esphome/core/log.h"
#include "debug_utils.h"

namespace esphome {
namespace tflite_micro_helper {

static const char *const TAG = "MemoryManager";

bool MemoryManager::has_psram() {
  size_t total_psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
  return total_psram > 0;
}

size_t MemoryManager::parse_size_string(const std::string &size_str) {
  size_t multiplier = 1;
  std::string str = size_str;

  if (str.find("KB") != std::string::npos) {
    multiplier = 1024;
    str = str.substr(0, str.length() - 2);
  } else if (str.find("MB") != std::string::npos) {
    multiplier = 1024 * 1024;
    str = str.substr(0, str.length() - 2);
  } else if (str.find("B") != std::string::npos) {
    str = str.substr(0, str.length() - 1);
  }

  // Manual string to integer conversion
  const char *cstr = str.c_str();
  char *end_ptr;
  long size_value = strtol(cstr, &end_ptr, 10);

  if (end_ptr != cstr && *end_ptr == '\0' && size_value > 0) {
    // Check for overflow: size_value * multiplier must fit in size_t
    uint64_t total = static_cast<uint64_t>(size_value) * multiplier;
    if (total > SIZE_MAX) {
      ESP_LOGE(TAG, "Size value overflow: %ld * %zu exceeds SIZE_MAX", size_value, multiplier);
      return 0;
    }
    return static_cast<size_t>(total);
  }
  return 0;  // Parsing failed
}

MemoryManager::AllocationResult MemoryManager::allocate_tensor_arena(size_t requested_size) {
  ESP_LOGD(TAG, "Attempting to allocate %zu bytes for tensor arena", requested_size);

  AllocationResult result;
  result.actual_size = requested_size;

// Check build-time override flags (these don't need has_psram() check)
#ifdef TFLITE_FORCE_SRAM
  ESP_LOGI(TAG, "TFLITE_FORCE_SRAM is defined -- forcing SRAM allocation");
  uint8_t *arena_ptr =
      static_cast<uint8_t *>(heap_caps_aligned_alloc(16, requested_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

  if (!arena_ptr) {
    size_t free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    ESP_LOGE(TAG, "TFLITE_FORCE_SRAM: Failed to allocate %zu bytes from SRAM (only %zu bytes free)", requested_size,
             free_sram);
  }
  if (arena_ptr) {
    result.data =
        std::unique_ptr<uint8_t[], AllocationResult::HeapCapsDeleter>(arena_ptr, AllocationResult::HeapCapsDeleter());
    result.from_psram = false;
    ESP_LOGD(TAG, "Allocated tensor arena: %zu bytes @ %p (SRAM, forced)", requested_size, arena_ptr);
  } else {
    result.actual_size = 0;
  }
  return result;
#endif

#ifdef TFLITE_FORCE_PSRAM
  ESP_LOGI(TAG, "TFLITE_FORCE_PSRAM is defined -- forcing PSRAM allocation");
  uint8_t *arena_ptr =
      static_cast<uint8_t *>(heap_caps_aligned_alloc(16, requested_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

  if (!arena_ptr) {
    size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    ESP_LOGE(TAG, "TFLITE_FORCE_PSRAM: Failed to allocate %zu bytes from PSRAM (only %zu bytes free)", requested_size,
             free_psram);
  }

  if (arena_ptr) {
    result.data =
        std::unique_ptr<uint8_t[], AllocationResult::HeapCapsDeleter>(arena_ptr, AllocationResult::HeapCapsDeleter());
    result.from_psram = true;
    ESP_LOGD(TAG, "Allocated tensor arena: %zu bytes @ %p (PSRAM, forced)", requested_size, arena_ptr);
  } else {
    result.actual_size = 0;
  }
  return result;
#endif

  // ===== Normal (no override) path =====
  bool psram_available = has_psram();
  if (psram_available) {
    // PSRAM is present: try PSRAM only, no fallback to SRAM.
    // The tensor arena (typically 50-200KB) is too large for the small internal SRAM
    // on most ESP32-S3 boards. Falling back to SRAM would silently exhaust heap space
    // and cause hard-to-debug crashes later.
    size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    ESP_LOGD(TAG, "PSRAM available: %zu bytes free", free_psram);

    uint8_t *arena_ptr =
        static_cast<uint8_t *>(heap_caps_aligned_alloc(16, requested_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

    if (!arena_ptr) {
      ESP_LOGE(TAG, "PSRAM allocation failed! Requested: %zu bytes, Free PSRAM: %zu bytes", requested_size, free_psram);
      ESP_LOGE(TAG, "  Possible causes:");
      ESP_LOGE(TAG, "  - PSRAM is fragmented (try a smaller tensor_arena_size)");
      ESP_LOGE(TAG, "  - PSRAM is full (other components using it)");
      ESP_LOGE(TAG, "  - PSRAM not properly initialized in board config");
      ESP_LOGE(TAG, "  To force SRAM allocation, add build flag: -DTFLITE_FORCE_SRAM");
      result.actual_size = 0;
      return result;
    }

    result.data =
        std::unique_ptr<uint8_t[], AllocationResult::HeapCapsDeleter>(arena_ptr, AllocationResult::HeapCapsDeleter());
    result.from_psram = true;
    ESP_LOGI(TAG, "Allocated tensor arena: %zu bytes @ %p (PSRAM)", requested_size, arena_ptr);

  } else {
    // No PSRAM: allocate from SRAM (internal RAM)
    // This is the normal case for boards without PSRAM (some ESP32 variants)
    size_t free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    ESP_LOGD(TAG, "No PSRAM available. SRAM free: %zu bytes", free_sram);

    uint8_t *arena_ptr =
        static_cast<uint8_t *>(heap_caps_aligned_alloc(16, requested_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

    if (!arena_ptr) {
      ESP_LOGE(TAG, "Internal SRAM allocation failed! Requested: %zu bytes, Free SRAM: %zu bytes", requested_size,
               free_sram);
      ESP_LOGE(TAG, "  The tensor arena is too large for the available SRAM.");
      ESP_LOGE(TAG, "  Try reducing tensor_arena_size or use a board with PSRAM.");
      result.actual_size = 0;
      return result;
    }

    result.data =
        std::unique_ptr<uint8_t[], AllocationResult::HeapCapsDeleter>(arena_ptr, AllocationResult::HeapCapsDeleter());
    result.from_psram = false;
    ESP_LOGI(TAG, "Allocated tensor arena: %zu bytes @ %p (SRAM)", requested_size, arena_ptr);
  }

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

}  // namespace tflite_micro_helper
}  // namespace esphome
