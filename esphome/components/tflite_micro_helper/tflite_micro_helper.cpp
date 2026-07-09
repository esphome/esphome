#include "tflite_micro_helper.h"
#include <cstdlib>

namespace esphome {
namespace tflite_micro_helper {

static const char *const TAG = "tflite_micro_helper";

void TFLiteMicroHelper::set_tensor_arena_size(size_t size) {
  this->tensor_arena_size_requested_ = size;
  this->arena_bumped_ = false;
}

void TFLiteMicroHelper::set_model(const uint8_t *model_data, size_t model_size) {
  this->model_data_ = model_data;
  this->model_length_ = model_size;
}

bool TFLiteMicroHelper::allocate_tensor_arena_() {
  ESP_LOGI(TAG, "Allocating tensor arena: %zu bytes", this->tensor_arena_size_requested_);

  this->tensor_arena_allocation_ = MemoryManager::allocate_tensor_arena(this->tensor_arena_size_requested_);
  if (!this->tensor_arena_allocation_) {
    ESP_LOGE(TAG, "Failed to allocate tensor arena");
    return false;
  }

  ESP_LOGI(TAG, "Tensor arena allocated: %zu bytes", this->tensor_arena_allocation_.actual_size);
  return true;
}

ModelConfig TFLiteMicroHelper::build_config_() {
  ModelConfig config;
  config.description = this->model_type_;
  config.input_type = this->input_type_;
  config.input_channels = this->input_channels_;
  config.input_size = {this->input_width_, this->input_height_};
  config.output_processing = this->output_processing_;
  config.scale_factor = this->scale_factor_;
  config.input_order = this->input_order_;
  config.normalize = this->normalize_;
  config.invert = this->invert_;
  config.tensor_arena_size = "";  // Arena is handled directly
  return config;
}

bool TFLiteMicroHelper::validate_input_tensor_(const uint8_t *src_data, size_t src_size) const {
  if (!src_data) {
    ESP_LOGE(TAG, "Null input data pointer");
    return false;
  }
  const TfLiteTensor *input = this->model_handler_.input_tensor();
  if (!input) {
    ESP_LOGE(TAG, "Input tensor is null");
    return false;
  }
  if (src_size != input->bytes) {
    ESP_LOGE(TAG, "Input size mismatch: expected %zu, got %zu", input->bytes, src_size);
    return false;
  }
  if (input->type != kTfLiteUInt8 && input->type != kTfLiteInt8 && input->type != kTfLiteFloat32) {
    ESP_LOGE(TAG, "Unsupported input tensor type: %d", input->type);
    return false;
  }
  return true;
}

bool TFLiteMicroHelper::run_inference_on_buffer(const uint8_t *src_data, size_t src_size) {
  if (!this->model_loaded_.load()) {
    ESP_LOGE(TAG, "Cannot run inference -- model not loaded");
    return false;
  }
  if (!this->validate_input_tensor_(src_data, src_size)) {
    return false;
  }

  TfLiteTensor *input = this->model_handler_.input_tensor();
  if (input->type == kTfLiteFloat32) {
    memcpy(input->data.f, src_data, src_size);
  } else if (input->type == kTfLiteInt8) {
    memcpy(input->data.int8, src_data, src_size);
  } else {
    memcpy(input->data.uint8, src_data, src_size);
  }

  if (this->model_handler_.invoke() != kTfLiteOk) {
    ESP_LOGE(TAG, "Model invocation failed");
    return false;
  }
  this->update_arena_stats_cache();
  return true;
}

ProcessedOutput TFLiteMicroHelper::run_inference(const uint8_t *src_data, size_t src_size) {
  if (!this->run_inference_on_buffer(src_data, src_size)) {
    return {0.0f, 0.0f};
  }
  TfLiteTensor *output = this->model_handler_.output_tensor();
  if (!output) {
    return {0.0f, 0.0f};
  }
  return this->model_handler_.process_output(output);
}

bool TFLiteMicroHelper::load_model() {
  ESP_LOGI(TAG, "Loading TFLite model...");

// ESP32-S3: 1.5x arena bump for cache line alignment (once per instance)
#if defined(CONFIG_ESP32S3_DATA_CACHE_64KB) && defined(CONFIG_ESP32S3_DATA_CACHE_LINE_64B)
  if (!this->arena_bumped_) {
    size_t original = this->tensor_arena_size_requested_;
    this->tensor_arena_size_requested_ = (this->tensor_arena_size_requested_ * 3 + 1) / 2;
    this->arena_bumped_ = true;
    ESP_LOGI(TAG, "ESP32-S3 cache: bumped arena (%zu -> %zu bytes)", original, this->tensor_arena_size_requested_);
  }
#endif

  if (this->model_data_ == nullptr || this->model_length_ == 0) {
    ESP_LOGE(TAG, "Model data is NULL or empty");
    return false;
  }

  // CRC check before allocation
  if (!this->model_handler_.verify_model_crc(this->model_data_, this->model_length_)) {
    ESP_LOGE(TAG, "Model CRC32 verification failed");
    return false;
  }

  if (!this->allocate_tensor_arena_()) {
    return false;
  }

  ModelConfig config = this->build_config_();
  if (!this->model_handler_.load_model_with_arena(this->model_data_, this->model_length_,
                                                  this->tensor_arena_allocation_.data.get(),
                                                  this->tensor_arena_allocation_.actual_size, config)) {
    ESP_LOGE(TAG, "Failed to load model into interpreter");
    this->model_handler_.unload();
    this->tensor_arena_allocation_.data.reset();
    this->tensor_arena_allocation_.actual_size = 0;
    return false;
  }

  this->model_loaded_ = true;
  ESP_LOGI(TAG, "Model loaded successfully");
  return true;
}

void TFLiteMicroHelper::unload_model() {
  ESP_LOGI(TAG, "Unloading model and freeing arena");
  this->model_handler_.unload();
  this->model_loaded_ = false;
  {
    std::lock_guard<std::mutex> lock(this->arena_stats_mutex_);
    this->cached_arena_stats_ = ArenaStats{};
  }
}

ModelSpec TFLiteMicroHelper::get_model_spec() const {
  ModelSpec spec;
  spec.input_width = this->model_handler_.get_input_width();
  spec.input_height = this->model_handler_.get_input_height();
  spec.input_channels = this->model_handler_.get_input_channels();
  spec.normalize = this->model_handler_.get_config().normalize;
  spec.input_order = this->model_handler_.get_config().input_order;

  const TfLiteTensor *input = this->model_handler_.input_tensor();
  size_t num_elements = spec.input_width * spec.input_height * spec.input_channels;
  if (input && input->bytes == num_elements * 4) {
    spec.input_type = 1;  // Float32
  } else {
    spec.input_type = 0;  // Uint8
  }
  return spec;
}

ArenaStats TFLiteMicroHelper::get_arena_stats() const {
  std::lock_guard<std::mutex> lock(this->arena_stats_mutex_);
  return this->cached_arena_stats_;
}

void TFLiteMicroHelper::update_arena_stats_cache() {
  ArenaStats stats;
  stats.total_size = this->tensor_arena_allocation_.actual_size;
  stats.used_bytes = this->model_handler_.get_arena_used_bytes();
  stats.wasted_bytes = (stats.total_size > stats.used_bytes) ? (stats.total_size - stats.used_bytes) : 0;
  stats.efficiency = (stats.total_size > 0) ? (100.0f * stats.used_bytes / stats.total_size) : 0.0f;

  std::lock_guard<std::mutex> lock(this->arena_stats_mutex_);
  this->cached_arena_stats_ = stats;
}

void TFLiteMicroHelper::report_memory_status() {
  MemoryManager::report_memory_status(this->tensor_arena_size_requested_, this->tensor_arena_allocation_.actual_size,
                                      this->model_handler_.get_arena_used_bytes(), this->model_length_);
}

}  // namespace tflite_micro_helper
}  // namespace esphome
