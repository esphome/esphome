#pragma once

#ifdef USE_ESP32

#include <cstddef>
#include <cstdint>
#include "esphome/core/helpers.h"

namespace esphome::micro_wake_word {

// Owns the buffer holding a runtime-downloaded TFLite model. The buffer prefers PSRAM but falls back to
// internal RAM, so a device without PSRAM can still hold a single model. It is filled over HTTP, checked
// for integrity by the caller (SHA256) and for a usable TFLite header here, then kept alive for the
// lifetime of the WakeWordModel that uses it. Only ever held behind a std::shared_ptr, so copies and
// moves are disabled.
class ModelData {
 public:
  ModelData() = default;
  ~ModelData();

  // Non-copyable, non-movable
  ModelData(const ModelData &) = delete;
  ModelData &operator=(const ModelData &) = delete;
  ModelData(ModelData &&) = delete;
  ModelData &operator=(ModelData &&) = delete;

  // Allocate memory for model
  bool allocate(size_t size);

  // Get stable pointer for TFLite (only valid after validate_and_mark_ready())
  const uint8_t *get_model_pointer() const;

  // Get writable pointer for downloading (invalidates the model)
  uint8_t *get_write_pointer();

  // Validate TFLite model and mark as ready for use
  bool validate_and_mark_ready();

  // Check if model is valid and ready for use
  bool is_valid() const { return this->valid_; }

  // Get size of model data
  size_t size() const { return this->size_; }

  // Check if memory is allocated
  bool is_allocated() const { return this->data_ != nullptr; }

 protected:
  // Deallocate memory
  void deallocate_();

  uint8_t *data_{nullptr};
  size_t size_{0};
  bool valid_{false};
  RAMAllocator<uint8_t> allocator_{RAMAllocator<uint8_t>::NONE};
};

}  // namespace esphome::micro_wake_word

#endif  // USE_ESP32
