#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include "esphome/components/tflite_model_loader/engine.h"

namespace esphome::tflite_memory_handler {

class MemoryManager {
 public:
  struct AllocationResult {
    std::unique_ptr<uint8_t[]> data;
    size_t actual_size{0};
    bool from_psram{false};
    explicit operator bool() const { return this->data != nullptr; }
    void reset() { *this = {}; }
  };
  static AllocationResult allocate_tensor_arena(size_t size) {
    if (!tflite_runtime_testing::engine.allocation_succeeds)
      return {};
    return {std::make_unique<uint8_t[]>(size), size, false};
  }
  static void report_memory_status(size_t, size_t, size_t, size_t) {}
};

}  // namespace esphome::tflite_memory_handler
