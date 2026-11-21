#pragma once

#include "esphome/core/defines.h"

#include <cstddef>
#include <cstdint>
#include <string>

#ifdef USE_TFLITE_MICRO_HELPER
#include "tensorflow/lite/c/common.h"
#endif

namespace esphome::tflite_output_processor {

/// @brief How a model output tensor is turned into a (value, confidence) pair.
enum class OutputProcessing : uint8_t {
  /// The tensor holds scores or probabilities; the winner is the largest entry and is its own
  /// confidence.
  OUTPUT_PROCESSING_ARGMAX,
  /// The tensor holds logits; the confidence is the softmax probability of the winner.
  OUTPUT_PROCESSING_SOFTMAX,
  /// Dequantized probabilities; the confidence is the winning probability.
  OUTPUT_PROCESSING_QAT_QUANTIZED,
  /// Derive the shape of the output from the data itself.
  OUTPUT_PROCESSING_AUTO_DETECT,
};

/// @brief Result of model output processing.
struct ProcessedOutput {
  float value;       // winning class index divided by scale_factor, in every mode
  float confidence;  // winning probability, or the winning tensor entry for argmax
  bool valid{true};  // false when the tensor could not be read; the figures above are placeholders
};

/// @brief Map a configured mode name to its enum value.
///
/// The YAML configuration and the model report both carry the mode as a name, so
/// the component that holds the model configuration calls this once when the
/// model is loaded and passes the enum to process_output() from then on.
///
/// @param name  One of "argmax", "softmax", "qat_quantized" or "auto_detect".
///              An empty name means the report did not specify one and selects
///              OUTPUT_PROCESSING_ARGMAX without being an error.
/// @param mode  Receives the matching mode, or OUTPUT_PROCESSING_ARGMAX when the
///              name is empty or unknown.
/// @return false only for a non-empty name this component does not know, so the
///         caller can log it; processing still uses OUTPUT_PROCESSING_ARGMAX.
bool parse_output_processing(const std::string &name, OutputProcessing &mode);

/// @brief Process a raw float32 output buffer.
///
/// The reported class index is divided by @p scale_factor in every mode; a value of zero or less
/// is treated as 1.0 so the result is never inf. The confidence is the winning probability for the
/// softmax based modes and the winning tensor entry itself for ARGMAX, so it is only bounded to
/// [0, 1] when the model output already is a probability.
///
/// @param output_data   Model output, @p num_classes elements long.
/// @param num_classes   Number of elements in @p output_data.
/// @param mode          How to interpret the values.
/// @param scale_factor  Divides the reported class index.
/// @param debug         Log the per-inference details.
/// @return The winning class and its confidence; {0.0f, 0.0f} on invalid arguments.
ProcessedOutput process_output(const float *output_data, int num_classes, OutputProcessing mode, float scale_factor,
                               bool debug);

#ifdef USE_TFLITE_MICRO_HELPER
/// @brief Process a model output tensor: dequantizes uint8/int8 tensors, then delegates to the
/// float overload. Returns {0.0f, 0.0f} on error.
ProcessedOutput process_output(TfLiteTensor *output_tensor, OutputProcessing mode, float scale_factor, bool debug);

/// @brief Read one entry of an output tensor as a float, dequantizing int8/uint8.
/// @return false when the tensor or its type cannot be read.
bool read_tensor_value(const TfLiteTensor *tensor, size_t index, float *out);

/// @brief Turns a model's output tensors into a result.
///
/// A model kind implements this when its outputs are not one classification tensor:
/// detection has four, segmentation a mask, pose keypoints, text logits or positions.
class OutputDecoder {
 public:
  virtual ~OutputDecoder() = default;

  /// @param outputs The model's output tensors, in flatbuffer order.
  /// @param count   Number of entries in @p outputs.
  /// @return true when the tensors were read.
  virtual bool decode(TfLiteTensor *const *outputs, size_t count) = 0;
};

/// @brief Classification: the first output tensor, processed as today.
class ClassificationDecoder final : public OutputDecoder {
 public:
  ClassificationDecoder(OutputProcessing mode, float scale_factor, bool debug)
      : mode_(mode), scale_factor_(scale_factor), debug_(debug) {}

  bool decode(TfLiteTensor *const *outputs, size_t count) override {
    if (count == 0)
      return false;
    this->result_ = process_output(outputs[0], this->mode_, this->scale_factor_, this->debug_);
    return this->result_.valid;
  }

  const ProcessedOutput &result() const { return this->result_; }

 private:
  OutputProcessing mode_;
  float scale_factor_;
  bool debug_;
  ProcessedOutput result_{};
};
#endif

}  // namespace esphome::tflite_output_processor
