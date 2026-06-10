#include "model_data.h"

#ifdef USE_ESP_IDF

namespace esphome {
namespace micro_wake_word {

static const char *const TAG = "micro_wake_word";

ModelData::~ModelData() { this->deallocate_(); }

ModelData::ModelData(ModelData &&other) noexcept {
  std::swap(this->data_, other.data_);
  std::swap(this->size_, other.size_);
  std::swap(this->valid_, other.valid_);
}

ModelData &ModelData::operator=(ModelData &&other) noexcept {
  if (this != &other) {
    this->deallocate_();
    std::swap(this->data_, other.data_);
    std::swap(this->size_, other.size_);
    std::swap(this->valid_, other.valid_);
  }
  return *this;
}

bool ModelData::allocate(size_t size) {
  // If we already have enough space, just update size
  if (this->data_ && this->size_ >= size) {
    this->size_ = size;
    this->valid_ = false;  // Need to revalidate with new data
    return true;
  }

  // Need new allocation
  this->deallocate_();

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
  if (!this->data_ || this->size_ < 4) {
    ESP_LOGE(TAG, "Model data is null or too small");
    return false;
  }

  // Check TFLite magic number "TFL3" in bytes 4-7
  if (memcmp(this->data_ + 4, "TFL3", 4) != 0) {
    ESP_LOGE(TAG, "Invalid TFLite model magic number");
    return false;
  }

  // Verify model can be parsed
  const tflite::Model *model = tflite::GetModel(this->data_);
  if (!model) {
    ESP_LOGE(TAG, "Failed to parse TFLite model");
    return false;
  }

  if (model->version() != TFLITE_SCHEMA_VERSION) {
    ESP_LOGE(TAG, "TFLite model version mismatch (expected %d, got %d)", TFLITE_SCHEMA_VERSION, model->version());
    return false;
  }

  this->valid_ = true;
  return true;
}

}  // namespace micro_wake_word
}  // namespace esphome

#endif  // USE_ESP_IDF
