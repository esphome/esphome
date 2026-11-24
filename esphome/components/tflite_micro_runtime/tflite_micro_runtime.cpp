#include "tflite_micro_runtime.h"

#ifdef USE_TFLITE_MICRO_HELPER
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <esp_heap_caps.h>

namespace esphome::tflite_micro_runtime {

ESPHOME_LOG_TAG(TAG, "tflite_micro_runtime");

namespace {

const char *input_type_name(InputType type) {
  switch (type) {
    case InputType::INPUT_TYPE_UINT8:
      return "uint8";
    case InputType::INPUT_TYPE_INT8:
      return "int8";
    case InputType::INPUT_TYPE_FLOAT32:
      return "float32";
    case InputType::INPUT_TYPE_UNKNOWN:
      break;
  }
  return "unknown";
}

InputType tensor_type_to_input_type(TfLiteType type) {
  switch (type) {
    case kTfLiteUInt8:
      return InputType::INPUT_TYPE_UINT8;
    case kTfLiteInt8:
      return InputType::INPUT_TYPE_INT8;
    case kTfLiteFloat32:
      return InputType::INPUT_TYPE_FLOAT32;
    default:
      return InputType::INPUT_TYPE_UNKNOWN;
  }
}

}  // namespace

TFLiteMicroHelper::LockGuard::LockGuard(TFLiteMicroHelper *helper) : helper_(helper) {
  if (this->helper_ == nullptr)
    return;
  const TaskHandle_t current = xTaskGetCurrentTaskHandle();
  if (this->helper_->lock_owner_.load(std::memory_order_relaxed) == current) {
    // A log emitted under the lock can run logger.on_message automations that call
    // back into the helper; blocking here would deadlock the main task.
    if (!this->helper_->reentry_reported_.exchange(true, std::memory_order_relaxed))
      ESP_LOGE(TAG, "A log callback re-entered the runtime; rejecting the nested call");
    return;
  }
  this->helper_->mutex_.lock();
  this->helper_->lock_owner_.store(current, std::memory_order_relaxed);
  this->locked_ = true;
}

TFLiteMicroHelper::LockGuard::~LockGuard() {
  if (!this->locked_)
    return;
  this->helper_->reentry_reported_.store(false, std::memory_order_relaxed);
  this->helper_->lock_owner_.store(nullptr, std::memory_order_relaxed);
  this->helper_->mutex_.unlock();
}

bool TFLiteMicroHelper::is_main_task_() { return xTaskGetCurrentTaskHandle() == esphome_main_task_handle; }

void TFLiteMicroHelper::setup() {
  if (this->defer_load_) {
    // A streaming consumer owns the model and its resource variables.
    const LogString *reason = nullptr;
    {
      LockGuard guard(this);
      reason = guard.locked() ? this->check_model_integrity_() : LOG_STR("Runtime re-entered during setup");
    }
    if (reason != nullptr) {
      this->mark_failed(reason);
      return;
    }
    ESP_LOGD(TAG, "Model load left to the streaming consumer");
    return;
  }
  // Usable without a consumer: load here so the model is verified and reported at boot.
  if (!this->load_model())
    this->mark_failed(LOG_STR("Model load failed"));
}

void TFLiteMicroHelper::loop() {
  if (this->pending_epoch_.load(std::memory_order_acquire) != this->dispatched_epoch_.load(std::memory_order_acquire))
    this->dispatch_result_();
}

void TFLiteMicroHelper::dump_config() {
  ESP_LOGCONFIG(TAG,
                "TFLite Micro Runtime:\n"
                "  Input pipeline: %s\n"
                "  Model size: %zu bytes\n"
                "  Tensor arena: %zu bytes configured",
                this->input_pipeline_ != nullptr ? this->input_pipeline_->name() : "raw", this->get_model_size_bytes(),
                this->get_tensor_arena_size());

  if (!this->is_model_loaded()) {
    ESP_LOGCONFIG(TAG, "  Model not loaded");
    return;
  }
  const ModelSpec spec = this->get_model_spec();
  ESP_LOGCONFIG(TAG,
                "  Input: %dx%dx%d %s\n"
                "  Tensor arena allocated: %zu bytes, %zu bytes requested after probing, %zu bytes in use",
                spec.input_width, spec.input_height, spec.input_channels, input_type_name(spec.input_type),
                this->get_tensor_arena_size_actual(), this->get_tensor_arena_size_effective(),
                this->get_arena_used_bytes());
}

void TFLiteMicroHelper::set_tensor_arena_size(size_t size) { this->tensor_arena_size_requested_ = size; }

bool TFLiteMicroHelper::set_model(const uint8_t *model_data, size_t model_size, uint32_t expected_crc32) {
  LockGuard guard(this);
  if (!guard.locked())
    return false;
  if (this->state_.load(std::memory_order_relaxed) != LoadState::LOAD_STATE_UNLOADED) {
    // READY: the interpreter still uses the old buffer. LOADING: it is being read now.
    ESP_LOGE(TAG, "set_model() while a model is loaded or loading -- call unload_model() first");
    return false;
  }
  this->model_data_ = model_data;
  this->model_length_ = model_size;
  this->expected_crc32_ = expected_crc32;
  return true;
}

bool TFLiteMicroHelper::allocate_tensor_arena_() {
  ESP_LOGD(TAG, "Allocating tensor arena: %zu bytes", this->tensor_arena_size_effective_);
  this->tensor_arena_allocation_ = MemoryManager::allocate_tensor_arena(this->tensor_arena_size_effective_);
  if (!this->tensor_arena_allocation_) {
    this->arena_heap_ = ArenaHeap::ARENA_HEAP_UNKNOWN;
    ESP_LOGE(TAG, "Failed to allocate tensor arena");
    return false;
  }
  // Cached here: report_memory_status() also runs after an unload.
  this->arena_heap_ =
      this->tensor_arena_allocation_.from_psram ? ArenaHeap::ARENA_HEAP_PSRAM : ArenaHeap::ARENA_HEAP_INTERNAL;
  ESP_LOGD(TAG, "Tensor arena allocated: %zu bytes", this->tensor_arena_allocation_.actual_size);
  return true;
}

ModelConfig TFLiteMicroHelper::build_config_() {
  ModelConfig config;
  config.description = this->input_pipeline_ != nullptr ? this->input_pipeline_->name() : "raw";
  config.output_processing = this->output_processing_.str();
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

bool TFLiteMicroHelper::prepare_run_() {
  if (this->pending_epoch_.load(std::memory_order_acquire) == this->dispatched_epoch_.load(std::memory_order_acquire))
    return true;
  if (is_main_task_()) {
    // A nested run from an on_result callback is allowed: that callback is already
    // consuming the pending result.
    if (!this->dispatching_.load(std::memory_order_relaxed))
      this->dispatch_result_();
    return true;
  }
  ESP_LOGW(TAG, "The previous result has not been delivered yet; skipping this inference");
  return false;
}

void TFLiteMicroHelper::dispatch_result_() {
  uint32_t epoch = 0;
  {
    LockGuard guard(this);
    if (!guard.locked())
      return;
    epoch = this->pending_epoch_.load(std::memory_order_acquire);
    if (epoch == this->dispatched_epoch_.load(std::memory_order_acquire))
      return;
  }
  this->dispatching_.store(true, std::memory_order_release);
  this->result_callback_.call();
  this->dispatching_.store(false, std::memory_order_release);
  // A nested run may have queued a newer result: mark only this epoch delivered.
  uint32_t seen = this->dispatched_epoch_.load(std::memory_order_acquire);
  while (epoch > seen && !this->dispatched_epoch_.compare_exchange_weak(seen, epoch, std::memory_order_release)) {
  }
}

void TFLiteMicroHelper::finish_run_(bool succeeded) {
  if (succeeded && is_main_task_())
    this->dispatch_result_();
}

bool TFLiteMicroHelper::run_inference_on_buffer(const uint8_t *src_data, size_t src_size) {
  if (!this->prepare_run_())
    return false;
  bool succeeded = false;
  {
    LockGuard guard(this);
    if (!guard.locked())
      return false;
    succeeded = this->run_inference_on_buffer_locked_(src_data, src_size);
    if (succeeded)
      this->pending_epoch_.fetch_add(1, std::memory_order_release);
  }
  this->finish_run_(succeeded);
  return succeeded;
}

bool TFLiteMicroHelper::run_inference_on_buffer_locked_(const uint8_t *src_data, size_t src_size) {
  if (!this->is_model_loaded()) {
    ESP_LOGE(TAG, "Cannot run inference -- model not loaded");
    return false;
  }
  if (this->model_handler_.input_count() != 1) {
    ESP_LOGE(TAG, "A raw inference needs one input, this model has %zu", this->model_handler_.input_count());
    return false;
  }
  if (!this->validate_input_tensor_(src_data, src_size)) {
    return false;
  }
  // validate_input_tensor_() checked type and size, so one raw copy serves every type.
  memcpy(this->model_handler_.input_tensor()->data.raw, src_data, src_size);
  if (this->model_handler_.invoke() != kTfLiteOk) {
    ESP_LOGE(TAG, "Model invocation failed");
    return false;
  }
  return true;
}

TfLiteStatus TFLiteMicroHelper::invoke() {
  LockGuard guard(this);
  if (!guard.locked())
    return kTfLiteError;
  return this->invoke_locked_();
}

TfLiteStatus TFLiteMicroHelper::invoke_locked_() {
  if (!this->is_model_loaded()) {
    ESP_LOGE(TAG, "Cannot invoke -- model not loaded");
    return kTfLiteError;
  }
  return this->model_handler_.invoke();
}

bool TFLiteMicroHelper::decode_outputs_locked_() {
  if (this->output_decoder_ == nullptr)
    return true;
  static constexpr size_t MAX_OUTPUTS = 8;
  const size_t count = this->model_handler_.output_count();
  if (count > MAX_OUTPUTS) {
    ESP_LOGE(TAG, "Model has %zu outputs, more than the %zu a decoder can take", count, MAX_OUTPUTS);
    return false;
  }
  TfLiteTensor *outputs[MAX_OUTPUTS];
  for (size_t i = 0; i < count; i++)
    outputs[i] = this->model_handler_.output(i);
  return this->output_decoder_->decode(outputs, count);
}

bool TFLiteMicroHelper::run_locked_(const uint8_t *src_data, size_t src_size) {
  if (!this->is_model_loaded()) {
    ESP_LOGE(TAG, "Cannot run inference -- model not loaded");
    return false;
  }
  if (this->input_pipeline_ != nullptr) {
    static constexpr size_t MAX_INPUTS = 8;
    const size_t input_count = this->model_handler_.input_count();
    if (input_count > MAX_INPUTS) {
      ESP_LOGE(TAG, "Model has %zu inputs, more than the %zu a pipeline can take", input_count, MAX_INPUTS);
      return false;
    }
    TfLiteTensor *inputs[MAX_INPUTS];
    for (size_t i = 0; i < input_count; i++)
      inputs[i] = this->model_handler_.input(i);
    if (!this->input_pipeline_->fill_all(inputs, input_count, src_data, src_size))
      return false;
  } else {
    if (this->model_handler_.input_count() != 1) {
      ESP_LOGE(TAG, "A raw inference needs one input, this model has %zu", this->model_handler_.input_count());
      return false;
    }
    if (!this->validate_input_tensor_(src_data, src_size))
      return false;
    memcpy(this->model_handler_.input_tensor()->data.raw, src_data, src_size);
  }
  if (this->invoke_locked_() != kTfLiteOk) {
    ESP_LOGE(TAG, "Model invocation failed");
    return false;
  }
  return this->decode_outputs_locked_();
}

bool TFLiteMicroHelper::run_(const uint8_t *src_data, size_t src_size, int frame_width, int frame_height) {
  if (!this->prepare_run_())
    return false;
  bool succeeded = false;
  {
    LockGuard guard(this);
    if (!guard.locked())
      return false;
    if (frame_width > 0 && frame_height > 0 && this->input_pipeline_ != nullptr)
      this->input_pipeline_->set_frame_geometry(frame_width, frame_height);
    succeeded = this->run_locked_(src_data, src_size);
    if (succeeded)
      this->pending_epoch_.fetch_add(1, std::memory_order_release);
  }
  this->finish_run_(succeeded);
  return succeeded;
}

bool TFLiteMicroHelper::run(const uint8_t *src_data, size_t src_size) { return this->run_(src_data, src_size, 0, 0); }

bool TFLiteMicroHelper::run(const uint8_t *src_data, size_t src_size, int frame_width, int frame_height) {
  return this->run_(src_data, src_size, frame_width, frame_height);
}

optional<ProcessedOutput> TFLiteMicroHelper::run_inference(const uint8_t *src_data, size_t src_size) {
  if (!this->prepare_run_())
    return nullopt;
  optional<ProcessedOutput> processed;
  {
    LockGuard guard(this);
    if (!guard.locked())
      return nullopt;
    if (!this->run_inference_on_buffer_locked_(src_data, src_size)) {
      return nullopt;  // the reason is already logged
    }
    TfLiteTensor *output = this->model_handler_.output_tensor();
    if (output == nullptr) {
      ESP_LOGE(TAG, "Model has no output tensor after a successful invocation");
      return nullopt;
    }
    const ProcessedOutput result = this->model_handler_.process_output(output);
    if (!result.valid) {
      // The reason is already logged by the output processor.
      return nullopt;
    }
    this->last_result_ = result;
    // Keep the kind's decoder in step with the result its callback reads.
    this->decode_outputs_locked_();
    processed = result;
    this->pending_epoch_.fetch_add(1, std::memory_order_release);
  }
  this->finish_run_(true);
  return processed;
}

#ifdef USE_TFLITE_AUDIO
void TFLiteMicroHelper::set_probability_cutoff(float f) {
  if (std::isnan(f)) {
    ESP_LOGW(TAG, "probability_cutoff is NaN -- using 0.5");
    f = 0.5f;
  } else if (f < 0.0f || f > 1.0f) {
    ESP_LOGW(TAG, "probability_cutoff is outside 0-100%% -- clamping");
    f = std::clamp(f, 0.0f, 1.0f);
  }
  this->audio_config_.probability_cutoff = static_cast<uint8_t>(f * 255.0f);
}
void TFLiteMicroHelper::set_sliding_window_size(size_t n) { this->audio_config_.sliding_window_size = n; }
void TFLiteMicroHelper::set_features_step_size(uint8_t ms) { this->audio_config_.features_step_size = ms; }
void TFLiteMicroHelper::set_feature_count(size_t n) { this->audio_config_.feature_count = n; }
#endif

const LogString *TFLiteMicroHelper::check_model_integrity_() {
  if (this->model_data_ == nullptr || this->model_length_ == 0)
    return LOG_STR("Model data is NULL or empty");
  if (this->expected_crc32_ == 0) {
    ESP_LOGW(TAG, "No expected CRC32 configured -- skipping the integrity check");
    return nullptr;
  }
  if (!this->model_handler_.verify_model_crc(this->model_data_, this->model_length_, this->expected_crc32_))
    return LOG_STR("Model CRC32 verification failed");
  return nullptr;
}

bool TFLiteMicroHelper::load_model() {
  LockGuard guard(this);
  if (!guard.locked())
    return false;
  const LoadState state = this->state_.load(std::memory_order_relaxed);
  if (state == LoadState::LOAD_STATE_LOADING) {
    ESP_LOGE(TAG, "load_model called while another load is in progress");
    return false;
  }
  if (state == LoadState::LOAD_STATE_UNLOADING) {
    ESP_LOGE(TAG, "load_model called while the model is being unloaded");
    return false;
  }
  if (state == LoadState::LOAD_STATE_READY) {
    ESP_LOGE(TAG, "load_model called while a model is loaded -- call unload_model() first");
    return false;
  }
  this->state_.store(LoadState::LOAD_STATE_LOADING, std::memory_order_relaxed);
  ESP_LOGD(TAG, "Loading TFLite model");
  LoadStats stats;
  stats.load_start_ms = millis();
  if (const LogString *reason = this->check_model_integrity_(); reason != nullptr) {
    ESP_LOGE(TAG, "%s", LOG_STR_ARG(reason));
    stats.total_load_time_ms = millis() - stats.load_start_ms;
    this->last_load_stats_ = stats;
    this->state_.store(LoadState::LOAD_STATE_UNLOADED, std::memory_order_relaxed);
    return false;
  }
  // Probe the arena size; the result is kept apart from the configured value so a
  // reload asks for the same size and the report does not call it requested.
  this->tensor_arena_size_effective_ = this->tensor_arena_size_requested_;
  size_t probe_size =
      this->model_handler_.probe_arena_size(this->model_data_, this->model_length_, this->tensor_arena_size_requested_);
  if (probe_size == 0 && this->tensor_arena_size_requested_ > 1024) {
    // An overestimate can exceed the heap's largest block; retry smaller before giving up.
    probe_size = this->model_handler_.probe_arena_size(this->model_data_, this->model_length_,
                                                       this->tensor_arena_size_requested_ / 2);
  }
  if (probe_size == 0) {
    ESP_LOGW(TAG, "Arena probe failed, using configured size: %zu bytes", this->tensor_arena_size_requested_);
  } else {
    this->tensor_arena_size_effective_ = probe_size;
    if (probe_size < this->tensor_arena_size_requested_) {
      ESP_LOGW(TAG, "Arena probe: model needs %zu bytes, %zu bytes configured -- allocating %zu", probe_size,
               this->tensor_arena_size_requested_, probe_size);
    } else if (probe_size > this->tensor_arena_size_requested_) {
      ESP_LOGW(TAG, "Arena probe: model needs %zu bytes, more than the %zu bytes configured -- allocating %zu",
               probe_size, this->tensor_arena_size_requested_, probe_size);
    } else {
      ESP_LOGD(TAG, "Arena probe: %zu bytes needed, which is the configured size", probe_size);
    }
  }
  stats.parse_time_ms = millis() - stats.load_start_ms;
  uint32_t arena_alloc_start = millis();
  if (!this->allocate_tensor_arena_()) {
    stats.arena_alloc_time_ms = millis() - arena_alloc_start;
    stats.total_load_time_ms = millis() - stats.load_start_ms;
    this->last_load_stats_ = stats;
    this->state_.store(LoadState::LOAD_STATE_UNLOADED, std::memory_order_relaxed);
    return false;
  }
  stats.arena_alloc_time_ms = millis() - arena_alloc_start;
  // The arena exists from here on, so every failure releases it and leaves UNLOADED.
  auto fail_load = [&]() {
    this->model_handler_.unload();
    this->tensor_arena_allocation_.reset();
    stats.total_load_time_ms = millis() - stats.load_start_ms;
    this->last_load_stats_ = stats;
    this->state_.store(LoadState::LOAD_STATE_UNLOADED, std::memory_order_relaxed);
    return false;
  };
  ModelConfig config = this->build_config_();
  if (!this->model_handler_.load_model_with_arena(this->model_data_, this->model_length_,
                                                  this->tensor_arena_allocation_.data.get(),
                                                  this->tensor_arena_allocation_.actual_size, config)) {
    ESP_LOGE(TAG, "Failed to load model into interpreter");
    return fail_load();
  }

  stats.success = true;
  stats.total_load_time_ms = millis() - stats.load_start_ms;
  this->last_load_stats_ = stats;
  this->state_.store(LoadState::LOAD_STATE_READY, std::memory_order_relaxed);
  ESP_LOGD(TAG, "Model loaded successfully");
  if (this->debug_) {
    ESP_LOGI(TAG, "Load stats: parse=%" PRIu32 " ms, arena_alloc=%" PRIu32 " ms, total=%" PRIu32 " ms",
             stats.parse_time_ms, stats.arena_alloc_time_ms, stats.total_load_time_ms);
  }
  return true;
}

bool TFLiteMicroHelper::unload_model(bool reset_config) {
  LockGuard guard(this);
  if (!guard.locked())
    return false;
  // A load in progress owns the buffers, so only a completed load may unload.
  const LoadState state = this->state_.load(std::memory_order_relaxed);
  if (state == LoadState::LOAD_STATE_LOADING || state == LoadState::LOAD_STATE_UNLOADING) {
    ESP_LOGE(TAG, "unload_model() while a model is loading -- not unloading");
    return false;
  }
  if (state == LoadState::LOAD_STATE_UNLOADED) {
    ESP_LOGD(TAG, "unload_model() while no model is loaded");
    if (reset_config)
      this->reset_config();
    return true;
  }
  this->state_.store(LoadState::LOAD_STATE_UNLOADING, std::memory_order_relaxed);
  ESP_LOGD(TAG, "Unloading model and freeing arena");
  this->model_handler_.unload();
  // The arena is owned here; load_model() hands it to the loader, which never frees it.
  this->tensor_arena_allocation_.reset();
  // Config is kept by default so a same-model reload works without re-issuing setters.
  if (reset_config) {
    this->reset_config();
  }
  this->state_.store(LoadState::LOAD_STATE_UNLOADED, std::memory_order_relaxed);
  return true;
}

void TFLiteMicroHelper::reset_config() {
  ESP_LOGD(TAG, "Resetting model configuration to defaults");
  this->model_data_ = nullptr;
  this->model_length_ = 0;
  this->tensor_arena_size_requested_ = tflite_model_loader::ModelLoader::DEFAULT_TENSOR_ARENA_SIZE;
  this->tensor_arena_size_effective_ = 0;
  this->arena_heap_ = ArenaHeap::ARENA_HEAP_UNKNOWN;
  this->expected_crc32_ = 0;
  this->output_processing_ = StringRef("argmax");
  this->scale_factor_ = 1.0f;
#ifdef USE_TFLITE_AUDIO
  this->audio_config_ = AudioModelConfig{};
#endif
}

int TFLiteMicroHelper::get_input_width_locked_() const {
  return this->is_model_loaded() ? this->model_handler_.get_input_width() : 0;
}

int TFLiteMicroHelper::get_input_height_locked_() const {
  return this->is_model_loaded() ? this->model_handler_.get_input_height() : 0;
}

int TFLiteMicroHelper::get_input_channels_locked_() const {
  return this->is_model_loaded() ? this->model_handler_.get_input_channels() : 0;
}

int TFLiteMicroHelper::get_input_width() {
  LockGuard guard(this);
  return guard.locked() ? this->get_input_width_locked_() : 0;
}

int TFLiteMicroHelper::get_input_height() {
  LockGuard guard(this);
  return guard.locked() ? this->get_input_height_locked_() : 0;
}

int TFLiteMicroHelper::get_input_channels() {
  LockGuard guard(this);
  return guard.locked() ? this->get_input_channels_locked_() : 0;
}

ModelConfig TFLiteMicroHelper::get_config() {
  LockGuard guard(this);
  if (!guard.locked())
    return ModelConfig{};
  return this->model_handler_.get_config();
}

size_t TFLiteMicroHelper::get_arena_used_bytes() {
  LockGuard guard(this);
  return guard.locked() && this->is_model_loaded() ? this->model_handler_.get_arena_used_bytes() : 0;
}

size_t TFLiteMicroHelper::get_tensor_arena_size() {
  LockGuard guard(this);
  return guard.locked() ? this->tensor_arena_size_requested_ : 0;
}

size_t TFLiteMicroHelper::get_tensor_arena_size_effective() {
  LockGuard guard(this);
  return guard.locked() ? this->tensor_arena_size_effective_ : 0;
}

size_t TFLiteMicroHelper::get_tensor_arena_size_actual() {
  LockGuard guard(this);
  return guard.locked() ? this->tensor_arena_allocation_.actual_size : 0;
}

size_t TFLiteMicroHelper::get_model_size_bytes() {
  LockGuard guard(this);
  return guard.locked() ? this->model_length_ : 0;
}

LoadStats TFLiteMicroHelper::get_last_load_stats() {
  LockGuard guard(this);
  return guard.locked() ? this->last_load_stats_ : LoadStats{};
}

ProcessedOutput TFLiteMicroHelper::get_last_result() {
  LockGuard guard(this);
  return guard.locked() ? this->last_result_ : ProcessedOutput{};
}

ModelSpec TFLiteMicroHelper::get_model_spec() {
  ModelSpec spec;
  LockGuard guard(this);
  if (!guard.locked() || !this->is_model_loaded())
    return spec;
  spec.input_width = this->get_input_width_locked_();
  spec.input_height = this->get_input_height_locked_();
  spec.input_channels = this->get_input_channels_locked_();
  // From the tensor the model was converted with: a byte-count heuristic mis-reads
  // 3D/audio tensors, whose element count is not width*height*channels.
  const TfLiteTensor *input = this->model_handler_.input_tensor();
  if (input != nullptr)
    spec.input_type = tensor_type_to_input_type(input->type);
  return spec;
}

ArenaStats TFLiteMicroHelper::get_arena_stats() {
  ArenaStats stats;
  LockGuard guard(this);
  if (!guard.locked())
    return stats;
  stats.total_size = this->tensor_arena_allocation_.actual_size;
  stats.used_bytes = this->is_model_loaded() ? this->model_handler_.get_arena_used_bytes() : 0;
  stats.wasted_bytes = (stats.total_size > stats.used_bytes) ? (stats.total_size - stats.used_bytes) : 0;
  stats.efficiency = (stats.total_size > 0) ? (100.0f * stats.used_bytes / stats.total_size) : 0.0f;
  return stats;
}

void TFLiteMicroHelper::report_memory_status() {
  LockGuard guard(this);
  if (!guard.locked())
    return;
  MemoryManager::report_memory_status(this->tensor_arena_size_requested_, this->tensor_arena_allocation_.actual_size,
                                      this->is_model_loaded() ? this->model_handler_.get_arena_used_bytes() : 0,
                                      this->model_length_);
  // Measure the heap the arena came from (PSRAM vs internal), which is only known
  // once a load has allocated it.
  if (this->arena_heap_ == ArenaHeap::ARENA_HEAP_UNKNOWN) {
    ESP_LOGD(TAG, "Heap fragmentation not checked: no tensor arena has been allocated yet");
    return;
  }
  const bool from_psram = this->arena_heap_ == ArenaHeap::ARENA_HEAP_PSRAM;
  const uint32_t heap_caps = from_psram ? MALLOC_CAP_SPIRAM : MALLOC_CAP_INTERNAL;
  const size_t total_free = heap_caps_get_free_size(heap_caps);
  const size_t max_block = heap_caps_get_largest_free_block(heap_caps);
  ESP_LOGD(TAG, "Heap status - Total free: %zu bytes, Largest block: %zu bytes", total_free, max_block);
  // Compare against the size the last load actually used, which a reload allocates again.
  const size_t needed =
      this->tensor_arena_size_effective_ > 0 ? this->tensor_arena_size_effective_ : this->tensor_arena_size_requested_;
  if (max_block < needed) {
    ESP_LOGW(TAG, "WARNING: Largest free block (%zu) < arena size (%zu)", max_block, needed);
  }
}

TfLiteTensor *TFLiteMicroHelper::input_tensor() {
  LockGuard guard(this);
  return guard.locked() && this->is_model_loaded() ? this->model_handler_.input_tensor() : nullptr;
}

const TfLiteTensor *TFLiteMicroHelper::input_tensor() const {
  LockGuard guard(const_cast<TFLiteMicroHelper *>(this));
  return guard.locked() && this->is_model_loaded() ? this->model_handler_.input_tensor() : nullptr;
}

TfLiteTensor *TFLiteMicroHelper::output_tensor() {
  LockGuard guard(this);
  return guard.locked() && this->is_model_loaded() ? this->model_handler_.output_tensor() : nullptr;
}

TfLiteTensor *InferenceSession::input_tensor() {
  if (!this->locked_ || !this->helper_->is_model_loaded())
    return nullptr;
  return this->helper_->model_handler_.input_tensor();
}

const TfLiteTensor *InferenceSession::input_tensor() const {
  if (!this->locked_ || !this->helper_->is_model_loaded())
    return nullptr;
  return this->helper_->model_handler_.input_tensor();
}

TfLiteTensor *InferenceSession::output_tensor() {
  if (!this->locked_ || !this->helper_->is_model_loaded())
    return nullptr;
  return this->helper_->model_handler_.output_tensor();
}

TfLiteStatus InferenceSession::invoke() {
  if (!this->locked_)
    return kTfLiteError;
  return this->helper_->invoke_locked_();
}

}  // namespace esphome::tflite_micro_runtime

#endif  // USE_TFLITE_MICRO_HELPER
