#pragma once

#ifdef USE_ESP_IDF

#include <memory>
#include <cstring>
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <tensorflow/lite/core/c/common.h>
#include <tensorflow/lite/micro/micro_interpreter.h>

namespace esphome {
namespace micro_wake_word {

class ModelData {
 public:
  ModelData() = default;
  ~ModelData();

  // Disable copy, allow move
  ModelData(const ModelData &) = delete;
  ModelData &operator=(const ModelData &) = delete;
  ModelData(ModelData &&other) noexcept;
  ModelData &operator=(ModelData &&other) noexcept;

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

}  // namespace micro_wake_word
}  // namespace esphome

#endif  // USE_ESP_IDF
