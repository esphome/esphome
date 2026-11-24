#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include "esphome/components/tflite_memory_handler/memory_manager.h"
#include "esphome/components/tflite_model_loader/model_handler.h"
#include "esphome/components/tflite_output_processor/output_processor.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/string_ref.h"

#include <atomic>
#include <vector>
#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/micro/micro_interpreter.h"

namespace esphome::tflite_micro_runtime {

using tflite_memory_handler::MemoryManager;
using tflite_model_loader::ImageModelConfig;
using tflite_model_loader::ModelConfig;

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

enum class InputType : uint8_t {
  INPUT_TYPE_UNKNOWN,  // no model loaded, or its input type is unsupported
  INPUT_TYPE_UINT8,
  INPUT_TYPE_INT8,
  INPUT_TYPE_FLOAT32,
};

enum class ArenaHeap : uint8_t {
  ARENA_HEAP_UNKNOWN,  // no arena allocated since setup()
  ARENA_HEAP_INTERNAL,
  ARENA_HEAP_PSRAM,
};

/// IMAGE (4D) input spec, filled only by a loaded model.
struct ModelSpec {
  int input_width{0};
  int input_height{0};
  int input_channels{0};
  InputType input_type{InputType::INPUT_TYPE_UNKNOWN};
};

/// @brief Fills the model's input tensor(s) from a payload.
///
/// A model kind implements this when the input is not a raw byte copy: image frames
/// need channel order and normalization, text needs several int32 tensors.
class InputPipeline {
 public:
  virtual ~InputPipeline() = default;
  /// @return true when the tensors were filled.
  virtual bool fill(TfLiteTensor *input, const uint8_t *src, size_t size) = 0;
  /// The runtime calls this with every input tensor; a kind that fills more
  /// than one (text needs ids, mask and segment) overrides it.
  virtual bool fill_all(TfLiteTensor *const *inputs, size_t count, const uint8_t *src, size_t size) {
    // The single-tensor fallback cannot fill a multi-input model.
    return count == 1 && this->fill(inputs[0], src, size);
  }
  /// Set the size of an incoming frame that a consumer learns at run time, such
  /// as a decoded camera frame. Pipelines that do not scale frames ignore it.
  virtual void set_frame_geometry(int width, int height) {
    (void) width;
    (void) height;
  }
  /// Name for dump_config(), e.g. "image".
  virtual const char *name() const = 0;
};

class TFLiteMicroHelper;

/// Model loading, tensor arena and inference. A Component on its own, so setup()
/// loads the configured model; an audio model is left to its streaming consumer.
class TFLiteMicroHelper : public Component {
 public:
  TFLiteMicroHelper() = default;
  // The interpreter reads the arena in its destructor, so unload it first.
  ~TFLiteMicroHelper() { this->model_handler_.unload(); }

  void setup() override;
  void dump_config() override;

  void set_tensor_arena_size(size_t size);
  /// Point the helper at the model data in flash and its checksum, in one step.
  /// False while a model is loaded or loading.
  bool set_model(const uint8_t *model_data, size_t model_size, uint32_t expected_crc32);
  /// The name must outlive the component; code generation passes a string literal.
  void set_output_processing(const char *p) { this->output_processing_ = StringRef(p); }
  void set_scale_factor(float f) { this->scale_factor_ = f; }
  void set_debug(bool d) {
    this->debug_ = d;
    this->model_handler_.set_debug(d);
  }

  /// Load the model and allocate the arena; false when one is already loaded.
  bool load_model();
  /// Unload the model and free the arena; true when unloaded on return.
  bool unload_model(bool reset_config = false);
  bool is_model_loaded() const { return this->state_.load() == LoadState::LOAD_STATE_READY; }

  /// Run the model on whatever is in input_tensor().
  TfLiteStatus invoke();
  /// Tensors of the loaded model; nullptr unless it is READY. The pointer stays
  /// valid only while no other task unloads the model.
  TfLiteTensor *input_tensor();
  const TfLiteTensor *input_tensor() const;
  TfLiteTensor *output_tensor();

  bool run_inference_on_buffer(const uint8_t *src_data, size_t src_size);

  void set_input_pipeline(InputPipeline *pipeline) { this->input_pipeline_ = pipeline; }
  /// Leave the load to a streaming consumer; setup() only checks the checksum.
  void set_defer_load(bool defer) { this->defer_load_ = defer; }
  void set_output_decoder(tflite_output_processor::OutputDecoder *decoder) { this->output_decoder_ = decoder; }
  /// Resource variables for a stateful (streaming) model; nullptr for a stateless one.
  void set_resource_variables(tflite::MicroResourceVariables *variables) { this->resource_variables_ = variables; }

  /// Model spec of the loaded model; all defaults when none is loaded.
  ModelSpec get_model_spec();

  // These read the interpreter, so they report 0 until the model is READY.
  int get_input_width();
  int get_input_height();
  int get_input_channels();
  /// A copy of the loaded model's configuration; defaults when none is loaded.
  ModelConfig get_config();
  size_t get_arena_used_bytes();

  size_t get_tensor_arena_size();
  /// Size the last load asked the allocator for, after probing (not a usage figure).
  size_t get_tensor_arena_size_effective();
  size_t get_tensor_arena_size_actual();
  size_t get_model_size_bytes();
  ArenaStats get_arena_stats();
  void report_memory_status();
  LoadStats get_last_load_stats();

#ifdef USE_TFLITE_MODEL_DEBUG
  void debug_test_parameters(const std::vector<std::vector<uint8_t>> &zone_data) {
    LockGuard guard(this);
    if (guard.locked())
      this->model_handler_.debug_test_parameters(zone_data);
  }
#endif

 private:
  /// Serializes load, unload, inference and set_model(), so an unload from another
  /// task cannot free the arena while it is in use.
  class LockGuard {
   public:
    explicit LockGuard(TFLiteMicroHelper *helper) : helper_(helper) { helper_->mutex_.lock(); }
    ~LockGuard() { helper_->mutex_.unlock(); }
    LockGuard(const LockGuard &) = delete;
    LockGuard &operator=(const LockGuard &) = delete;
    bool locked() const { return true; }

   private:
    TFLiteMicroHelper *helper_;
  };

  int get_input_width_locked_() const;
  int get_input_height_locked_() const;
  int get_input_channels_locked_() const;
  /// Copy raw source bytes into the model's single input tensor.
  bool fill_raw_input_locked_(const uint8_t *src_data, size_t src_size);

  /// Clear the model configuration after the arena and interpreter are gone.
  void reset_config();
  StringRef output_processing_{"argmax"};  // passed by name; ModelConfig parses it
  float scale_factor_{1.0f};
  bool debug_{false};

  const uint8_t *model_data_{nullptr};
  size_t model_length_{0};
  size_t tensor_arena_size_requested_{tflite_model_loader::ModelLoader::DEFAULT_TENSOR_ARENA_SIZE};
  size_t tensor_arena_size_effective_{0};  // after probing; 0 before the first load

  tflite_model_loader::ModelHandler model_handler_;
  InputPipeline *input_pipeline_{nullptr};
  bool defer_load_{false};
  tflite_output_processor::OutputDecoder *output_decoder_{nullptr};
  tflite::MicroResourceVariables *resource_variables_{nullptr};
  MemoryManager::AllocationResult tensor_arena_allocation_;  // owned here, freed in unload_model()
  ArenaHeap arena_heap_{ArenaHeap::ARENA_HEAP_UNKNOWN};      // cached for reports after an unload
  LoadStats last_load_stats_{};

  // One atomic value, so there is no check-then-act across flags.
  enum class LoadState : uint8_t {
    LOAD_STATE_UNLOADED,
    LOAD_STATE_READY,
  };
  std::atomic<LoadState> state_{LoadState::LOAD_STATE_UNLOADED};
  uint32_t expected_crc32_{0};  // 0 = skip verification, set from __init__.py

  /// Serializes load, unload, inference and set_model(), so an unload from another
  /// task cannot free the arena while it is in use.
  esphome::Mutex mutex_;

  TfLiteStatus invoke_locked_();
  bool run_inference_on_buffer_locked_(const uint8_t *src_data, size_t src_size);

  /// Why the model data does not match the configured checksum, or nullptr.
  const LogString *check_model_integrity_();
  bool allocate_tensor_arena_();
  ModelConfig build_config_();
  bool validate_input_tensor_(const uint8_t *src_data, size_t src_size) const;
};

}  // namespace esphome::tflite_micro_runtime

#endif  // USE_TFLITE_MICRO_HELPER
