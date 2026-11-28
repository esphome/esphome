#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include "esphome/components/tflite_model_loader/engine.h"
#include "esphome/components/tflite_output_processor/output_processor.h"
#include "tensorflow/lite/c/common.h"

namespace tflite {
class MicroResourceVariables;
}  // namespace tflite

namespace esphome::tflite_model_loader {

struct ModelConfig {
  std::string description;
  std::string output_processing;
  float scale_factor{1.0f};
};
struct ImageModelConfig {};

struct ModelLoader {
  static constexpr size_t DEFAULT_TENSOR_ARENA_SIZE = 1024;
};

/// One uint8 input of four bytes and one output, whose value counts the invocations.
class ModelHandler {
 public:
  void set_debug(bool) {}
  bool verify_model_crc(const uint8_t *, size_t, uint32_t) { return true; }
  size_t probe_arena_size(const uint8_t *, size_t, size_t seed, tflite::MicroResourceVariables *) { return seed; }
  bool load_model_with_arena(const uint8_t *, size_t, uint8_t *, size_t, const ModelConfig &config,
                             tflite::MicroResourceVariables *) {
    this->config_ = config;
    return tflite_runtime_testing::engine.load_succeeds;
  }
  void unload() { ++tflite_runtime_testing::engine.unloads; }
  TfLiteStatus invoke() {
    auto &engine = tflite_runtime_testing::engine;
    this->output_value_ = static_cast<float>(++engine.invocations);
    this->output_.type = engine.output_type;
    return engine.invoke_status;
  }
  size_t input_count() const { return 1; }
  size_t output_count() const { return 1; }
  TfLiteTensor *input(size_t) { return &this->input_; }
  TfLiteTensor *output(size_t) { return &this->output_; }
  TfLiteTensor *input_tensor() { return &this->input_; }
  const TfLiteTensor *input_tensor() const { return &this->input_; }
  TfLiteTensor *output_tensor() { return &this->output_; }
  tflite_output_processor::ProcessedOutput process_output(const TfLiteTensor *) const {
    return {this->output_value_, 1.0f, true};
  }
  ModelConfig get_config() const { return this->config_; }
  int get_input_width() const { return 4; }
  int get_input_height() const { return 1; }
  int get_input_channels() const { return 1; }
  size_t get_arena_used_bytes() const { return 128; }

 protected:
  ModelConfig config_;
  uint8_t input_bytes_[4]{};
  float output_value_{0.0f};
  TfLiteTensor input_{kTfLiteUInt8, {reinterpret_cast<char *>(this->input_bytes_)}, sizeof(this->input_bytes_)};
  TfLiteTensor output_{kTfLiteFloat32, {reinterpret_cast<char *>(&this->output_value_)}, sizeof(float)};
};

}  // namespace esphome::tflite_model_loader
