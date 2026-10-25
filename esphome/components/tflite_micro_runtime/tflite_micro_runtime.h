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

/**
 * @struct AudioModelConfig
 * @brief Audio (streaming) model parameters from the YAML configuration.
 *
 * TFLiteMicroHelper stores them so a consumer can read the validated values; the
 * streaming runtime itself (resource variables, sliding window, stride) belongs
 * to the consumer. The helper never loads an audio model: see setup().
 */
struct AudioModelConfig {
  uint8_t probability_cutoff{128};  // quantized 0-255 (from 0.0-1.0)
  size_t sliding_window_size{10};
  uint8_t features_step_size{20};  // ms between feature slices
  size_t feature_count{40};        // Mel filterbank bins (e.g., PREPROCESSOR_FEATURE_SIZE)
};

/**
 * @struct ArenaStats
 * @brief Tensor arena utilization statistics.
 */
struct ArenaStats {
  size_t total_size{0};
  size_t used_bytes{0};
  size_t wasted_bytes{0};
  float efficiency{0.0f};
};

/**
 * @struct LoadStats
 * @brief Model loading stage timings and final result.
 */
struct LoadStats {
  uint32_t load_start_ms{0};
  uint32_t parse_time_ms{0};  // CRC verify + arena probe
  uint32_t arena_alloc_time_ms{0};
  uint32_t total_load_time_ms{0};
  bool success{false};
};

/// @brief What the configured model consumes, which decides the load-time checks.
enum class ModelType : uint8_t {
  MODEL_TYPE_IMAGE,
  MODEL_TYPE_AUDIO,
};

/// @brief Element type of the buffer handed to run_inference().
enum class InputType : uint8_t {
  /// Nothing is known yet: no model is loaded, or its input type is unsupported.
  INPUT_TYPE_UNKNOWN,
  INPUT_TYPE_UINT8,
  INPUT_TYPE_INT8,
  INPUT_TYPE_FLOAT32,
};

/// @brief Channel order of an image input.
enum class InputOrder : uint8_t {
  INPUT_ORDER_RGB,
  INPUT_ORDER_BGR,
  INPUT_ORDER_GRAY,
};

/**
 * @struct ModelSpec
 * @brief Model dimensions and input spec as a consumer reads them.
 *
 * NOTE: This struct uses IMAGE (4D) tensor semantics and only a loaded model fills
 * it: the helper leaves an audio model to its streaming consumer, so every field
 * keeps its default there. Audio consumers read input_tensor()->dims directly and
 * manage their own streaming runtime (MRV, sliding window).
 */
struct ModelSpec {
  int input_width{0};
  int input_height{0};
  int input_channels{0};
  bool normalize{false};
  InputOrder input_order{InputOrder::INPUT_ORDER_RGB};
  InputType input_type{InputType::INPUT_TYPE_UNKNOWN};
};

/**
 * @class TFLiteMicroHelper
 * @brief Reusable TFLite Micro component for ESPHome.
 *
 * Wraps model loading, tensor arena management, config building,
 * and inference invocation. Consumer components set config via setters
 * then call load_model() / run_inference().
 *
 * It is also a Component on its own, so a `tflite_micro_runtime` entry without a
 * consumer still does something: setup() loads the configured model and
 * dump_config() reports the model spec and the tensor arena usage at boot. An
 * audio model is the exception: loading it needs the MicroResourceVariables that
 * belong to the streaming consumer, so setup() leaves that model to the consumer.
 */
class TFLiteMicroHelper : public Component {
 public:
  TFLiteMicroHelper() = default;
  ~TFLiteMicroHelper() = default;

  // -- Component lifecycle --------------------------------------------
  void setup() override;
  void dump_config() override;

  // -- Common Model Config Setters -----------------------------------
  void set_model_type(ModelType t) { this->model_type_ = t; }
  void set_tensor_arena_size(size_t size);
  /// @brief Point the helper at the model data in flash.
  /// @return false (logged) when a model is loaded or loading; call
  ///   unload_model() first. True when the data was accepted.
  bool set_model(const uint8_t *model_data, size_t model_size);
  void set_output_processing(const std::string &p) { this->output_processing_ = p; }
  void set_scale_factor(float f) { this->scale_factor_ = f; }
  /// @brief Per-instance verbose logging (load timings, tensor details).
  /// The `debug:` option only calls this: it is not tied to a build-wide define,
  /// so the setting of one model never changes what another one logs.
  void set_debug(bool d) {
    this->debug_ = d;
    this->model_handler_.set_debug(d);
  }

  // -- Image Model Config Setters ------------------------------------
  void set_input_type(InputType t) { this->input_type_ = t; }
  void set_input_channels(int c);
  void set_input_width(int w);
  void set_input_height(int h);
  void set_input_order(InputOrder o) { this->input_order_ = o; }
  void set_normalize(bool n);
  void set_invert(bool i);

  // -- Audio Model Config Setters ------------------------------------
  void set_probability_cutoff(float f);  // 0.0-1.0, quantized internally
  void set_sliding_window_size(size_t n);
  void set_features_step_size(uint8_t ms);
  void set_feature_count(size_t n);

  /// @brief Validated audio parameters, for the consumer that streams the input.
  const AudioModelConfig &get_audio_config() const { return this->audio_config_; }

  // -- Model Integrity ------------------------------------------------
  void set_expected_crc32(uint32_t crc) { this->expected_crc32_ = crc; }

  // -- Lifecycle ------------------------------------------------------
  /// @brief Load the model into the interpreter and allocate the tensor arena.
  /// @return false (logged) when loading fails, and when a model is already
  ///   loaded or loading -- call unload_model() first. setup() performs the load
  ///   for an image model, so a consumer that finds is_model_loaded() true has
  ///   nothing to do. An audio model is never loaded by setup(): the streaming
  ///   consumer calls this with the MicroResourceVariables it owns.
  bool load_model();

  /// @brief Unload the model and free the arena.
  /// @param reset_config If true, also clear all model-config fields via
  ///   reset_config() so a subsequent load_model() starts from clean defaults.
  ///   Use this when switching to a *different* model -- the consumer must then
  ///   re-issue set_model() and all relevant setters before load_model().
  ///   Keep false (default) for same-model reload where config is preserved
  ///   (e.g. meter_reader_tflite::reload_resources).
  /// @return true when the helper is UNLOADED on return, which includes the case
  ///   where nothing was loaded. False (logged) only when a load is in progress:
  ///   that load owns the arena, so nothing was freed.
  bool unload_model(bool reset_config = false);

  /// @brief Reset all configuration fields to type-safe defaults (P1).
  /// Prevents a stale arena size, input dimensions, preprocessing, or audio
  /// configuration from leaking into the next load_model() when a consumer
  /// switches to a different model. Must be followed by fresh setters +
  /// set_model() before load_model().
  void reset_config();

  /// @brief Returns true only when the model has been fully loaded (READY).
  /// Uses a single atomic state machine: no check-then-act across multiple flags.
  bool is_model_loaded() const { return this->state_.load() == LoadState::READY; }

  // -- Inference ------------------------------------------------------
  /// @brief Run the model on whatever is in input_tensor().
  /// @return kTfLiteError (logged) when no model is loaded.
  TfLiteStatus invoke();

  /// @brief Tensors of the loaded model; nullptr unless it is READY.
  /// A partial or failed load leaves no valid tensors, so the state check lives
  /// here instead of in every consumer.
  TfLiteTensor *input_tensor() { return this->is_model_loaded() ? this->model_handler_.input_tensor() : nullptr; }
  const TfLiteTensor *input_tensor() const {
    return this->is_model_loaded() ? this->model_handler_.input_tensor() : nullptr;
  }
  TfLiteTensor *output_tensor() { return this->is_model_loaded() ? this->model_handler_.output_tensor() : nullptr; }

  bool run_inference_on_buffer(const uint8_t *src_data, size_t src_size);

  /// @brief run_inference_on_buffer() plus output processing.
  /// @return nullopt when the model is not loaded, the buffer does not match the
  ///   input tensor, or the invocation fails. The reason is logged; a failed run
  ///   is never reported as a reading.
  optional<ProcessedOutput> run_inference(const uint8_t *src_data, size_t src_size);

  // -- Accessors -----------------------------------------------------
  /// @brief Model spec of the loaded model; all defaults when none is loaded.
  ModelSpec get_model_spec() const;

  // These read the interpreter, so they report 0 until the model is READY: a
  // partial or failed load leaves no tensors, and a consumer would otherwise act
  // on the dimensions of a model that never finished loading.
  int get_input_width() const { return this->is_model_loaded() ? this->model_handler_.get_input_width() : 0; }
  int get_input_height() const { return this->is_model_loaded() ? this->model_handler_.get_input_height() : 0; }
  int get_input_channels() const { return this->is_model_loaded() ? this->model_handler_.get_input_channels() : 0; }
  const ModelConfig &get_config() const { return this->model_handler_.get_config(); }

  // -- Memory ---------------------------------------------------------
  /// @brief Bytes the loaded model's tensors occupy in the arena; 0 when not loaded.
  size_t get_arena_used_bytes() const {
    return this->is_model_loaded() ? this->model_handler_.get_arena_used_bytes() : 0;
  }
  /// @brief Tensor arena size requested by the configuration.
  size_t get_tensor_arena_size() const { return this->tensor_arena_size_requested_; }
  /// @brief Tensor arena size the last load actually used, after probing.
  size_t get_tensor_arena_size_effective() const { return this->tensor_arena_size_effective_; }
  size_t get_tensor_arena_size_actual() const { return this->tensor_arena_allocation_.actual_size; }
  size_t get_model_size_bytes() const { return this->model_length_; }

  /// @brief Current tensor arena utilization, read from the live interpreter.
  ArenaStats get_arena_stats() const;
  void report_memory_status();

  // -- Load statistics -------------------------------------------------
  const LoadStats &get_last_load_stats() const { return this->last_load_stats_; }

#ifdef USE_TFLITE_MODEL_DEBUG
  /// @brief Parameter sweep of the debug component. Gated on the component that
  /// implements it, not on any model's `debug:` option.
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
  /// Passed through by name to ModelConfig, which parses it into its own enum.
  std::string output_processing_{"argmax"};
  float scale_factor_{1.0f};
  InputOrder input_order_{InputOrder::INPUT_ORDER_RGB};
  bool normalize_{false};
  bool invert_{false};
  bool debug_{false};

  // Model data (set via set_model)
  const uint8_t *model_data_{nullptr};
  size_t model_length_{0};
  /// Configured size, kept as-is so a reload uses the same value.
  size_t tensor_arena_size_requested_{tflite_model_loader::ModelLoader::DEFAULT_TENSOR_ARENA_SIZE};
  /// Size the last load used after probing; 0 before the first load.
  size_t tensor_arena_size_effective_{0};

  // Audio model config
  AudioModelConfig audio_config_;

  // Components
  tflite_model_loader::ModelHandler model_handler_;
  /// Tensor arena owned by this helper; freed in unload_model().
  MemoryManager::AllocationResult tensor_arena_allocation_;
  /// Heap the last arena came from, cached at allocation so a report made after
  /// unload_model() still monitors the heap the model actually had to share.
  bool tensor_arena_from_psram_{false};

  // Load statistics (last load_model() attempt)
  LoadStats last_load_stats_{};

  // State -- single atomic load state machine (E2 / TOCTOU fix)
  enum class LoadState : uint8_t { UNLOADED, LOADING, READY };
  std::atomic<LoadState> state_{LoadState::UNLOADED};

  // Expected CRC32 (0 = skip verification, set from __init__.py)
  uint32_t expected_crc32_{0};

  // Internals
  bool allocate_tensor_arena_();
  ModelConfig build_config_();
  bool validate_input_tensor_(const uint8_t *src_data, size_t src_size) const;
};

}  // namespace esphome::tflite_micro_runtime

#endif  // USE_TFLITE_MICRO_HELPER
