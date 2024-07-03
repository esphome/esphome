#pragma once

#ifdef USE_ESP_IDF

#include "preprocessor_settings.h"

#include <tensorflow/lite/core/c/common.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/micro/micro_mutable_op_resolver.h>

namespace esphome {
namespace micro_wake_word {

static const uint32_t STREAMING_MODEL_VARIABLE_ARENA_SIZE = 1024;

class StreamingModel {
 public:
  virtual void log_model_config() = 0;
  virtual bool determine_detected() = 0;

  bool perform_streaming_inference(const int8_t features[PREPROCESSOR_FEATURE_SIZE]);

  /// @brief Sets all recent_streaming_probabilities to 0
  void reset_probabilities();

  /// @brief Allocates tensor and variable arenas and sets up the model interpreter
  /// @param op_resolver MicroMutableOpResolver object that must exist until the model is unloaded
  /// @return True if successful, false otherwise
  bool load_model(tflite::MicroMutableOpResolver<20> &op_resolver);

  /// @brief Destroys the TFLite interpreter and frees the tensor and variable arenas' memory
  void unload_model();

 protected:
  uint8_t current_stride_step_{0};

  float probability_cutoff_;
  size_t sliding_window_size_;
  size_t last_n_index_{0};
  size_t tensor_arena_size_;
  std::vector<uint8_t> recent_streaming_probabilities_;

  const uint8_t *model_start_;
  uint8_t *tensor_arena_{nullptr};
  uint8_t *var_arena_{nullptr};
  std::unique_ptr<tflite::MicroInterpreter> interpreter_;
  tflite::MicroResourceVariables *mrv_{nullptr};
  tflite::MicroAllocator *ma_{nullptr};
};

class WakeWordModel final : public StreamingModel {
 public:
  WakeWordModel(const uint8_t *model_start, float probability_cutoff, size_t sliding_window_average_size,
                const std::string &wake_word, size_t tensor_arena_size);

  void log_model_config() override;

  /// @brief Checks for the wake word by comparing the mean probability in the sliding window with the probability
  /// cutoff
  /// @return True if wake word is detected, false otherwise
  bool determine_detected() override;

  const std::string &get_wake_word() const { return this->wake_word_; }

 protected:
  std::string wake_word_;
};

class VADModel final : public StreamingModel {
 public:
  VADModel(const uint8_t *model_start, float probability_cutoff, size_t sliding_window_size, size_t tensor_arena_size);

  void log_model_config() override;

  /// @brief Checks for voice activity by comparing the max probability in the sliding window with the probability
  /// cutoff
  /// @return True if voice activity is detected, false otherwise
  bool determine_detected() override;
};

}  // namespace micro_wake_word
}  // namespace esphome

#endif
