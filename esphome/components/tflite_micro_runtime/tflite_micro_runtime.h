#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include "esphome/components/tflite_memory_handler/memory_manager.h"
#include "esphome/components/tflite_model_loader/model_handler.h"
#include "esphome/components/tflite_output_processor/output_processor.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/optional.h"

#include <atomic>
#include <string>
#include <vector>
#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/micro/micro_interpreter.h"

namespace esphome::tflite_micro_runtime {

using tflite_memory_handler::MemoryManager;
using tflite_model_loader::ImageModelConfig;
using tflite_model_loader::ModelConfig;
using tflite_output_processor::ProcessedOutput;

/// Audio (streaming) parameters from the YAML configuration, read by the consumer.
struct AudioModelConfig {
  uint8_t probability_cutoff{128};  // quantized 0-255 (from 0.0-1.0)
  size_t sliding_window_size{10};
  uint8_t features_step_size{20};  // ms between feature slices
  size_t feature_count{40};        // Mel filterbank bins
};

/// Tensor arena utilization.
struct ArenaStats {
  size_t total_size{0};
  size_t used_bytes{0};
  size_t wasted_bytes{0};
  float efficiency{0.0f};
};

/// Timings and result of the last load_model().
struct LoadStats {
  uint32_t load_start_ms{0};
  uint32_t parse_time_ms{0};  // CRC verify + arena probe
  uint32_t arena_alloc_time_ms{0};
  uint32_t total_load_time_ms{0};
  bool success{false};
};

/// What the configured model consumes.
enum class ModelType : uint8_t {
  MODEL_TYPE_IMAGE,
  MODEL_TYPE_AUDIO,
};

/// Element type of the buffer handed to run_inference().
enum class InputType : uint8_t {
  INPUT_TYPE_UNKNOWN,  // no model loaded, or its input type is unsupported
  INPUT_TYPE_UINT8,
  INPUT_TYPE_INT8,
  INPUT_TYPE_FLOAT32,
};

/// Channel order of an image input.
enum class InputOrder : uint8_t {
  INPUT_ORDER_RGB,
  INPUT_ORDER_BGR,
  INPUT_ORDER_GRAY,
};

/// Heap the tensor arena came from.
enum class ArenaHeap : uint8_t {
  ARENA_HEAP_UNKNOWN,  // no arena allocated since setup()
  ARENA_HEAP_INTERNAL,
  ARENA_HEAP_PSRAM,
};

/// Model dimensions and input spec; IMAGE (4D) semantics, filled only by a loaded model.
struct ModelSpec {
  int input_width{0};
  int input_height{0};
  int input_channels{0};
  bool normalize{false};
  InputOrder input_order{InputOrder::INPUT_ORDER_RGB};
  InputType input_type{InputType::INPUT_TYPE_UNKNOWN};
};

/// Reusable TFLite Micro helper: model loading, tensor arena and inference.
/// A Component on its own: setup() loads the configured model and dump_config()
/// reports it. An audio model is loaded by its streaming consumer instead.
class TFLiteMicroHelper : public Component {
 public:
  TFLiteMicroHelper() = default;
  ~TFLiteMicroHelper() = default;

  // Component lifecycle
  void setup() override;
  void dump_config() override;

  // Common model config
  void set_model_type(ModelType t) { this->model_type_ = t; }
  void set_tensor_arena_size(size_t size);
  /// Point the helper at the model data in flash; false (logged) while a model is loaded or loading.
  bool set_model(const uint8_t *model_data, size_t model_size);
  void set_output_processing(const std::string &p) { this->output_processing_ = p; }
  void set_scale_factor(float f) { this->scale_factor_ = f; }
  /// Per-instance verbose logging (load timings, tensor details).
  void set_debug(bool d) {
    this->debug_ = d;
    this->model_handler_.set_debug(d);
  }

  // Image model config
  void set_input_type(InputType t) { this->input_type_ = t; }
  void set_input_channels(int c);
  void set_input_width(int w);
  void set_input_height(int h);
  void set_input_order(InputOrder o) { this->input_order_ = o; }
  void set_normalize(bool n);
  void set_invert(bool i);

  // Audio model config
  void set_probability_cutoff(float f);  // 0.0-1.0, quantized internally
  void set_sliding_window_size(size_t n);
  void set_features_step_size(uint8_t ms);
  void set_feature_count(size_t n);

  /// Validated audio parameters, for the consumer that streams the input.
  const AudioModelConfig &get_audio_config() const { return this->audio_config_; }

  void set_expected_crc32(uint32_t crc) { this->expected_crc32_ = crc; }

  /// Load the model and allocate the arena. An audio model is rejected: only its
  /// streaming consumer can supply the MicroResourceVariables.
  bool load_model();
  /// Unload the model and free the arena; true when unloaded on return.
  /// With reset_config, also clear every config field (see reset_config()).
  bool unload_model(bool reset_config = false);
  /// Reset all configuration fields to defaults, for switching models.
  void reset_config();
  bool is_model_loaded() const { return this->state_.load() == LoadState::READY; }

  // Inference
  /// Run the model on whatever is in input_tensor().
  TfLiteStatus invoke();
  /// Tensors of the loaded model; nullptr unless it is READY.
  TfLiteTensor *input_tensor() { return this->is_model_loaded() ? this->model_handler_.input_tensor() : nullptr; }
  const TfLiteTensor *input_tensor() const {
    return this->is_model_loaded() ? this->model_handler_.input_tensor() : nullptr;
  }
  TfLiteTensor *output_tensor() { return this->is_model_loaded() ? this->model_handler_.output_tensor() : nullptr; }

  bool run_inference_on_buffer(const uint8_t *src_data, size_t src_size);
  /// run_inference_on_buffer() plus output processing; nullopt (logged) on failure.
  optional<ProcessedOutput> run_inference(const uint8_t *src_data, size_t src_size);

  /// Model spec of the loaded model; all defaults when none is loaded.
  ModelSpec get_model_spec() const;

  // These read the interpreter, so they report 0 until the model is READY.
  int get_input_width() const { return this->is_model_loaded() ? this->model_handler_.get_input_width() : 0; }
  int get_input_height() const { return this->is_model_loaded() ? this->model_handler_.get_input_height() : 0; }
  int get_input_channels() const { return this->is_model_loaded() ? this->model_handler_.get_input_channels() : 0; }
  const ModelConfig &get_config() const { return this->model_handler_.get_config(); }

  // Memory
  size_t get_arena_used_bytes() const {
    return this->is_model_loaded() ? this->model_handler_.get_arena_used_bytes() : 0;
  }
  size_t get_tensor_arena_size() const { return this->tensor_arena_size_requested_; }
  /// Size the last load asked the allocator for, after probing (not a usage figure).
  size_t get_tensor_arena_size_effective() const { return this->tensor_arena_size_effective_; }
  size_t get_tensor_arena_size_actual() const { return this->tensor_arena_allocation_.actual_size; }
  size_t get_model_size_bytes() const { return this->model_length_; }
  ArenaStats get_arena_stats() const;
  void report_memory_status();

  const LoadStats &get_last_load_stats() const { return this->last_load_stats_; }

#ifdef USE_TFLITE_MODEL_DEBUG
  /// Parameter sweep of the debug component.
  void debug_test_parameters(const std::vector<std::vector<uint8_t>> &zone_data) {
    this->model_handler_.debug_test_parameters(zone_data);
  }
#endif

 private:
  // Model config (set from __init__.py)
  ModelType model_type_{ModelType::MODEL_TYPE_IMAGE};
  InputType input_type_{InputType::INPUT_TYPE_UINT8};
  int input_channels_{3};
  int input_width_{32};
  int input_height_{20};
  /// Passed by name to ModelConfig, which parses it into its own enum.
  std::string output_processing_{"argmax"};
  float scale_factor_{1.0f};
  InputOrder input_order_{InputOrder::INPUT_ORDER_RGB};
  bool normalize_{false};
  bool invert_{false};
  bool debug_{false};

  // Model data (set via set_model)
  const uint8_t *model_data_{nullptr};
  size_t model_length_{0};
  size_t tensor_arena_size_requested_{tflite_model_loader::ModelLoader::DEFAULT_TENSOR_ARENA_SIZE};
  size_t tensor_arena_size_effective_{0};  // after probing; 0 before the first load

  AudioModelConfig audio_config_;

  tflite_model_loader::ModelHandler model_handler_;
  /// Tensor arena owned by this helper; freed in unload_model().
  MemoryManager::AllocationResult tensor_arena_allocation_;
  /// Heap the last arena came from, cached at allocation for reports after an unload.
  ArenaHeap arena_heap_{ArenaHeap::ARENA_HEAP_UNKNOWN};

  LoadStats last_load_stats_{};

  // One atomic value, no check-then-act across flags.
  enum class LoadState : uint8_t { UNLOADED, LOADING, READY };
  std::atomic<LoadState> state_{LoadState::UNLOADED};

  uint32_t expected_crc32_{0};  // 0 = skip verification, set from __init__.py

  /// Why the model data does not match the configured checksum, or nullptr.
  const LogString *check_model_integrity_();
  bool allocate_tensor_arena_();
  ModelConfig build_config_();
  bool validate_input_tensor_(const uint8_t *src_data, size_t src_size) const;
};

}  // namespace esphome::tflite_micro_runtime

#endif  // USE_TFLITE_MICRO_HELPER
