#pragma once

#include "esphome/core/defines.h"

#include <cstdint>

#ifdef USE_TFLITE_MICRO_HELPER
#include "tensorflow/lite/c/common.h"
#endif

namespace esphome::tflite_output_processor {

/// @brief How a model output tensor is turned into a (value, confidence) pair.
enum class OutputProcessing : uint8_t {
  /// The tensor already holds scores or probabilities; the winning entry is the confidence.
  OUTPUT_PROCESSING_DIRECT_CLASS,
  /// Same as OUTPUT_PROCESSING_DIRECT_CLASS.
  OUTPUT_PROCESSING_ARGMAX,
  /// The tensor holds logits; the confidence is the softmax probability of the winner.
  OUTPUT_PROCESSING_SOFTMAX,
  /// Logits as well: softmax does not change the winning class, so this matches SOFTMAX.
  OUTPUT_PROCESSING_LOGITS,
  /// Dequantized probabilities; the confidence is the winning probability.
  OUTPUT_PROCESSING_QAT_QUANTIZED,
  /// Softmax over logits multiplied by a fixed scale, which softens the distribution.
  OUTPUT_PROCESSING_EXPERIMENTAL_SCALE,
  /// Legacy alias of OUTPUT_PROCESSING_LOGITS.
  OUTPUT_PROCESSING_LOGITS_JOMJOL,
  /// Legacy alias of OUTPUT_PROCESSING_SOFTMAX.
  OUTPUT_PROCESSING_SOFTMAX_JOMJOL,
  /// Derive the shape of the output from the data itself.
  OUTPUT_PROCESSING_AUTO_DETECT,
};

/// @brief Result of model output processing.
struct ProcessedOutput {
  float value;       // winning class index divided by scale_factor, in every mode
  float confidence;  // winning probability, or the winning tensor entry for argmax
};

/// @brief Process a raw float32 output buffer.
///
/// The reported class index is divided by @p scale_factor in every mode; a value of zero or less
/// is treated as 1.0 so the result is never inf. The confidence is the winning probability for the
/// softmax based modes and the winning tensor entry itself for DIRECT_CLASS/ARGMAX, so it is only
/// bounded to [0, 1] when the model output already is a probability.
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
#endif

}  // namespace esphome::tflite_output_processor
