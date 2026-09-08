#include "model_data.h"

#ifdef USE_ESP32

#include <cstring>
#include "esphome/core/log.h"

#include <tensorflow/lite/core/c/common.h>
#include <tensorflow/lite/micro/micro_interpreter.h>

namespace esphome::micro_wake_word {

static const char *const TAG = "micro_wake_word";

ModelData::~ModelData() { this->deallocate_(); }

bool ModelData::allocate(size_t size) {
  // Reject up front: reallocating to zero frees the buffer and returns null, which would leave data_ pointing at
  // freed memory. A zero-length model is never usable anyway.
  if (size == 0) {
    ESP_LOGE(TAG, "Refusing to allocate a zero-length model");
    return false;
  }

  // Already allocated, so reallocate to the new size
  if (this->data_) {
    uint8_t *new_allocation = this->allocator_.reallocate(this->data_, size);
    if (new_allocation == nullptr) {
      ESP_LOGE(TAG, "Failed to reallocate %zu bytes", size);
      return false;
    }
    this->data_ = new_allocation;
    this->size_ = size;
    this->valid_ = false;  // Need to revalidate with new data
    return true;
  }

  // Try to allocate in PSRAM first
  this->data_ = this->allocator_.allocate(size);
  if (this->data_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate %zu bytes", size);
    return false;
  }

  this->size_ = size;
  this->valid_ = false;
  return true;
}

void ModelData::deallocate_() {
  if (this->data_ != nullptr) {
    this->allocator_.deallocate(this->data_, this->size_);
    this->data_ = nullptr;
    this->size_ = 0;
    this->valid_ = false;
  }
}

const uint8_t *ModelData::get_model_pointer() const { return this->valid_ ? this->data_ : nullptr; }

uint8_t *ModelData::get_write_pointer() {
  this->valid_ = false;  // Mark invalid while writing
  return this->data_;
}

bool ModelData::validate_and_mark_ready() {
  // The magic number lives in bytes 4-7, so we need at least 8 bytes to read it.
  if (!this->data_ || this->size_ < 8) {
    ESP_LOGE(TAG, "Model data is null or too small");
    return false;
  }

  // Check TFLite magic number "TFL3" in bytes 4-7
  if (memcmp(this->data_ + 4, "TFL3", 4) != 0) {
    ESP_LOGE(TAG, "Invalid TFLite model magic number");
    return false;
  }

  // Bytes 0-3 hold the offset of the root table. tflite::GetModel only adds that offset to the start of the
  // buffer, so check it lands inside the buffer before reading through it.
  uint32_t root_offset;
  memcpy(&root_offset, this->data_, sizeof(root_offset));
  if (root_offset >= this->size_) {
    ESP_LOGE(TAG, "TFLite model root offset is out of bounds");
    return false;
  }

  const tflite::Model *model = tflite::GetModel(this->data_);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    ESP_LOGE(TAG, "TFLite model version mismatch (expected %d, got %d)", TFLITE_SCHEMA_VERSION, model->version());
    return false;
  }

  this->valid_ = true;
  return true;
}

}  // namespace esphome::micro_wake_word

#endif  // USE_ESP32
