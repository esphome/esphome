#include "output_processor.h"

#include "esphome/core/log.h"

#include <algorithm>
#include <cmath>

namespace esphome::tflite_output_processor {

ESPHOME_LOG_TAG(TAG, "output_processor");

namespace {

struct OutputStats {
  float min_value;
  float max_value;
  float sum;
  int max_index;
  int non_zero_count;
};

/// Single pass over the output: range, winning index, sum and non-zero count.
/// `value_at` returns the already dequantized value of one element.
template<typename ValueAt> OutputStats compute_stats(int num_classes, const ValueAt &value_at) {
  const float first = value_at(0);
  OutputStats stats{first, first, 0.0f, 0, 0};
  for (int i = 0; i < num_classes; i++) {
    const float value = value_at(i);
    if (value < stats.min_value)
      stats.min_value = value;
    if (value > stats.max_value) {
      stats.max_value = value;
      stats.max_index = i;
    }
    stats.sum += value;
    if (std::fabs(value) > 1e-6f)
      stats.non_zero_count++;
  }
  return stats;
}

/// Probability of the winning class after softmax. Softmax keeps the argmax, so the winner is
/// already known and only its probability has to be computed.
template<typename ValueAt> float winning_probability(int num_classes, float max_value, const ValueAt &value_at) {
  float sum = 0.0f;
  for (int i = 0; i < num_classes; i++)
    sum += expf(value_at(i) - max_value);
  return sum > 0.0f ? 1.0f / sum : 0.0f;
}

/// Shared implementation for float output buffers and dequantized integer tensors.
template<typename ValueAt>
ProcessedOutput process_values(int num_classes, OutputProcessing mode, float scale_factor, bool debug,
                               const ValueAt &value_at) {
  const OutputStats stats = compute_stats(num_classes, value_at);

  if (debug) {
    ESP_LOGD(TAG, "Output range: min=%.6f, max=%.6f, sum=%.6f", stats.min_value, stats.max_value, stats.sum);
  }

  float confidence = 0.0f;

  switch (mode) {
    case OutputProcessing::OUTPUT_PROCESSING_ARGMAX:
      // The tensor already holds scores or probabilities, so the winning entry is the confidence.
      confidence = stats.max_value;
      break;
    case OutputProcessing::OUTPUT_PROCESSING_SOFTMAX:
      // Logits: softmax does not change the winning class, so only its probability is needed.
      confidence = winning_probability(num_classes, stats.max_value, value_at);
      break;
    case OutputProcessing::OUTPUT_PROCESSING_QAT_QUANTIZED:
      // Quantization-aware trained models apply softmax, so this already is a probability.
      confidence = std::clamp(stats.max_value, 0.0f, 1.0f);
      break;
    case OutputProcessing::OUTPUT_PROCESSING_AUTO_DETECT: {
      const bool looks_like_probabilities =
          stats.min_value >= 0.0f && stats.max_value <= 1.0f && std::fabs(stats.sum - 1.0f) <= 0.05f;
      // A single non-zero element above 0.5 is not logits and does not sum to 1.
      const bool looks_like_one_hot = stats.non_zero_count == 1 && stats.max_value >= 0.5f;
      if (debug) {
        ESP_LOGD(TAG, "Auto-detect: non-zero=%d probabilities=%s one_hot=%s", stats.non_zero_count,
                 looks_like_probabilities ? LOG_STR_LITERAL("yes") : LOG_STR_LITERAL("no"),
                 looks_like_one_hot ? LOG_STR_LITERAL("yes") : LOG_STR_LITERAL("no"));
      }
      if (looks_like_probabilities) {
        // An exact one-hot vector ({0, 1, 0}) sums to 1 and lands here as well.
        confidence = stats.max_value;
      } else if (looks_like_one_hot) {
        // Exactly one non-zero element, so the winner is that element. Its value is all the
        // information the model gave about how sure it is.
        confidence = std::clamp(stats.max_value, 0.0f, 1.0f);
      } else {
        confidence = winning_probability(num_classes, stats.max_value, value_at);
      }
      break;
    }
  }

  float value = static_cast<float>(stats.max_index);
  // The class index is scaled in every mode. Configuration validation rejects this, but a caller
  // can still pass it, and dividing by zero would report inf.
  if (!(scale_factor > 0.0f))
    scale_factor = 1.0f;
  value /= scale_factor;

  if (debug) {
    ESP_LOGD(TAG, "Processed output: value=%.4f confidence=%.6f", value, confidence);
  }

  return {value, confidence};
}

}  // namespace

ProcessedOutput process_output(const float *output_data, int num_classes, OutputProcessing mode, float scale_factor,
                               bool debug) {
  if (output_data == nullptr) {
    ESP_LOGE(TAG, "Null output data pointer");
    return {0.0f, 0.0f};
  }
  if (num_classes <= 0) {
    ESP_LOGE(TAG, "Invalid number of output classes: %d", num_classes);
    return {0.0f, 0.0f};
  }
  return process_values(num_classes, mode, scale_factor, debug,
                        [output_data](int i) -> float { return output_data[i]; });
}

#ifdef USE_TFLITE_MICRO_HELPER

namespace {

/// Product of the tensor dimensions; 0 when the tensor has no usable dimensions.
int tensor_element_count(const TfLiteTensor *tensor) {
  if (tensor->dims == nullptr || tensor->dims->size <= 0)
    return 0;
  int count = 1;
  for (int i = 0; i < tensor->dims->size; i++) {
    if (tensor->dims->data[i] <= 0)
      return 0;
    count *= tensor->dims->data[i];
  }
  return count;
}

}  // namespace

ProcessedOutput process_output(TfLiteTensor *output_tensor, OutputProcessing mode, float scale_factor, bool debug) {
  if (output_tensor == nullptr) {
    ESP_LOGE(TAG, "Null output tensor");
    return {0.0f, 0.0f};
  }

  const int count = tensor_element_count(output_tensor);
  if (count <= 0) {
    ESP_LOGE(TAG, "Output tensor has no usable dimensions");
    return {0.0f, 0.0f};
  }

  if (output_tensor->type == kTfLiteFloat32) {
    if (output_tensor->data.f == nullptr) {
      ESP_LOGE(TAG, "Output tensor has no data");
      return {0.0f, 0.0f};
    }
    return process_output(output_tensor->data.f, count, mode, scale_factor, debug);
  }

  if (output_tensor->type != kTfLiteUInt8 && output_tensor->type != kTfLiteInt8) {
    ESP_LOGE(TAG, "Unsupported output tensor type: %d", output_tensor->type);
    return {0.0f, 0.0f};
  }

  float scale = output_tensor->params.scale;
  const int32_t zero_point = output_tensor->params.zero_point;
  if (scale == 0.0f) {
    scale = 1.0f;
    ESP_LOGW(TAG, "Output tensor has scale 0.0, assuming 1.0 -- probabilities may be garbage");
  }

  if (output_tensor->type == kTfLiteUInt8) {
    const uint8_t *data = output_tensor->data.uint8;
    if (data == nullptr) {
      ESP_LOGE(TAG, "Output tensor has no data");
      return {0.0f, 0.0f};
    }
    return process_values(count, mode, scale_factor, debug,
                          [data, zero_point, scale](int i) -> float { return (data[i] - zero_point) * scale; });
  }

  const int8_t *data = output_tensor->data.int8;
  if (data == nullptr) {
    ESP_LOGE(TAG, "Output tensor has no data");
    return {0.0f, 0.0f};
  }
  return process_values(count, mode, scale_factor, debug,
                        [data, zero_point, scale](int i) -> float { return (data[i] - zero_point) * scale; });
}

#endif  // USE_TFLITE_MICRO_HELPER

}  // namespace esphome::tflite_output_processor
