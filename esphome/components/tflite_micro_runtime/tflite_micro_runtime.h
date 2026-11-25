#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include "esphome/components/tflite_memory_handler/memory_manager.h"
#include "esphome/components/tflite_model_loader/model_handler.h"
#include "esphome/components/tflite_output_processor/output_processor.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/main_task.h"
#include "esphome/core/optional.h"
#include "esphome/core/string_ref.h"

#include <atomic>
#include <string>
#include <utility>
#include <vector>
#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/micro/micro_interpreter.h"

namespace esphome::tflite_micro_runtime {

using tflite_memory_handler::MemoryManager;
using tflite_model_loader::ImageModelConfig;
using tflite_model_loader::ModelConfig;
using tflite_output_processor::ProcessedOutput;

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
class InferenceSession;

/// Model loading, tensor arena and inference. A Component on its own, so setup()
/// loads the configured model; an audio model is left to its streaming consumer.
class TFLiteMicroHelper : public Component {
 public:
  TFLiteMicroHelper() = default;
  // The interpreter reads the arena in its destructor, so unload it first.
  ~TFLiteMicroHelper() { this->model_handler_.unload(); }

  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_tensor_arena_size(size_t size);
  /// Point the helper at the model data in flash and its checksum, in one step.
  /// False while a model is loaded or loading. The build-wide resolver capacity is
  /// fixed from the configured models, so a replacement must not need more slots.
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
  /// Unload the model and free the arena; true when unloaded on return. Refused
  /// while a result waits for its callback, which reads the output tensors.
  bool unload_model(bool reset_config = false);
  bool is_model_loaded() const { return this->state_.load() == LoadState::LOAD_STATE_READY; }

  /// Run the model on whatever is in input_tensor(). Prefer InferenceSession.
  TfLiteStatus invoke();
  /// Tensors of the loaded model; nullptr unless it is READY. Another task can
  /// unload the model between the call and the use; use InferenceSession instead.
  TfLiteTensor *input_tensor();
  const TfLiteTensor *input_tensor() const;
  TfLiteTensor *output_tensor();

  bool run_inference_on_buffer(const uint8_t *src_data, size_t src_size);
  /// run_inference_on_buffer() plus output processing; nullopt (logged) on failure.
  optional<ProcessedOutput> run_inference(const uint8_t *src_data, size_t src_size);

  /// Fill through the pipeline when one is set, invoke, then decode every output.
  bool run(const uint8_t *src_data, size_t src_size);
  /// run() with a frame size learned at run time (a decoded camera frame). The
  /// geometry is set under the inference lock before the pipeline fills the tensor.
  bool run(const uint8_t *src_data, size_t src_size, int frame_width, int frame_height);

  /// Called after run() decoded a result, or run_inference() produced one. The
  /// callbacks run on the main loop, synchronously, while the result is reserved:
  /// run() and run_inference() are refused until they return. Read the kind's
  /// decoder, or get_last_result() for a raw model without one. An action that
  /// keeps running after the callback returns (delay, script.execute, defer) is
  /// outside that window: the next inference reuses the tensors and the decoder,
  /// so copy the values it needs or read the entities the callback published.
  template<typename F> void add_on_result_callback(F &&callback) {
    this->result_callback_.add(std::forward<F>(callback));
  }

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
  /// The result run_inference() produced for the current notification.
  ProcessedOutput get_last_result();

#ifdef USE_TFLITE_MODEL_DEBUG
  /// Sweep the model's parameters. Refused while a result waits for its callback,
  /// because the sweep overwrites the tensors that callback reads.
  void debug_test_parameters(const std::vector<std::vector<uint8_t>> &zone_data);
#endif

 private:
  friend class InferenceSession;

  /// Lock the lifecycle mutex, or report a same-task re-entry instead of blocking.
  class LockGuard {
   public:
    explicit LockGuard(TFLiteMicroHelper *helper);
    ~LockGuard();
    LockGuard(const LockGuard &) = delete;
    LockGuard &operator=(const LockGuard &) = delete;
    bool locked() const { return this->locked_; }
    /// Release the lock early, for a session that turns out to be blocked.
    void unlock();

   private:
    TFLiteMicroHelper *helper_;
    bool locked_{false};
  };

  /// Reserve the notification slot under the mutex, or refuse this run.
  bool reserve_result_locked_();
  /// Whether a delivered result is still waiting, so the tensors must not change.
  bool result_blocks_inference_locked_() const;
  /// Run on the main loop: deliver the pending result, then clear the slot.
  void dispatch_result_();
  bool run_(const uint8_t *src_data, size_t src_size, int frame_width, int frame_height);
  /// Queue the result for delivery and deliver it right away on the main task.
  void finish_run_(bool succeeded);

  int get_input_width_locked_() const;
  int get_input_height_locked_() const;
  int get_input_channels_locked_() const;
  /// Copy raw source bytes into the model's single input tensor.
  bool fill_raw_input_locked_(const uint8_t *src_data, size_t src_size);

  /// Clear the model configuration after the arena and interpreter are gone.
  void reset_config();
  // Word-sized members first; the one-byte fields are grouped so pointers do not
  // each pay for their padding.
  size_t model_length_{0};
  size_t tensor_arena_size_requested_{tflite_model_loader::ModelLoader::DEFAULT_TENSOR_ARENA_SIZE};
  size_t tensor_arena_size_effective_{0};  // after probing; 0 before the first load
  size_t arena_size_probed_{0};            // last proven probe; 0 = probe again
  uint32_t expected_crc32_{0};             // 0 = skip verification, set from __init__.py
  StringRef output_processing_{"argmax"};  // passed by name; ModelConfig parses it
  float scale_factor_{1.0f};
  LoadStats last_load_stats_{};
  ArenaHeap arena_heap_{ArenaHeap::ARENA_HEAP_UNKNOWN};  // cached for reports after an unload
  bool debug_{false};
  bool defer_load_{false};

  const uint8_t *model_data_{nullptr};
  tflite_model_loader::ModelHandler model_handler_;
  InputPipeline *input_pipeline_{nullptr};
  tflite_output_processor::OutputDecoder *output_decoder_{nullptr};
  tflite::MicroResourceVariables *resource_variables_{nullptr};
  MemoryManager::AllocationResult tensor_arena_allocation_;  // owned here, freed in unload_model()

  // One atomic value, so there is no check-then-act across flags.
  enum class LoadState : uint8_t {
    LOAD_STATE_UNLOADED,
    LOAD_STATE_READY,
  };
  std::atomic<LoadState> state_{LoadState::LOAD_STATE_UNLOADED};

  /// Each successful run advances pending_epoch_; dispatch_result_() stores the
  /// epoch it delivered.
  std::atomic<uint32_t> pending_epoch_{0};
  std::atomic<uint32_t> dispatched_epoch_{0};
  /// Task holding mutex_, so a log emitted under the lock cannot deadlock the device.
  std::atomic<TaskHandle_t> lock_owner_{nullptr};
  /// The result run_inference() produced, kept until its callback runs. Invalid
  /// until an inference fills it.
  ProcessedOutput last_result_{0.0f, 0.0f, false};
  /// Null unless a run is configured with `on_result`, so an idle instance pays 4 bytes.
  LazyCallbackManager<void()> result_callback_;

  /// Serializes load, unload, inference and set_model(), so an unload from another
  /// task cannot free the arena while it is in use. The mutex is not recursive, so
  /// a same-task re-entry is rejected by LockGuard instead of deadlocking.
  esphome::Mutex mutex_;
  /// True while the main loop is inside the callback. A run started there is
  /// refused, so the callbacks for the current result all read that result.
  std::atomic<bool> dispatching_{false};
  /// Set once per re-entry so the nested error log cannot recurse forever.
  std::atomic<bool> reentry_reported_{false};
  /// True from the start of a run until its callback is delivered, so another
  /// task cannot overwrite the decoder before the callbacks read it.
  bool result_reserved_{false};
  TfLiteStatus invoke_locked_();
  bool run_inference_on_buffer_locked_(const uint8_t *src_data, size_t src_size);
  bool run_locked_(const uint8_t *src_data, size_t src_size);
  /// Decode the loaded model's outputs into output_decoder_, when one is set.
  bool decode_outputs_locked_();

  /// Why the model data does not match the configured checksum, or nullptr.
  const LogString *check_model_integrity_();
  bool allocate_tensor_arena_();
  ModelConfig build_config_();
  bool validate_input_tensor_(const uint8_t *src_data, size_t src_size) const;
};

/// @brief Holds the inference lock while a consumer fills the input, invokes and reads
/// the output, so another task cannot unload the interpreter in between.
///
/// Stack-allocate it around the whole raw-inference block:
///
///     InferenceSession session(&helper);
///     if (!session.valid()) return;
///     TfLiteTensor *input = session.input_tensor();
///     ...
///     session.invoke();
///     TfLiteTensor *output = session.output_tensor();
class InferenceSession {
 public:
  explicit InferenceSession(TFLiteMicroHelper *helper) : helper_(helper), guard_(helper) {
    this->locked_ = helper != nullptr && this->guard_.locked();
    // A result waiting for its callback must keep the tensors it describes.
    if (this->locked_ && helper->result_blocks_inference_locked_()) {
      this->guard_.unlock();
      this->locked_ = false;
    }
  }
  ~InferenceSession() = default;
  InferenceSession(const InferenceSession &) = delete;
  InferenceSession &operator=(const InferenceSession &) = delete;

  /// False when the lock could not be taken (re-entered from a log callback).
  bool valid() const { return this->locked_; }
  /// Tensors of the loaded model; nullptr unless a model is loaded.
  TfLiteTensor *input_tensor();
  const TfLiteTensor *input_tensor() const;
  TfLiteTensor *output_tensor();
  /// Run the model on whatever is in input_tensor().
  TfLiteStatus invoke();

 protected:
  TFLiteMicroHelper *helper_;
  TFLiteMicroHelper::LockGuard guard_;
  bool locked_{false};
};

}  // namespace esphome::tflite_micro_runtime

#endif  // USE_TFLITE_MICRO_HELPER
