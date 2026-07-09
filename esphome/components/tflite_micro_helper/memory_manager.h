#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <memory>
#include <cstdint>
#include "esp_heap_caps.h"

namespace esphome {
namespace tflite_micro_helper {

class MemoryManager {
 public:
  struct AllocationResult {
    struct HeapCapsDeleter {
      void operator()(uint8_t *p) const {
        if (p)
          heap_caps_free(p);
      }
    };

    std::unique_ptr<uint8_t[], HeapCapsDeleter> data;
    size_t actual_size;

    // Source of allocation for logging / diagnostics
    bool from_psram{false};

    operator bool() const { return static_cast<bool>(data); }
  };

  [[nodiscard]] static AllocationResult allocate_tensor_arena(size_t requested_size);
  static void report_memory_status(size_t requested_size, size_t allocated_size, size_t peak_usage, size_t model_size);

  /**
   * @brief Parse an arena size string (e.g. "110KB", "2MB", "51200") to bytes.
   * Shared utility -- replaces duplicated logic in ModelHandler and TFLiteMicroHelper.
   * @param size_str The size string to parse.
   * @return The size in bytes, or 0 if parsing fails.
   */
  static size_t parse_size_string(const std::string &size_str);

  /**
   * @brief Check if PSRAM (SPIRAM) is available on this device.
   * @return true if PSRAM is present and has non-zero total size.
   */
  static bool has_psram();
};

}  // namespace tflite_micro_helper
}  // namespace esphome

#endif  // USE_TFLITE_MICRO_HELPER
