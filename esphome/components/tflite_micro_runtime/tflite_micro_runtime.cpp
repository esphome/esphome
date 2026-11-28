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

/// Output types process_output() can turn into a class and a confidence.
bool has_classification_mapping(const TfLiteTensor *output) {
  return output->type == kTfLiteFloat32 || output->type == kTfLiteUInt8 || output->type == kTfLiteInt8;
}

/// One lock for every runtime: the optimized ESP-NN kernels keep their scratch
/// buffers in globals, so two models must not invoke at the same time, whatever
/// their own locks allow. A function static, so it exists before its first use.
Mutex &shared_invoke_mutex() {
  static Mutex mutex;
  return mutex;
}

/// Task holding shared_invoke_mutex(), which is not recursive.
std::atomic<TaskHandle_t> shared_invoke_owner{nullptr};

}  // namespace

TFLiteMicroHelper::LockGuard::LockGuard(const TFLiteMicroHelper *helper, bool wait) : helper_(helper) {
  if (this->helper_ == nullptr)
    return;
  const TaskHandle_t current = xTaskGetCurrentTaskHandle();
  if (this->helper_->lock_owner_.load(std::memory_order_relaxed) == current) {
    // A log emitted under the lock can run logger.on_message automations that call
    // back into the helper; blocking here would deadlock the main task.
    if (!this->helper_->reentry_reported_.exchange(true, std::memory_order_relaxed)) {
      ESP_LOGE(TAG, "A log callback re-entered the runtime; rejecting the nested call");
    }
    return;
  }
  // A task inside invoke() holds shared_invoke_mutex(), and the owner of this
  // lock may be waiting for that one: a log callback reaching another model must
  // not block here.
  if (wait && shared_invoke_owner.load(std::memory_order_relaxed) != current) {
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

void TFLiteMicroHelper::loop() {
  if (this->pending_epoch_.load(std::memory_order_acquire) != this->dispatched_epoch_.load(std::memory_order_acquire))
    this->dispatch_result_();
  // Nothing left to deliver: finish_run_() re-enables the loop for the next result.
  if (this->pending_epoch_.load(std::memory_order_acquire) == this->dispatched_epoch_.load(std::memory_order_acquire))
    this->disable_loop();
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

bool TFLiteMicroHelper::reserve_result_locked_() {
  if (this->result_callback_.empty())
    return true;  // no subscriber, so there is no notification to protect
  if (this->dispatching_.load(std::memory_order_acquire)) {
    ESP_LOGD(TAG, "A result callback is running; start the next inference after it returns");
    return false;
  }
  if (this->result_reserved_) {
    // Routine when frames arrive faster than the callbacks finish, so not a warning.
    ESP_LOGD(TAG, "The previous result has not been delivered yet; skipping this inference");
    return false;
  }
  this->result_reserved_ = true;
  return true;
}

bool TFLiteMicroHelper::result_blocks_inference_locked_() const {
  return !this->result_callback_.empty() &&
         (this->result_reserved_ || this->dispatching_.load(std::memory_order_acquire));
}

void TFLiteMicroHelper::dispatch_result_() {
  // A run started from a callback would overwrite the result the remaining
  // callbacks are reading, and would recurse here until the stack overflows.
  if (this->dispatching_.load(std::memory_order_acquire))
    return;
  // Reached from a log emitted during an invocation: the lock taken after the
  // callbacks might be refused, which would deliver the result twice. loop() does it.
  if (shared_invoke_owner.load(std::memory_order_relaxed) == xTaskGetCurrentTaskHandle())
    return;
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
  LockGuard guard(this);
  if (!guard.locked())
    return;
  // Only the main task dispatches, so this is the last delivered epoch even when
  // the counter wraps; no ordering comparison is needed.
  this->dispatched_epoch_.store(epoch, std::memory_order_release);
  if (this->pending_epoch_.load(std::memory_order_acquire) == epoch)
    this->result_reserved_ = false;
}

void TFLiteMicroHelper::finish_run_(bool succeeded) {
  if (!succeeded || this->result_callback_.empty())
    return;
  if (is_main_loop_thread()) {
    this->dispatch_result_();
  } else {
    // The result waits for the main loop, which may be on a long interval.
    this->enable_loop_soon_any_context();
  }
}

bool TFLiteMicroHelper::run_inference_on_buffer(const uint8_t *src_data, size_t src_size) {
  LockGuard guard(this);
  if (!guard.locked())
    return false;
  if (this->result_blocks_inference_locked_()) {
    ESP_LOGW(TAG, "A result is waiting for its callback; start the next inference after it returns");
    return false;
  }
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
  if (this->invoke_locked_() != kTfLiteOk) {
    ESP_LOGE(TAG, "Model invocation failed");
    return false;
  }
  return true;
}

TfLiteStatus TFLiteMicroHelper::invoke() {
  LockGuard guard(this);
  if (!guard.locked())
    return kTfLiteError;
  if (this->result_blocks_inference_locked_()) {
    ESP_LOGW(TAG, "A result is waiting for its callback; start the next inference after it returns");
    return kTfLiteError;
  }
  return this->invoke_locked_();
}

TfLiteStatus TFLiteMicroHelper::invoke_locked_() {
  if (!this->is_model_loaded()) {
    ESP_LOGE(TAG, "Cannot invoke -- model not loaded");
    return kTfLiteError;
  }
  const TaskHandle_t current = xTaskGetCurrentTaskHandle();
  if (shared_invoke_owner.load(std::memory_order_relaxed) == current) {
    // A log emitted by a kernel can run an automation that invokes another model.
    ESP_LOGE(TAG, "Cannot invoke a model while this task is invoking another one");
    return kTfLiteError;
  }
  esphome::LockGuard guard(shared_invoke_mutex());
  shared_invoke_owner.store(current, std::memory_order_relaxed);
  const TfLiteStatus status = this->model_handler_.invoke();
  shared_invoke_owner.store(nullptr, std::memory_order_relaxed);
  return status;
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
  if (this->output_decoder_->decode(outputs, count))
    return true;
  // A decoder need not log, and without this a failed run would only look like a missing notification.
  ESP_LOGE(TAG, "The output decoder could not decode the model outputs");
  return false;
}

bool TFLiteMicroHelper::run_locked_(const uint8_t *src_data, size_t src_size) {
  if (!this->is_model_loaded()) {
    ESP_LOGE(TAG, "Cannot run inference -- model not loaded");
    return false;
  }
  if (this->input_pipeline_ != nullptr) {
    // Checked here, so a pipeline never has to.
    if (src_data == nullptr) {
      ESP_LOGE(TAG, "Null input data pointer");
      return false;
    }
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
    if (!this->fill_raw_input_locked_(src_data, src_size))
      return false;
  }
  if (this->invoke_locked_() != kTfLiteOk) {
    ESP_LOGE(TAG, "Model invocation failed");
    return false;
  }
  // A result run_inference() cached belongs to an earlier invocation.
  this->last_result_ = ProcessedOutput{0.0f, 0.0f, false};
  if (this->output_decoder_ != nullptr)
    return this->decode_outputs_locked_();
  // A raw model has no decoder, so store the processed result for get_last_result().
  // A successful invocation is still notified when its output has no
  // classification mapping; the stored result is then marked invalid.
  if (!this->result_callback_.empty()) {
    TfLiteTensor *output = this->model_handler_.output_tensor();
    if (output == nullptr) {
      ESP_LOGW(TAG, "No output tensor; get_last_result() is invalid for this run");
    } else if (has_classification_mapping(output)) {
      this->last_result_ = this->model_handler_.process_output(output);
    } else {
      // The processor would log an error on every run for a type it cannot classify.
      ESP_LOGV(TAG, "The output type has no classification mapping; get_last_result() is invalid");
    }
  }
  return true;
}

bool TFLiteMicroHelper::run_(const uint8_t *src_data, size_t src_size, int frame_width, int frame_height) {
  // Deliver a pending result first, so its callbacks read it before this run decodes.
  if (is_main_loop_thread())
    this->dispatch_result_();
  bool succeeded = false;
  {
    LockGuard guard(this);
    if (!guard.locked())
      return false;
    if (!this->reserve_result_locked_())
      return false;
    // The frame size applies to this run only, so the configured one is put back.
    const bool resized = frame_width > 0 && frame_height > 0 && this->input_pipeline_ != nullptr;
    int configured_width = 0;
    int configured_height = 0;
    if (resized) {
      this->input_pipeline_->get_frame_geometry(&configured_width, &configured_height);
      this->input_pipeline_->set_frame_geometry(frame_width, frame_height);
    }
    succeeded = this->run_locked_(src_data, src_size);
    if (resized)
      this->input_pipeline_->set_frame_geometry(configured_width, configured_height);
    if (succeeded && !this->result_callback_.empty())
      this->pending_epoch_.fetch_add(1, std::memory_order_release);
    else if (!succeeded)
      this->result_reserved_ = false;
  }
  this->finish_run_(succeeded);
  return succeeded;
}

bool TFLiteMicroHelper::run(const uint8_t *src_data, size_t src_size) { return this->run_(src_data, src_size, 0, 0); }

bool TFLiteMicroHelper::run(const uint8_t *src_data, size_t src_size, int frame_width, int frame_height) {
  return this->run_(src_data, src_size, frame_width, frame_height);
}

optional<ProcessedOutput> TFLiteMicroHelper::run_inference(const uint8_t *src_data, size_t src_size) {
  // Deliver a pending result first, so its callbacks read it before this run decodes.
  if (is_main_loop_thread())
    this->dispatch_result_();
  optional<ProcessedOutput> processed;
  {
    LockGuard guard(this);
    if (!guard.locked())
      return nullopt;
    if (!this->reserve_result_locked_())
      return nullopt;
    if (!this->run_inference_on_buffer_locked_(src_data, src_size)) {
      this->result_reserved_ = false;
      return nullopt;  // the reason is already logged
    }
    TfLiteTensor *output = this->model_handler_.output_tensor();
    if (output == nullptr) {
      ESP_LOGE(TAG, "Model has no output tensor after a successful invocation");
      this->result_reserved_ = false;
      return nullopt;
    }
    // An output type without a classification mapping is still a successful run, with an invalid result.
    ProcessedOutput result{0.0f, 0.0f, false};
    if (has_classification_mapping(output)) {
      result = this->model_handler_.process_output(output);
      if (!result.valid) {
        // The reason is already logged by the output processor.
        this->result_reserved_ = false;
        return nullopt;
      }
    } else {
      ESP_LOGV(TAG, "The output type has no classification mapping; the result is invalid");
    }
    // Keep the kind's decoder in step with the result its callback reads; a
    // decoder failure is this inference's failure, not a stale notification.
    if (!this->decode_outputs_locked_()) {
      this->result_reserved_ = false;
      return nullopt;
    }
    this->last_result_ = result;
    processed = result;
    if (!this->result_callback_.empty())
      this->pending_epoch_.fetch_add(1, std::memory_order_release);
  }
  this->finish_run_(true);
  return processed;
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
  // A queued result, or a callback already reading the output tensors, needs the
  // interpreter and its arena to stay alive until the callback returns.
  if (this->result_blocks_inference_locked_()) {
    ESP_LOGW(TAG, "A result callback is pending; call unload_model() after it returns");
    return false;
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

#ifdef USE_TFLITE_MODEL_DEBUG
void TFLiteMicroHelper::debug_test_parameters(const std::vector<std::vector<uint8_t>> &zone_data) {
  LockGuard guard(this);
  if (!guard.locked())
    return;
  // The sweep fills the input and invokes the interpreter repeatedly; a queued
  // result or a running callback reads the tensors it would overwrite.
  if (this->result_blocks_inference_locked_()) {
    ESP_LOGW(TAG, "A result callback is pending; skipping the parameter sweep");
    return;
  }
  this->model_handler_.debug_test_parameters(zone_data);
}
#endif

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

int TFLiteMicroHelper::get_input_width() const {
  LockGuard guard(this);
  return guard.locked() ? this->get_input_width_locked_() : 0;
}

int TFLiteMicroHelper::get_input_height() const {
  LockGuard guard(this);
  return guard.locked() ? this->get_input_height_locked_() : 0;
}

int TFLiteMicroHelper::get_input_channels() const {
  LockGuard guard(this);
  return guard.locked() ? this->get_input_channels_locked_() : 0;
}

ModelConfig TFLiteMicroHelper::get_config() const {
  LockGuard guard(this);
  if (!guard.locked())
    return ModelConfig{};
  return this->model_handler_.get_config();
}

size_t TFLiteMicroHelper::get_arena_used_bytes() const {
  LockGuard guard(this);
  return guard.locked() && this->is_model_loaded() ? this->model_handler_.get_arena_used_bytes() : 0;
}

size_t TFLiteMicroHelper::get_tensor_arena_size() const {
  LockGuard guard(this);
  return guard.locked() ? this->tensor_arena_size_requested_ : 0;
}

size_t TFLiteMicroHelper::get_tensor_arena_size_effective() const {
  LockGuard guard(this);
  return guard.locked() ? this->tensor_arena_size_effective_ : 0;
}

size_t TFLiteMicroHelper::get_tensor_arena_size_actual() const {
  LockGuard guard(this);
  return guard.locked() ? this->tensor_arena_allocation_.actual_size : 0;
}

size_t TFLiteMicroHelper::get_model_size_bytes() const {
  LockGuard guard(this);
  return guard.locked() ? this->model_length_ : 0;
}

LoadStats TFLiteMicroHelper::get_last_load_stats() const {
  LockGuard guard(this);
  return guard.locked() ? this->last_load_stats_ : LoadStats{};
}

ProcessedOutput TFLiteMicroHelper::get_last_result() const {
  LockGuard guard(this);
  return guard.locked() ? this->last_result_ : ProcessedOutput{0.0f, 0.0f, false};
}

ModelSpec TFLiteMicroHelper::get_model_spec() const {
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

ArenaStats TFLiteMicroHelper::get_arena_stats() const {
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
