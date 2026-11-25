#include "tflite_micro_runtime.h"

#ifdef USE_TFLITE_MICRO_HELPER
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cinttypes>
#include <cstdlib>
#include <cstring>
#include <esp_heap_caps.h>

namespace esphome::tflite_micro_runtime {

ESPHOME_LOG_TAG(TAG, "tflite_micro_runtime");

namespace {

const LogString *input_type_name(InputType type) {
  switch (type) {
    case InputType::INPUT_TYPE_UINT8:
      return LOG_STR("uint8");
    case InputType::INPUT_TYPE_INT8:
      return LOG_STR("int8");
    case InputType::INPUT_TYPE_FLOAT32:
      return LOG_STR("float32");
    case InputType::INPUT_TYPE_UNKNOWN:
      break;
  }
  return LOG_STR("unknown");
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

TFLiteMicroHelper::LockGuard::LockGuard(TFLiteMicroHelper *helper, bool wait) : helper_(helper) {
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
  if (wait) {
    this->helper_->mutex_.lock();
  } else if (!this->helper_->mutex_.try_lock()) {
    return;
  }
  this->helper_->lock_owner_.store(current, std::memory_order_relaxed);
  this->locked_ = true;
}

void TFLiteMicroHelper::LockGuard::unlock() {
  if (!this->locked_)
    return;
  this->helper_->reentry_reported_.store(false, std::memory_order_relaxed);
  this->helper_->lock_owner_.store(nullptr, std::memory_order_relaxed);
  this->helper_->mutex_.unlock();
  this->locked_ = false;
}

TFLiteMicroHelper::LockGuard::~LockGuard() { this->unlock(); }

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

void TFLiteMicroHelper::dump_config() {
  // Do not wait for the lock: an inference on another task holds it for its whole
  // run, and waiting here would stall the main loop.
  LockGuard guard(this, false);
  if (!guard.locked()) {
    ESP_LOGCONFIG(TAG, "TFLite Micro Runtime: busy, the details are unavailable while a model runs");
    return;
  }
  ESP_LOGCONFIG(TAG,
                "TFLite Micro Runtime:\n"
                "  Input pipeline: %s\n"
                "  Model size: %zu bytes\n"
                "  Tensor arena: %zu bytes configured",
                this->input_pipeline_ != nullptr ? this->input_pipeline_->name() : LOG_STR_ARG(LOG_STR_LITERAL("raw")),
                this->model_length_, this->tensor_arena_size_requested_);

  if (!this->is_model_loaded()) {
    ESP_LOGCONFIG(TAG, "  Model not loaded");
    return;
  }
  const TfLiteTensor *input = this->model_handler_.input_tensor();
  const InputType input_type =
      input != nullptr ? tensor_type_to_input_type(input->type) : InputType::INPUT_TYPE_UNKNOWN;
  ESP_LOGCONFIG(TAG,
                "  Input: %dx%dx%d %s\n"
                "  Tensor arena allocated: %zu bytes, %zu bytes requested after probing, %zu bytes in use",
                this->get_input_width_locked_(), this->get_input_height_locked_(), this->get_input_channels_locked_(),
                LOG_STR_ARG(input_type_name(input_type)), this->tensor_arena_allocation_.actual_size,
                this->tensor_arena_size_effective_, this->model_handler_.get_arena_used_bytes());
}

void TFLiteMicroHelper::set_tensor_arena_size(size_t size) {
  LockGuard guard(this);
  if (!guard.locked())
    return;
  this->tensor_arena_size_requested_ = size;
  // A different seed needs a fresh probe; the proven size no longer applies.
  this->arena_size_probed_ = 0;
}

bool TFLiteMicroHelper::set_model(const uint8_t *model_data, size_t model_size, uint32_t expected_crc32) {
  LockGuard guard(this);
  if (!guard.locked())
    return false;
  if (this->state_.load(std::memory_order_relaxed) == LoadState::LOAD_STATE_READY) {
    // The interpreter still uses the old buffer until unload_model() frees it.
    ESP_LOGE(TAG, "set_model() while a model is loaded -- call unload_model() first");
    return false;
  }
  this->model_data_ = model_data;
  this->model_length_ = model_size;
  this->expected_crc32_ = expected_crc32;
  // A different model needs its own probe.
  this->arena_size_probed_ = 0;
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

bool TFLiteMicroHelper::run_inference_on_buffer(const uint8_t *src_data, size_t src_size) {
  LockGuard guard(this);
  if (!guard.locked())
    return false;
  return this->run_inference_on_buffer_locked_(src_data, src_size);
}

bool TFLiteMicroHelper::fill_raw_input_locked_(const uint8_t *src_data, size_t src_size) {
  if (this->model_handler_.input_count() != 1) {
    ESP_LOGE(TAG, "A raw inference needs one input, this model has %zu", this->model_handler_.input_count());
    return false;
  }
  if (!this->validate_input_tensor_(src_data, src_size))
    return false;
  // validate_input_tensor_() checked type and size, so one raw copy serves every type.
  memcpy(this->model_handler_.input_tensor()->data.raw, src_data, src_size);
  return true;
}

bool TFLiteMicroHelper::run_inference_on_buffer_locked_(const uint8_t *src_data, size_t src_size) {
  if (!this->is_model_loaded()) {
    ESP_LOGE(TAG, "Cannot run inference -- model not loaded");
    return false;
  }
  if (!this->fill_raw_input_locked_(src_data, src_size))
    return false;
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
  if (state == LoadState::LOAD_STATE_READY) {
    ESP_LOGE(TAG, "load_model called while a model is loaded -- call unload_model() first");
    return false;
  }
  ESP_LOGD(TAG, "Loading TFLite model");
  LoadStats stats;
  stats.load_start_ms = millis();
  if (const LogString *reason = this->check_model_integrity_(); reason != nullptr) {
    ESP_LOGE(TAG, "%s", LOG_STR_ARG(reason));
    stats.total_load_time_ms = millis() - stats.load_start_ms;
    this->last_load_stats_ = stats;
    return false;
  }
  // Reuse the size a previous load proved, or skip probing for a stateful model:
  // its resource variables must not be shared with temporary probe interpreters,
  // and the consumer sizes its own arena.
  this->tensor_arena_size_effective_ = this->tensor_arena_size_requested_;
  if (this->resource_variables_ != nullptr) {
    ESP_LOGD(TAG, "Stateful model: using the configured arena size %zu bytes", this->tensor_arena_size_requested_);
  } else if (this->arena_size_probed_ > 0) {
    this->tensor_arena_size_effective_ = this->arena_size_probed_;
    ESP_LOGD(TAG, "Reusing the proven arena size: %zu bytes", this->arena_size_probed_);
  } else {
    size_t probe_size = this->model_handler_.probe_arena_size(
        this->model_data_, this->model_length_, this->tensor_arena_size_requested_, this->resource_variables_);
    if (probe_size == 0 && this->tensor_arena_size_requested_ > 1024) {
      // An overestimate can exceed the heap's largest block; retry smaller before giving up.
      probe_size = this->model_handler_.probe_arena_size(
          this->model_data_, this->model_length_, this->tensor_arena_size_requested_ / 2, this->resource_variables_);
    }
    if (probe_size == 0) {
      ESP_LOGW(TAG, "Arena probe failed, using configured size: %zu bytes", this->tensor_arena_size_requested_);
    } else {
      this->tensor_arena_size_effective_ = probe_size;
      this->arena_size_probed_ = probe_size;
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
  }
  stats.parse_time_ms = millis() - stats.load_start_ms;
  uint32_t arena_alloc_start = millis();
  if (!this->allocate_tensor_arena_()) {
    stats.arena_alloc_time_ms = millis() - arena_alloc_start;
    stats.total_load_time_ms = millis() - stats.load_start_ms;
    this->last_load_stats_ = stats;
    return false;
  }
  stats.arena_alloc_time_ms = millis() - arena_alloc_start;
  // The arena exists from here on, so every failure releases it.
  auto fail_load = [&]() {
    this->model_handler_.unload();
    this->tensor_arena_allocation_.reset();
    stats.total_load_time_ms = millis() - stats.load_start_ms;
    this->last_load_stats_ = stats;
    return false;
  };
  ModelConfig config = this->build_config_();
  if (!this->model_handler_.load_model_with_arena(
          this->model_data_, this->model_length_, this->tensor_arena_allocation_.data.get(),
          this->tensor_arena_allocation_.actual_size, config, this->resource_variables_)) {
    ESP_LOGE(TAG, "Failed to load model into interpreter");
    return fail_load();
  }

  stats.success = true;
  stats.total_load_time_ms = millis() - stats.load_start_ms;
  this->last_load_stats_ = stats;
  // Release: a lock-free is_model_loaded() that sees READY also sees the loaded interpreter.
  this->state_.store(LoadState::LOAD_STATE_READY, std::memory_order_release);
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
  // The lifecycle mutex excludes a load or unload running at the same time, so
  // the state is either READY or UNLOADED here.
  if (this->state_.load(std::memory_order_relaxed) == LoadState::LOAD_STATE_UNLOADED) {
    ESP_LOGD(TAG, "unload_model() while no model is loaded");
    if (reset_config)
      this->reset_config();
    return true;
  }
  // Clear READY first, so a lock-free is_model_loaded() stops reporting the model
  // before its interpreter and arena are released.
  this->state_.store(LoadState::LOAD_STATE_UNLOADED, std::memory_order_release);
  ESP_LOGD(TAG, "Unloading model and freeing arena");
  this->model_handler_.unload();
  // The arena is owned here; load_model() hands it to the loader, which never frees it.
  this->tensor_arena_allocation_.reset();
  // Config is kept by default so a same-model reload works without re-issuing setters.
  if (reset_config) {
    this->reset_config();
  }
  return true;
}

void TFLiteMicroHelper::reset_config() {
  ESP_LOGD(TAG, "Resetting model configuration to defaults");
  this->model_data_ = nullptr;
  this->model_length_ = 0;
  this->tensor_arena_size_requested_ = tflite_model_loader::ModelLoader::DEFAULT_TENSOR_ARENA_SIZE;
  this->tensor_arena_size_effective_ = 0;
  this->arena_size_probed_ = 0;
  this->arena_heap_ = ArenaHeap::ARENA_HEAP_UNKNOWN;
  this->expected_crc32_ = 0;
  this->output_processing_ = StringRef("argmax");
  this->scale_factor_ = 1.0f;
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

}  // namespace esphome::tflite_micro_runtime

#endif  // USE_TFLITE_MICRO_HELPER
