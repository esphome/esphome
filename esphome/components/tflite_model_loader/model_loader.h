#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "esphome/components/tflite_memory_handler/memory_manager.h"
#include "esphome/components/tflite_op_processor/op_resolver.h"
#include "esphome/components/tflite_output_processor/output_processor.h"

namespace esphome::tflite_model_loader {

/// @brief Model configuration passed to the loader at load time.
struct ModelConfig {
  std::string description;
  /// One of the tflite_output_processor modes: "argmax", "softmax",
  /// "qat_quantized" or "auto_detect".
  std::string output_processing;
  float scale_factor{1.0f};  // Scale factor for class index normalization (for large class counts)
};

/// @brief Loads a TFLite model into an interpreter with the given tensor arena.
/// Owns the interpreter and op resolver for the loaded model.
class ModelLoader {
 public:
  /// @brief Tensor arena size used when none was requested explicitly.
  static constexpr size_t DEFAULT_TENSOR_ARENA_SIZE = 100 * 1024;

  /// @brief Unloads the model and frees all associated memory.
  void unload();

  /// @brief Loads a model using the provided data and config.
  /// Allocates the tensor arena via MemoryManager (PSRAM-aware).
  bool load_model(const uint8_t *model_data, size_t model_size, const ModelConfig &config);

  /// @brief Loads a model with an externally allocated tensor arena.
  /// If mrv is provided, uses it for resource variables (e.g., streaming audio models).
  bool load_model_with_arena(const uint8_t *model_data, size_t model_size, uint8_t *tensor_arena,
                             size_t tensor_arena_size, const ModelConfig &config,
                             tflite::MicroResourceVariables *mrv = nullptr);

  /// @brief Get the loaded TFLite model.
  const tflite::Model *tflite_model() const { return this->tflite_model_; }

  /// @brief Get the raw interpreter (nullptr when no model is loaded).
  tflite::MicroInterpreter *interpreter() { return this->interpreter_.get(); }

  /// @brief Get the model size in bytes.
  size_t model_length() const { return this->model_length_; }

  /// @brief True when a model is loaded and tensors are allocated.
  bool is_loaded() const { return this->interpreter_ != nullptr && this->tflite_model_ != nullptr; }

  /// @brief Get a pointer to the input tensor (nullptr when no model is loaded).
  TfLiteTensor *input_tensor();

  /// @brief Get a pointer to the input tensor (nullptr when no model is loaded).
  const TfLiteTensor *input_tensor() const;

  /// @brief Get a pointer to the output tensor (nullptr when no model is loaded).
  TfLiteTensor *output_tensor();

  /// @brief Get a pointer to the output tensor (nullptr when no model is loaded).
  const TfLiteTensor *output_tensor() const;

  /// @brief Run inference on the loaded model. Returns kTfLiteOk on success.
  TfLiteStatus invoke();

  /// @brief Get the number of bytes used in the arena after allocation.
  size_t get_arena_used_bytes() const;

  /// @brief Get the tensor arena size in use (0 when no model is loaded).
  size_t get_tensor_arena_size() const { return this->tensor_arena_size_; }

  /// @brief Get the tensor arena size requested from MemoryManager.
  size_t tensor_arena_size_requested() const { return this->tensor_arena_size_requested_; }

  /// @brief Set the tensor arena size to request from MemoryManager.
  void set_tensor_arena_size(size_t size) { this->tensor_arena_size_requested_ = size; }

  /// @brief Get the configuration of the loaded model.
  const ModelConfig &config() const { return this->config_; }

  /// @brief Get the output processing mode resolved from config().output_processing.
  ///
  /// Resolved once when the configuration is committed, so the inference path hands
  /// process_output() the enum instead of comparing mode names every time.
  tflite_output_processor::OutputProcessing output_processing() const { return this->output_processing_; }

  /// @brief Get the number of elements in the model output.
  int output_size() const { return this->output_size_; }

  /// @brief Enable or disable verbose logging.
  void set_debug(bool debug) { this->debug_ = debug; }

 protected:
  /// @brief Store the configuration and resolve its output processing mode.
  void set_config(const ModelConfig &config) {
    this->config_ = config;
    this->output_processing_ = tflite_output_processor::parse_output_processing(config.output_processing);
  }

  /// @brief Load a model into an already allocated tensor arena.
  /// On failure the model state is only partially cleared; use unload() or
  /// load_model_with_arena() so the interpreter never keeps a stale arena pointer.
  bool load_with_arena_(const uint8_t *model_data, size_t model_size, uint8_t *tensor_arena, size_t tensor_arena_size,
                        const ModelConfig &config, tflite::MicroResourceVariables *mrv);

  /// @brief Tear down the interpreter and forget the model, leaving the owned arena untouched.
  void release_model_();

  const tflite::Model *tflite_model_{nullptr};
  size_t model_length_{0};
  std::unique_ptr<tflite::MicroInterpreter> interpreter_;
  std::unique_ptr<tflite::MicroMutableOpResolver<TFLITE_MICRO_MAX_OPERATORS>> resolver_;
  /// Arena owned by load_model(); empty when the caller supplies the arena.
  tflite_memory_handler::MemoryManager::AllocationResult tensor_arena_allocation_;
  uint8_t *tensor_arena_{nullptr};
  size_t tensor_arena_size_{0};
  size_t tensor_arena_size_requested_{0};
  ModelConfig config_;
  tflite_output_processor::OutputProcessing output_processing_{
      tflite_output_processor::OutputProcessing::OUTPUT_PROCESSING_ARGMAX};
  int output_size_{0};
  bool debug_{false};
};

}  // namespace esphome::tflite_model_loader

#endif  // USE_TFLITE_MICRO_HELPER
