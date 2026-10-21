#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_resource_variable.h"
#include "esphome/components/tflite_memory_handler/arena_probe.h"
#include "esphome/components/tflite_memory_handler/memory_manager.h"
#include "esphome/components/tflite_model_loader/crc32.h"
#include "esphome/components/tflite_model_loader/model_loader.h"
#include "esphome/components/tflite_output_processor/output_processor.h"

namespace esphome::tflite_model_loader {

using tflite_memory_handler::MemoryManager;
using tflite_output_processor::ProcessedOutput;

// Image-specific model config (replaces old combined ModelConfig)
struct ImageModelConfig {
  std::string input_type{"uint8"};
  int input_channels{3};
  std::string input_order{"RGB"};
  std::vector<int> input_size;  // [width, height]
  bool normalize{false};
  bool invert{false};
};

// Audio-specific model config (streaming models)
struct AudioModelConfig {
  uint8_t probability_cutoff{128};  // quantized 0-255 (from 0.0-1.0)
  size_t sliding_window_size{10};
  uint8_t features_step_size{20};  // ms between feature slices
  size_t feature_count{40};        // Mel filterbank bins (e.g., PREPROCESSOR_FEATURE_SIZE)
};

#ifdef DEBUG_TFLITE_MICRO_HELPER
struct ConfigTestResult {
  ImageModelConfig config;
  float avg_confidence;
  std::vector<float> zone_confidences;
  std::vector<float> zone_values;
};
#endif

/// @brief Facade over the tflite_* sub-components (model loader, output
/// processor, memory handler) keeping the historical ModelHandler API.
class ModelHandler {
 public:
  // High-level load model that handles memory allocation
  [[nodiscard]] bool load_model(const uint8_t *model_data, size_t model_size, const ModelConfig &config);

  // Unload model and free resources
  void unload();

  // Low-level load model (kept for internal use or specific cases)
  // If mrv is non-null, the interpreter is constructed with MicroResourceVariables
  // for streaming (stateful) inference support.
  [[nodiscard]] bool load_model_with_arena(const uint8_t *model_data, size_t model_size, uint8_t *tensor_arena,
                                           size_t tensor_arena_size, const ModelConfig &config,
                                           tflite::MicroResourceVariables *mrv = nullptr);

  TfLiteStatus invoke() { return this->loader_.invoke(); }

  TfLiteTensor *input_tensor() { return this->loader_.input_tensor(); }

  const TfLiteTensor *input_tensor() const { return this->loader_.input_tensor(); }

  TfLiteTensor *output_tensor() { return this->loader_.output_tensor(); }

  const TfLiteTensor *output_tensor() const { return this->loader_.output_tensor(); }

  /// @brief Get the raw interpreter (nullptr when no model is loaded).
  tflite::MicroInterpreter *interpreter() { return this->loader_.interpreter(); }

  // Helper methods for input dimensions
  int get_input_width() const {
    const TfLiteTensor *input = input_tensor();
    if (!input || input->dims->size < 4)
      return 0;
    return input->dims->data[2];  // [batch, height, width, channels]
  }

  int get_input_height() const {
    const TfLiteTensor *input = input_tensor();
    if (!input || input->dims->size < 4)
      return 0;
    return input->dims->data[1];
  }

  int get_input_channels() const {
    const TfLiteTensor *input = input_tensor();
    if (!input || input->dims->size < 4)
      return 0;
    return input->dims->data[3];
  }

  ProcessedOutput process_output(const float *output_data) const;
  ProcessedOutput process_output(TfLiteTensor *output_tensor) const;

  const ModelConfig &get_config() const { return this->config_; }

  const ImageModelConfig &get_image_config() const { return this->image_config_; }
  void set_image_config(const ImageModelConfig &cfg) { this->image_config_ = cfg; }

  size_t get_arena_used_bytes() const { return this->loader_.get_arena_used_bytes(); }

  // Memory management helpers
  size_t get_tensor_arena_size() const { return this->tensor_arena_allocation_.actual_size; }

  void report_memory_status();

  // Debug helpers
  void log_input_stats() const;
  void debug_input_pattern() const;

  // CRC32 verification
  static uint32_t calculate_crc32(const uint8_t *data, size_t length) {
    return tflite_model_loader::calculate_crc32(data, length);
  }
  bool verify_model_crc(const uint8_t *model_data, size_t length, uint32_t expected_crc = 0) {
    return tflite_model_loader::verify_model_crc(model_data, length, expected_crc);
  }
  void debug_model_architecture() const;

  /// @brief Probes the actual required tensor arena size by trial allocation.
  /// Tries the requested size first, then 1.5x, then 2x if it fails.
  /// On success, binary-searches down to the minimum and returns it (16-byte aligned + padding).
  /// Uses MemoryManager::allocate_tensor_arena() (PSRAM-aware) and builds a local
  /// op resolver from the model's operators. Returns 0 on total failure.
  [[nodiscard]] size_t probe_arena_size(const uint8_t *model_start, size_t initial_size) {
    return tflite_memory_handler::probe_arena_size(model_start, initial_size);
  }

#ifdef DEBUG_TFLITE_MICRO_HELPER
  // Advanced Debugging
  // Note: debug methods that need tensor access are non-const to avoid const_cast
  void debug_input_quantization_analysis(const uint8_t *input_data, size_t input_size, const std::string &stage);
  void debug_input_tensor_details();
  void debug_tensor_types();
  void debug_input_data_stats(const uint8_t *input_data, size_t input_size) const;
  void debug_quantized_input_details(TfLiteTensor *input, size_t input_size) const;
  void debug_int8_conversion_details(TfLiteTensor *input, const uint8_t *input_data, size_t input_size) const;
  void debug_pre_inference_state();
  void debug_output_tensor_details(TfLiteTensor *output) const;
  void debug_raw_outputs(TfLiteTensor *output) const;
  void debug_qat_model_output();

  // Parameter Sweeping / Testing
  std::vector<ImageModelConfig> generate_debug_configs() const;
  void test_configuration(const ImageModelConfig &config, const std::vector<std::vector<uint8_t>> &zone_data,
                          std::vector<ConfigTestResult> &results);
  void debug_test_parameters(const std::vector<std::vector<uint8_t>> &zone_data);
  bool invoke_model(const uint8_t *data, size_t len);  // Helper for tests

  static void feed_watchdog();
#endif  // DEBUG_TFLITE_MICRO_HELPER

  void set_debug(bool debug) {
    this->debug_ = debug;
    this->loader_.set_debug(debug);
  }

 private:
  tflite_model_loader::ModelLoader loader_;
  ModelConfig config_;
  ImageModelConfig image_config_;
  int output_size_{0};

  // Memory management (mirrors the arena owned by the facade or the loader)
  MemoryManager::AllocationResult tensor_arena_allocation_;
  size_t tensor_arena_size_requested_{0};
  size_t model_length_{0};

  bool debug_{false};
};

}  // namespace esphome::tflite_model_loader

#endif  // USE_TFLITE_MICRO_HELPER
