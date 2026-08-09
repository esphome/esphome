#include "tflite_micro_helper.h"
#include <cstdlib>
#include <algorithm>

namespace esphome::tflite_micro_helper {

static const char *const TAG = "tflite_micro_helper";

void TFLiteMicroHelper::set_tensor_arena_size(size_t size) { this->tensor_arena_size_requested_ = size; }

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
  config.output_processing = this->output_processing_;
  config.scale_factor = this->scale_factor_;
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
  if (!this->is_model_loaded()) {
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

// Image model setters
void TFLiteMicroHelper::set_input_type(const std::string &t) { this->input_type_ = t; }
void TFLiteMicroHelper::set_input_channels(int c) { this->input_channels_ = c; }
void TFLiteMicroHelper::set_input_width(int w) { this->input_width_ = w; }
void TFLiteMicroHelper::set_input_height(int h) { this->input_height_ = h; }
void TFLiteMicroHelper::set_input_order(const std::string &o) { this->input_order_ = o; }
void TFLiteMicroHelper::set_normalize(bool n) { this->normalize_ = n; }
void TFLiteMicroHelper::set_invert(bool i) { this->invert_ = i; }

// Audio model setters.
// NOTE: TFLiteMicroHelper is *configuration-only* for audio (option b of the review
// plan). The setters below populate audio_config_ as a pass-through so consumers can
// read the validated values, but the streaming runtime (MRV, sliding window, stride
// management) is owned by the consumer (e.g. micro_wake_word::StreamingModel).
void TFLiteMicroHelper::set_probability_cutoff(float f) {
  // Saturating clamp [0.0, 1.0] plus NaN guard before the uint8 cast (E10).
  if (f != f) {
    f = 0.5f;  // NaN -> default midpoint
  }
  f = std::max(0.0f, std::min(1.0f, f));
  this->audio_config_.probability_cutoff = static_cast<uint8_t>(f * 255.0f);
}
void TFLiteMicroHelper::set_sliding_window_size(size_t n) { this->audio_config_.sliding_window_size = n; }
void TFLiteMicroHelper::set_features_step_size(uint8_t ms) { this->audio_config_.features_step_size = ms; }
void TFLiteMicroHelper::set_feature_count(size_t n) { this->audio_config_.feature_count = n; }

bool TFLiteMicroHelper::load_model() {
  // E2: single atomic state machine -- reject concurrent loads, only transition
  // LOADING -> READY at the very end on success.
  LoadState expected = LoadState::UNLOADED;
  if (!this->state_.compare_exchange_strong(expected, LoadState::LOADING)) {
    if (this->state_.load() == LoadState::READY) {
      ESP_LOGW(TAG, "load_model called while already loaded -- returning true");
      return true;
    }
    ESP_LOGE(TAG, "load_model called while another load is in progress");
    return false;
  }
  ESP_LOGI(TAG, "Loading TFLite model...");

  if (this->model_data_ == nullptr || this->model_length_ == 0) {
    ESP_LOGE(TAG, "Model data is NULL or empty");
    this->state_.store(LoadState::UNLOADED);
    return false;
  }

  if (!this->model_handler_.verify_model_crc(this->model_data_, this->model_length_, this->expected_crc32_)) {
    ESP_LOGE(TAG, "Model CRC32 verification failed");
    this->state_.store(LoadState::UNLOADED);
    return false;
  }

  // E1: probe the actual required arena size (PSRAM-aware, replaces the old 1.5x
  // ESP32-S3 cache bump). Falls back to the configured size if probing fails.
  size_t probe_size = this->model_handler_.probe_arena_size(this->model_data_, this->tensor_arena_size_requested_);
  if (probe_size > 0 && probe_size < this->tensor_arena_size_requested_) {
    ESP_LOGI(TAG, "Probed arena size %zu bytes < configured %zu bytes -- using probed size", probe_size,
             this->tensor_arena_size_requested_);
    this->tensor_arena_size_requested_ = probe_size;
  } else if (probe_size > 0 && probe_size > this->tensor_arena_size_requested_) {
    ESP_LOGW(TAG, "Model needs %zu bytes > configured %zu bytes -- using probed size", probe_size,
             this->tensor_arena_size_requested_);
    this->tensor_arena_size_requested_ = probe_size;
  } else if (probe_size == 0) {
    ESP_LOGW(TAG, "Arena probe failed, using configured size: %zu bytes", this->tensor_arena_size_requested_);
  } else {
    ESP_LOGI(TAG, "Probed size matches configured size: %zu bytes", this->tensor_arena_size_requested_);
  }

  if (!this->allocate_tensor_arena_()) {
    this->state_.store(LoadState::UNLOADED);
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
    this->state_.store(LoadState::UNLOADED);
    return false;
  }

  // Sync image config to ModelHandler for image models
  if (this->model_type_ == "image") {
    ImageModelConfig img_cfg;
    img_cfg.input_type = this->input_type_;
    img_cfg.input_channels = this->input_channels_;
    img_cfg.input_order = this->input_order_;
    img_cfg.input_size = {this->input_width_, this->input_height_};
    img_cfg.normalize = this->normalize_;
    img_cfg.invert = this->invert_;
    this->model_handler_.set_image_config(img_cfg);
  }

  this->state_.store(LoadState::READY);
  ESP_LOGI(TAG, "Model loaded successfully");
  return true;
}

void TFLiteMicroHelper::unload_model() {
  ESP_LOGI(TAG, "Unloading model and freeing arena");
  this->model_handler_.unload();

  // Free the arena owned by this helper. ModelHandler::unload() only resets its
  // own (empty) allocation, so we must release the one allocated here.
  this->tensor_arena_allocation_.data.reset();
  this->tensor_arena_allocation_.actual_size = 0;

  // E3: reset runtime state (stats cache + load state). Config fields (model data,
  // size, CRC, input/audio config) are intentionally preserved: the consumer reload
  // path (meter_reader_tflite::reload_resources) calls unload_model() then
  // load_model() without re-issuing set_model()/set_expected_crc32().
  {
    std::scoped_lock<std::mutex> lock(this->arena_stats_mutex_);
    this->cached_arena_stats_ = ArenaStats{};
  }
  this->state_.store(LoadState::UNLOADED);
}

ModelSpec TFLiteMicroHelper::get_model_spec() const {
  ModelSpec spec;
  spec.input_width = this->model_handler_.get_input_width();
  spec.input_height = this->model_handler_.get_input_height();
  spec.input_channels = this->model_handler_.get_input_channels();
  spec.normalize = this->model_handler_.get_image_config().normalize;
  spec.input_order = this->model_handler_.get_image_config().input_order;

  // E5: derive input type directly from the tensor type. The old heuristic
  // (bytes == num_elements * 4) mis-classified audio/3D models because
  // num_elements = 0*0*0 = 0 made the comparison always match.
  const TfLiteTensor *input = this->model_handler_.input_tensor();
  if (input != nullptr) {
    spec.input_type = (input->type == kTfLiteFloat32) ? 1 : 0;
  } else {
    spec.input_type = 0;
  }
  return spec;
}

ArenaStats TFLiteMicroHelper::get_arena_stats() const {
  std::scoped_lock<std::mutex> lock(this->arena_stats_mutex_);
  return this->cached_arena_stats_;
}

void TFLiteMicroHelper::update_arena_stats_cache() {
  ArenaStats stats;
  stats.total_size = this->tensor_arena_allocation_.actual_size;
  stats.used_bytes = this->model_handler_.get_arena_used_bytes();
  stats.wasted_bytes = (stats.total_size > stats.used_bytes) ? (stats.total_size - stats.used_bytes) : 0;
  stats.efficiency = (stats.total_size > 0) ? (100.0f * stats.used_bytes / stats.total_size) : 0.0f;
  std::scoped_lock<std::mutex> lock(this->arena_stats_mutex_);
  this->cached_arena_stats_ = stats;
}

void TFLiteMicroHelper::report_memory_status() {
  MemoryManager::report_memory_status(this->tensor_arena_size_requested_, this->tensor_arena_allocation_.actual_size,
                                      this->model_handler_.get_arena_used_bytes(), this->model_length_);
}

}  // namespace esphome::tflite_micro_helper
