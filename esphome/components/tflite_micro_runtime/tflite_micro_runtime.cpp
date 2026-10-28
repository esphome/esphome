#include "tflite_micro_runtime.h"

#ifdef USE_TFLITE_MICRO_HELPER
#include "esphome/core/application.h"
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

/// Name a model type is logged and reported under. Points at a string literal.
const char *model_type_name(ModelType type) {
  switch (type) {
    case ModelType::MODEL_TYPE_IMAGE:
      return "image";
    case ModelType::MODEL_TYPE_AUDIO:
      return "audio";
  }
  return "unknown";
}

/// Name an input type is logged under. Points at a string literal.
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

/// Name a channel order is logged under. Points at a string literal.
const char *input_order_name(InputOrder order) {
  switch (order) {
    case InputOrder::INPUT_ORDER_RGB:
      return "RGB";
    case InputOrder::INPUT_ORDER_BGR:
      return "BGR";
    case InputOrder::INPUT_ORDER_GRAY:
      return "GRAY";
  }
  return "unknown";
}

/// The tensor type a configured input type denotes.
TfLiteType configured_input_tensor_type(InputType type) {
  switch (type) {
    case InputType::INPUT_TYPE_UINT8:
      return kTfLiteUInt8;
    case InputType::INPUT_TYPE_INT8:
      return kTfLiteInt8;
    case InputType::INPUT_TYPE_FLOAT32:
      return kTfLiteFloat32;
    case InputType::INPUT_TYPE_UNKNOWN:
      break;
  }
  return kTfLiteNoType;
}

/// The configured input type a model's input tensor type denotes.
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

const LogString *tensor_type_name(TfLiteType type) {
  switch (type) {
    case kTfLiteUInt8:
      return LOG_STR("uint8");
    case kTfLiteInt8:
      return LOG_STR("int8");
    case kTfLiteFloat32:
      return LOG_STR("float32");
    default:
      return LOG_STR("unknown");
  }
}

}  // namespace

void TFLiteMicroHelper::setup() {
  if (this->model_type_ == ModelType::MODEL_TYPE_AUDIO) {
    // A streaming model needs MicroResourceVariables when the interpreter is built,
    // and ModelHandler::load_model() cannot pass them: model_loader.cpp calls
    // load_model_with_arena() with a null mrv. Loading here would therefore fail on
    // every boot for a model only the streaming consumer can load.
    //
    // The configured checksum is still checked here: nothing else reads it for this
    // model, so a corrupted model in flash would otherwise reach the consumer
    // unnoticed.
    if (const LogString *reason = this->check_model_integrity_(); reason != nullptr) {
      this->mark_failed(reason);
      return;
    }
    ESP_LOGI(TAG, "Audio model: load left to the streaming consumer");
    return;
  }
  // The helper is usable without a consumer: load here so a configuration whose
  // model has no other user still verifies the model, sizes the arena and reports
  // it at boot instead of only embedding it.
  if (!this->load_model())
    this->mark_failed(LOG_STR("Model load failed"));
}

void TFLiteMicroHelper::dump_config() {
  ESP_LOGCONFIG(TAG, "TFLite Micro Runtime:");
  ESP_LOGCONFIG(TAG, "  Model type: %s", model_type_name(this->model_type_));
  ESP_LOGCONFIG(TAG, "  Model size: %zu bytes", this->model_length_);
  ESP_LOGCONFIG(TAG, "  Tensor arena: %zu bytes configured", this->tensor_arena_size_requested_);
  if (this->model_type_ == ModelType::MODEL_TYPE_AUDIO) {
    // setup() leaves this model to the streaming consumer, so there is no arena and
    // no input tensor to report. The parameters that consumer reads are.
    ESP_LOGCONFIG(TAG, "  Load left to the streaming consumer");
    ESP_LOGCONFIG(TAG, "  Probability cutoff: %u/255, sliding window: %zu frames",
                  static_cast<unsigned>(this->audio_config_.probability_cutoff),
                  this->audio_config_.sliding_window_size);
    ESP_LOGCONFIG(TAG, "  Features step: %u ms, feature count: %zu",
                  static_cast<unsigned>(this->audio_config_.features_step_size), this->audio_config_.feature_count);
    return;
  }
  if (!this->is_model_loaded()) {
    ESP_LOGCONFIG(TAG, "  Model not loaded");
    return;
  }
  const ModelSpec spec = this->get_model_spec();
  ESP_LOGCONFIG(TAG, "  Input: %dx%dx%d %s (%s)", spec.input_width, spec.input_height, spec.input_channels,
                input_type_name(this->input_type_), input_order_name(this->input_order_));
  ESP_LOGCONFIG(TAG, "  Tensor arena allocated: %zu bytes, %zu bytes requested after probing, %zu bytes in use",
                this->get_tensor_arena_size_actual(), this->tensor_arena_size_effective_, this->get_arena_used_bytes());
}

void TFLiteMicroHelper::set_tensor_arena_size(size_t size) { this->tensor_arena_size_requested_ = size; }

bool TFLiteMicroHelper::set_model(const uint8_t *model_data, size_t model_size) {
  if (this->state_.load() != LoadState::UNLOADED) {
    // READY: the interpreter still uses the old buffer, so a swap would leave the
    // load silently not happening. LOADING: load_model() is reading it right now.
    ESP_LOGE(TAG, "set_model() while a model is loaded or loading -- call unload_model() first");
    return false;
  }
  this->model_data_ = model_data;
  this->model_length_ = model_size;
  return true;
}

bool TFLiteMicroHelper::allocate_tensor_arena_() {
  ESP_LOGI(TAG, "Allocating tensor arena: %zu bytes", this->tensor_arena_size_effective_);
  this->tensor_arena_allocation_ = MemoryManager::allocate_tensor_arena(this->tensor_arena_size_effective_);
  if (!this->tensor_arena_allocation_) {
    // Nothing is allocated, so a report must not describe the heap the previous
    // model came from.
    this->arena_heap_ = ArenaHeap::ARENA_HEAP_UNKNOWN;
    ESP_LOGE(TAG, "Failed to allocate tensor arena");
    return false;
  }
  // Cached here: report_memory_status() runs after an unload too, when the
  // allocation no longer says which heap the model had to share.
  this->arena_heap_ =
      this->tensor_arena_allocation_.from_psram ? ArenaHeap::ARENA_HEAP_PSRAM : ArenaHeap::ARENA_HEAP_INTERNAL;
  ESP_LOGI(TAG, "Tensor arena allocated: %zu bytes", this->tensor_arena_allocation_.actual_size);
  return true;
}

ModelConfig TFLiteMicroHelper::build_config_() {
  ModelConfig config;
  config.description = model_type_name(this->model_type_);
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
  // data is a union, and validate_input_tensor_() checked both the type and the
  // size, so one copy of the raw bytes is all any input type needs.
  memcpy(input->data.raw, src_data, src_size);
  if (this->model_handler_.invoke() != kTfLiteOk) {
    ESP_LOGE(TAG, "Model invocation failed");
    return false;
  }
  return true;
}

TfLiteStatus TFLiteMicroHelper::invoke() {
  if (!this->is_model_loaded()) {
    ESP_LOGE(TAG, "Cannot invoke -- model not loaded");
    return kTfLiteError;
  }
  return this->model_handler_.invoke();
}

optional<ProcessedOutput> TFLiteMicroHelper::run_inference(const uint8_t *src_data, size_t src_size) {
  if (!this->run_inference_on_buffer(src_data, src_size)) {
    // The reason is already logged. Returning a default ProcessedOutput here would
    // look like a real class-0 result with zero confidence.
    return nullopt;
  }
  TfLiteTensor *output = this->model_handler_.output_tensor();
  if (output == nullptr) {
    ESP_LOGE(TAG, "Model has no output tensor after a successful invocation");
    return nullopt;
  }
  return this->model_handler_.process_output(output);
}

// Image model setters
void TFLiteMicroHelper::set_input_channels(int c) { this->input_channels_ = c; }
void TFLiteMicroHelper::set_input_width(int w) { this->input_width_ = w; }
void TFLiteMicroHelper::set_input_height(int h) { this->input_height_ = h; }
void TFLiteMicroHelper::set_normalize(bool n) { this->normalize_ = n; }
void TFLiteMicroHelper::set_invert(bool i) { this->invert_ = i; }

// Audio model setters.
// NOTE: TFLiteMicroHelper is *configuration-only* for audio. The setters below
// populate audio_config_ as a pass-through so consumers can read the validated
// values, but the streaming runtime (MRV, sliding window, stride management) is
// owned by the consumer (e.g. micro_wake_word::StreamingModel).
void TFLiteMicroHelper::set_probability_cutoff(float f) {
  if (std::isnan(f)) {
    ESP_LOGW(TAG, "probability_cutoff is NaN -- using 0.5");
    f = 0.5f;
  } else if (f < 0.0f || f > 1.0f) {
    // Clamping silently would accept a value the caller got wrong, so say so.
    ESP_LOGW(TAG, "probability_cutoff %d%% is outside 0-100%% -- clamping", static_cast<int>(f * 100.0f));
    f = std::clamp(f, 0.0f, 1.0f);
  }
  this->audio_config_.probability_cutoff = static_cast<uint8_t>(f * 255.0f);
}
void TFLiteMicroHelper::set_sliding_window_size(size_t n) { this->audio_config_.sliding_window_size = n; }
void TFLiteMicroHelper::set_features_step_size(uint8_t ms) { this->audio_config_.features_step_size = ms; }
void TFLiteMicroHelper::set_feature_count(size_t n) { this->audio_config_.feature_count = n; }

const LogString *TFLiteMicroHelper::check_model_integrity_() {
  if (this->model_data_ == nullptr || this->model_length_ == 0)
    return LOG_STR("Model data is NULL or empty");
  if (this->expected_crc32_ == 0) {
    // 0 means "no checksum configured", which is what reset_config() leaves
    // behind. Skipping the check is legitimate, but it must be visible. Returning
    // here keeps the skip independent of how the loader's verify_model_crc()
    // treats 0, and keeps its own skip log out of the output.
    ESP_LOGW(TAG, "No expected CRC32 configured -- skipping the integrity check");
    return nullptr;
  }
  if (!this->model_handler_.verify_model_crc(this->model_data_, this->model_length_, this->expected_crc32_))
    return LOG_STR("Model CRC32 verification failed");
  return nullptr;
}

bool TFLiteMicroHelper::load_model() {
  if (this->model_type_ == ModelType::MODEL_TYPE_AUDIO) {
    // A streaming model needs MicroResourceVariables when its interpreter is
    // built, and only load_model_with_arena() can pass them. Failing here says so
    // instead of leaving the generic interpreter error from the load below.
    ESP_LOGE(TAG, "load_model() cannot load an audio model; its streaming consumer must call load_model_with_arena()");
    return false;
  }
  // Single atomic state machine: reject concurrent loads, and only go from LOADING
  // to READY at the very end, on success.
  LoadState expected = LoadState::UNLOADED;
  if (!this->state_.compare_exchange_strong(expected, LoadState::LOADING)) {
    if (expected == LoadState::READY) {
      // Returning true here reported success without loading anything, so a
      // caller that swapped set_model() first kept the old model and never
      // learned about it.
      ESP_LOGE(TAG, "load_model called while a model is loaded -- call unload_model() first");
      return false;
    }
    ESP_LOGE(TAG, "load_model called while another load is in progress");
    return false;
  }
  ESP_LOGI(TAG, "Loading TFLite model...");

  LoadStats stats;
  stats.load_start_ms = millis();

  if (const LogString *reason = this->check_model_integrity_(); reason != nullptr) {
    ESP_LOGE(TAG, "%s", LOG_STR_ARG(reason));
    stats.total_load_time_ms = millis() - stats.load_start_ms;
    this->last_load_stats_ = stats;
    this->state_.store(LoadState::UNLOADED);
    return false;
  }

  // Probe the actual required arena size (PSRAM-aware, replacing the old 1.5x
  // ESP32-S3 cache bump). The result is kept separately from the configured size:
  // overwriting the configured value would lose the user's choice for the next
  // load and report the probed size as if it had been requested.
  this->tensor_arena_size_effective_ = this->tensor_arena_size_requested_;
  const size_t probe_size =
      this->model_handler_.probe_arena_size(this->model_data_, this->tensor_arena_size_requested_);
  if (probe_size == 0) {
    ESP_LOGW(TAG, "Arena probe failed, using configured size: %zu bytes", this->tensor_arena_size_requested_);
  } else {
    this->tensor_arena_size_effective_ = probe_size;
    if (probe_size < this->tensor_arena_size_requested_) {
      // The model fits in less than was configured, so the arena is allocated at the
      // probed size. Warned rather than logged at INFO: the configured size is an
      // explicit setting and this load allocates less than it.
      ESP_LOGW(TAG, "Arena probe: model needs %zu bytes, %zu bytes configured -- allocating %zu", probe_size,
               this->tensor_arena_size_requested_, probe_size);
    } else if (probe_size > this->tensor_arena_size_requested_) {
      // The probe raises the size (1.5x, then 2x) so that a model which does not fit
      // the configured arena still loads.
      ESP_LOGW(TAG, "Arena probe: model needs %zu bytes, more than the %zu bytes configured -- allocating %zu",
               probe_size, this->tensor_arena_size_requested_, probe_size);
    } else {
      ESP_LOGI(TAG, "Arena probe: %zu bytes needed, which is the configured size", probe_size);
    }
  }
  stats.parse_time_ms = millis() - stats.load_start_ms;

  uint32_t arena_alloc_start = millis();
  if (!this->allocate_tensor_arena_()) {
    stats.arena_alloc_time_ms = millis() - arena_alloc_start;
    stats.total_load_time_ms = millis() - stats.load_start_ms;
    this->last_load_stats_ = stats;
    this->state_.store(LoadState::UNLOADED);
    return false;
  }
  stats.arena_alloc_time_ms = millis() - arena_alloc_start;

  // From here on the arena exists, so every failure has to release it and leave the
  // helper UNLOADED; otherwise the next load_model() starts on a stale arena.
  auto fail_load = [&]() {
    this->model_handler_.unload();
    this->tensor_arena_allocation_.reset();
    stats.total_load_time_ms = millis() - stats.load_start_ms;
    this->last_load_stats_ = stats;
    this->state_.store(LoadState::UNLOADED);
    return false;
  };

  ModelConfig config = this->build_config_();
  if (!this->model_handler_.load_model_with_arena(this->model_data_, this->model_length_,
                                                  this->tensor_arena_allocation_.data.get(),
                                                  this->tensor_arena_allocation_.actual_size, config)) {
    ESP_LOGE(TAG, "Failed to load model into interpreter");
    return fail_load();
  }

  // Check that the configured input matches the tensor the model was converted
  // with: a mismatch would feed the model a differently shaped buffer and only
  // show up as bad predictions.
  if (this->model_type_ == ModelType::MODEL_TYPE_IMAGE) {
    ImageModelConfig img_cfg;
    img_cfg.input_type = input_type_name(this->input_type_);
    img_cfg.input_channels = this->input_channels_;
    img_cfg.input_order = input_order_name(this->input_order_);
    img_cfg.input_size = {this->input_width_, this->input_height_};
    img_cfg.normalize = this->normalize_;
    img_cfg.invert = this->invert_;
    this->model_handler_.set_image_config(img_cfg);

    const int model_width = this->model_handler_.get_input_width();
    const int model_height = this->model_handler_.get_input_height();
    const int model_channels = this->model_handler_.get_input_channels();
    if (model_width != this->input_width_ || model_height != this->input_height_ ||
        model_channels != this->input_channels_) {
      ESP_LOGE(TAG, "Configured input %dx%dx%d does not match the model's %dx%dx%d", this->input_width_,
               this->input_height_, this->input_channels_, model_width, model_height, model_channels);
      ESP_LOGE(TAG, "  Set input_width/input_height/input_channels, or supply the model's .txt report");
      return fail_load();
    }

    // The bytes handed to run_inference_on_buffer() are copied as-is, so a model
    // whose tensor type differs from the configured one would reinterpret them
    // silently and only show up as wrong predictions.
    const TfLiteType expected_type = configured_input_tensor_type(this->input_type_);
    const TfLiteTensor *input = this->model_handler_.input_tensor();
    if (input == nullptr) {
      // Without an input tensor nothing can be fed to the model, which is a failed
      // load rather than a mismatch to report against a tensor that is not there.
      ESP_LOGE(TAG, "Model has no input tensor");
      return fail_load();
    }
    if (input->type != expected_type) {
      ESP_LOGE(TAG, "Configured input_type '%s' does not match the model's %s input tensor",
               input_type_name(this->input_type_), LOG_STR_ARG(tensor_type_name(input->type)));
      ESP_LOGE(TAG, "  Supply the model's .txt report, or set input_type to the tensor's type");
      return fail_load();
    }
  }

  stats.success = true;
  stats.total_load_time_ms = millis() - stats.load_start_ms;
  this->last_load_stats_ = stats;

  this->state_.store(LoadState::READY);
  ESP_LOGI(TAG, "Model loaded successfully");
  if (this->debug_) {
    ESP_LOGI(TAG, "Load stats: parse=%" PRIu32 " ms, arena_alloc=%" PRIu32 " ms, total=%" PRIu32 " ms",
             stats.parse_time_ms, stats.arena_alloc_time_ms, stats.total_load_time_ms);
  }
  return true;
}

bool TFLiteMicroHelper::unload_model(bool reset_config) {
  // Only the owner of a completed load may unload. A load still in progress is
  // reading these buffers, and letting it finish would store READY over an
  // unloaded helper.
  LoadState state = LoadState::READY;
  if (!this->state_.compare_exchange_strong(state, LoadState::UNLOADED)) {
    if (state == LoadState::UNLOADED) {
      // Already unloaded, so the caller's goal holds: a reload path must not read
      // this as a failure. A requested reset still runs: a failed load leaves the
      // state UNLOADED while the fields still describe that model, so skipping the
      // reset would carry its arena size, geometry and audio settings into the next
      // load -- the leak reset_config() exists to prevent.
      ESP_LOGD(TAG, "unload_model() while no model is loaded");
      if (reset_config)
        this->reset_config();
      return true;
    }
    ESP_LOGE(TAG, "unload_model() while a model is loading -- not unloading");
    return false;
  }
  ESP_LOGI(TAG, "Unloading model and freeing arena");
  this->model_handler_.unload();

  // The arena is owned here: load_model() hands it to the loader, which never frees it.
  this->tensor_arena_allocation_.reset();

  // Config fields (model data, size, CRC, input/audio config) are preserved by
  // default so a same-model reload (meter_reader_tflite::reload_resources) works
  // without re-issuing the setters; reset_config() is for switching models.
  if (reset_config) {
    this->reset_config();
  }
  return true;
}

void TFLiteMicroHelper::reset_config() {
  ESP_LOGI(TAG, "Resetting model configuration to defaults");

  this->model_data_ = nullptr;
  this->model_length_ = 0;
  this->tensor_arena_size_requested_ = tflite_model_loader::ModelLoader::DEFAULT_TENSOR_ARENA_SIZE;
  this->tensor_arena_size_effective_ = 0;
  this->arena_heap_ = ArenaHeap::ARENA_HEAP_UNKNOWN;
  this->expected_crc32_ = 0;

  this->model_type_ = ModelType::MODEL_TYPE_IMAGE;
  this->input_type_ = InputType::INPUT_TYPE_UINT8;
  this->input_channels_ = 3;
  this->input_width_ = 32;
  this->input_height_ = 20;
  this->output_processing_ = "argmax";
  this->scale_factor_ = 1.0f;
  this->input_order_ = InputOrder::INPUT_ORDER_RGB;
  this->normalize_ = false;
  this->invert_ = false;

  this->audio_config_ = AudioModelConfig{};
}

ModelSpec TFLiteMicroHelper::get_model_spec() const {
  ModelSpec spec;
  if (!this->is_model_loaded())
    return spec;
  spec.input_width = this->get_input_width();
  spec.input_height = this->get_input_height();
  spec.input_channels = this->get_input_channels();
  spec.normalize = this->normalize_;
  spec.input_order = this->input_order_;

  // The input type comes from the tensor the model was converted with, not from a
  // byte-count heuristic: a heuristic mis-reads 3D/audio tensors, whose element
  // count is not width*height*channels.
  const TfLiteTensor *input = this->model_handler_.input_tensor();
  if (input != nullptr)
    spec.input_type = tensor_type_to_input_type(input->type);
  return spec;
}

ArenaStats TFLiteMicroHelper::get_arena_stats() const {
  ArenaStats stats;
  stats.total_size = this->get_tensor_arena_size_actual();
  stats.used_bytes = this->get_arena_used_bytes();
  stats.wasted_bytes = (stats.total_size > stats.used_bytes) ? (stats.total_size - stats.used_bytes) : 0;
  stats.efficiency = (stats.total_size > 0) ? (100.0f * stats.used_bytes / stats.total_size) : 0.0f;
  return stats;
}

void TFLiteMicroHelper::report_memory_status() {
  MemoryManager::report_memory_status(this->tensor_arena_size_requested_, this->get_tensor_arena_size_actual(),
                                      this->get_arena_used_bytes(), this->model_length_);

  // Warn when the largest contiguous free block can no longer satisfy the arena.
  // Query the heap the arena was actually allocated from (PSRAM vs internal) to
  // avoid false warnings when internal RAM is fragmented but PSRAM has sufficient
  // free contiguous space.
  //
  // Which heap that is only becomes known once a load has allocated the arena,
  // and this method is public: an audio model has no arena before its streaming
  // consumer loads it, and a call before setup() has none either. Measuring
  // internal RAM then would report fragmentation of a heap nothing was asked to
  // fit in.
  if (this->arena_heap_ == ArenaHeap::ARENA_HEAP_UNKNOWN) {
    ESP_LOGD(TAG, "Heap fragmentation not checked: no tensor arena has been allocated yet");
    return;
  }
  const bool from_psram = this->arena_heap_ == ArenaHeap::ARENA_HEAP_PSRAM;
  const uint32_t heap_caps = from_psram ? MALLOC_CAP_SPIRAM : MALLOC_CAP_INTERNAL;
  const size_t total_free = heap_caps_get_free_size(heap_caps);
  const size_t max_block = heap_caps_get_largest_free_block(heap_caps);

  ESP_LOGD(TAG, "Heap status - Total free: %zu bytes, Largest block: %zu bytes", total_free, max_block);

  // Compare against the size the last load actually used, which is what a reload
  // has to allocate again.
  const size_t needed =
      this->tensor_arena_size_effective_ > 0 ? this->tensor_arena_size_effective_ : this->tensor_arena_size_requested_;
  if (max_block < needed) {
    ESP_LOGW(TAG, "WARNING: Largest free block (%zu) < arena size (%zu)", max_block, needed);
  }
}

}  // namespace esphome::tflite_micro_runtime

#endif  // USE_TFLITE_MICRO_HELPER
