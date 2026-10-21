#include "output_processor.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace esphome::tflite_output_processor {

using tflite_model_loader::ModelConfig;

ESPHOME_LOG_TAG(TAG, "output_processor");

ProcessedOutput process_output(TfLiteTensor *output_tensor, const ModelConfig &config, bool debug) {
  (void) config;
  (void) debug;
  if (!output_tensor) {
    ESP_LOGE(TAG, "Null output tensor");
    return {0.0f, 0.0f};
  }

  if (output_tensor->type == kTfLiteFloat32) {
    // num_classes is derived from the tensor dimensions by the caller path; use the
    // dequantized/float count computed here for the float overload.
    int count = 1;
    for (int i = 0; i < output_tensor->dims->size; i++) {
      count *= output_tensor->dims->data[i];
    }
    return process_output(output_tensor->data.f, count, config, debug);
  } else if (output_tensor->type == kTfLiteUInt8 || output_tensor->type == kTfLiteInt8) {
    // Calculate total elements
    int count = 1;
    for (int i = 0; i < output_tensor->dims->size; i++) {
      count *= output_tensor->dims->data[i];
    }

    // Safety check against buffer overflow if count differs from output_size_
    // Use local vector to ensure bounds safety regardless of output_size_ member state.
    // This ensures dequantization loop is safe.

    std::vector<float> dequantized(count);
    float scale = output_tensor->params.scale;
    int32_t zero_point = output_tensor->params.zero_point;

    if (scale == 0.0f) {
      scale = 1.0f;
      ESP_LOGW(TAG, "Output tensor has scale 0.0, assuming 1.0 -- probabilities may be garbage");
    }

    if (output_tensor->type == kTfLiteUInt8) {
      uint8_t *data = output_tensor->data.uint8;
      for (int i = 0; i < count; i++) {
        dequantized[i] = (data[i] - zero_point) * scale;
      }
    } else {  // Int8
      int8_t *data = output_tensor->data.int8;
      for (int i = 0; i < count; i++) {
        dequantized[i] = (data[i] - zero_point) * scale;
      }
    }

    return process_output(dequantized.data(), count, config, debug);
  } else {
    ESP_LOGE(TAG, "Unsupported output tensor type: %d", output_tensor->type);
    return {0.0f, 0.0f};
  }
}

ProcessedOutput process_output(const float *output_data, int num_classes, const ModelConfig &config, bool debug) {
  ProcessedOutput result = {0.0f, 0.0f};

  if (output_data == nullptr) {
    ESP_LOGE(TAG, "Null output data pointer");
    return result;
  }

  if (num_classes <= 0) {
    ESP_LOGE(TAG, "Invalid number of output classes: %d", num_classes);
    return result;
  }

  // Single-pass statistics calculation
  float min_val = output_data[0];
  float max_val = output_data[0];
  int max_idx = 0;

  for (int i = 1; i < num_classes; i++) {
    float val = output_data[i];
    if (val < min_val)
      min_val = val;
    if (val > max_val) {
      max_val = val;
      max_idx = i;
    }
  }

  float max_val_output = max_val;
  if (debug) {
    ESP_LOGD(TAG, "Output range: min=%.6f, max=%.6f", min_val, max_val);
  }

  if (config.output_processing == "direct_class" || config.output_processing == "argmax") {
    result.value = static_cast<float>(max_idx);
    result.confidence = max_val_output;
    ESP_LOGD(TAG, "Direct class - Value: %.1f, Confidence: %.6f", result.value, result.confidence);

  } else if (config.output_processing == "softmax") {
    float max_logit = *std::max_element(output_data, output_data + num_classes);
    std::vector<float> exp_vals(num_classes);
    float sum = 0.0f;
    for (int i = 0; i < num_classes; i++) {
      exp_vals[i] = expf(output_data[i] - max_logit);
      sum += exp_vals[i];
    }

    int softmax_max_idx = 0;
    float softmax_max_val = 0.0f;
    for (int i = 0; i < num_classes; i++) {
      float prob = exp_vals[i] / sum;
      if (prob > softmax_max_val) {
        softmax_max_val = prob;
        softmax_max_idx = i;
      }
    }

    result.value = static_cast<float>(softmax_max_idx) / config.scale_factor;
    result.confidence = softmax_max_val;
    ESP_LOGD(TAG, "Softmax - Value: %.1f, Confidence: %.6f", result.value, result.confidence);

  } else if (config.output_processing == "logits") {
    result.value = static_cast<float>(max_idx);

    if (min_val >= 0.0f && max_val <= 1.0f) {
      result.confidence = max_val_output;
    } else {
      float confidence_range = max_val - min_val;
      if (confidence_range > 0.001f) {
        result.confidence = (max_val_output - min_val) / confidence_range;
      } else {
        result.confidence = 1.0f;
      }
      result.confidence = std::max(0.0f, std::min(1.0f, result.confidence));
    }

    if (config.scale_factor != 1.0f) {
      result.value = result.value / config.scale_factor;
    }

    ESP_LOGD(TAG, "Logits - Value: %.1f, Raw Max: %.6f, Confidence: %.6f", result.value, max_val_output,
             result.confidence);

  } else if (config.output_processing == "qat_quantized") {
    result.value = static_cast<float>(max_idx);

    if (max_val > min_val) {
      result.confidence = (max_val_output - min_val) / (max_val - min_val);
    } else {
      result.confidence = 1.0f;
    }
    result.confidence = std::max(0.0f, std::min(1.0f, result.confidence));

    if (config.scale_factor != 1.0f) {
      result.value = result.value / config.scale_factor;
    }

    ESP_LOGD(TAG, "QAT Quantized - Value: %.1f, Confidence: %.6f (raw: %.6f)", result.value, result.confidence,
             max_val_output);

  } else if (config.output_processing == "experimental_scale") {
    const float scale_factor = 0.1f;  // tweak if needed
    std::vector<float> scaled(num_classes);
    for (int i = 0; i < num_classes; ++i) {
      scaled[i] = output_data[i] * scale_factor;
    }

    float max_scaled = *std::max_element(scaled.begin(), scaled.end());
    std::vector<float> exp_vals(num_classes);
    float sum = 0.0f;
    for (int i = 0; i < num_classes; ++i) {
      exp_vals[i] = expf(scaled[i] - max_scaled);
      sum += exp_vals[i];
    }

    int exp_max_idx = 0;
    float exp_max_val = 0.0f;
    for (int i = 0; i < num_classes; ++i) {
      float prob = exp_vals[i] / sum;
      if (prob > exp_max_val) {
        exp_max_val = prob;
        exp_max_idx = i;
      }
    }

    result.value = static_cast<float>(exp_max_idx) / config.scale_factor;
    result.confidence = exp_max_val;
    ESP_LOGD(TAG, "Experimental scale - Value: %.1f, Confidence: %.6f", result.value, result.confidence);

  } else if (config.output_processing == "auto_detect") {
    float min_val_ad = output_data[0];
    float max_val_ad = output_data[0];
    float sum_ad = 0.0f;
    int non_zero_ad = 0;

    for (int i = 0; i < num_classes; ++i) {
      float v = output_data[i];
      if (v < min_val_ad)
        min_val_ad = v;
      if (v > max_val_ad)
        max_val_ad = v;
      sum_ad += v;
      if (std::fabs(v) > 1e-6f)
        ++non_zero_ad;
    }

    bool looks_like_probs_ad = (min_val_ad >= 0.0f && max_val_ad <= 1.0f && std::fabs(sum_ad - 1.0f) <= 0.05f);

    bool looks_like_one_hot_ad = (non_zero_ad == 1 && max_val_ad >= 0.5f);
    bool looks_like_logits_ad = !looks_like_probs_ad && !looks_like_one_hot_ad;

    ESP_LOGI(TAG, "AUTO-DETECT: min=%.3f max=%.3f sum=%.3f nz=%d => probs=%s one_hot=%s logits=%s", min_val_ad,
             max_val_ad, sum_ad, non_zero_ad, looks_like_probs_ad ? LOG_STR_LITERAL("YES") : LOG_STR_LITERAL("NO"),
             looks_like_one_hot_ad ? LOG_STR_LITERAL("YES") : LOG_STR_LITERAL("NO"),
             looks_like_logits_ad ? LOG_STR_LITERAL("YES") : LOG_STR_LITERAL("NO"));

    if (looks_like_probs_ad) {
      int max_idx_ad = 0;
      float max_prob_ad = output_data[0];
      for (int i = 1; i < num_classes; ++i) {
        if (output_data[i] > max_prob_ad) {
          max_prob_ad = output_data[i];
          max_idx_ad = i;
        }
      }
      result.value = static_cast<float>(max_idx_ad) / config.scale_factor;
      result.confidence = max_prob_ad;
      ESP_LOGD(TAG, "Auto-detect (softmax): value=%.1f confidence=%.6f", result.value, result.confidence);
    } else if (looks_like_one_hot_ad) {
      int class_id_ad = 0;
      for (int i = 0; i < num_classes; ++i) {
        if (std::fabs(output_data[i]) > 0.5f) {
          class_id_ad = i;
          break;
        }
      }
      result.value = static_cast<float>(class_id_ad) / config.scale_factor;
      result.confidence = 1.0f;
      ESP_LOGD(TAG, "Auto-detect (direct): value=%.1f confidence=1.0", result.value);
    } else {
      float max_logit_ad = *std::max_element(output_data, output_data + num_classes);
      std::vector<float> exp_vals_ad(num_classes);
      float exp_sum_ad = 0.0f;
      for (int i = 0; i < num_classes; ++i) {
        exp_vals_ad[i] = expf(output_data[i] - max_logit_ad);
        exp_sum_ad += exp_vals_ad[i];
      }

      int max_idx_ad = 0;
      float max_prob_ad = exp_vals_ad[0] / exp_sum_ad;
      for (int i = 1; i < num_classes; ++i) {
        float prob = exp_vals_ad[i] / exp_sum_ad;
        if (prob > max_prob_ad) {
          max_prob_ad = prob;
          max_idx_ad = i;
        }
      }

      result.value = static_cast<float>(max_idx_ad) / config.scale_factor;
      result.confidence = max_prob_ad;
      ESP_LOGD(TAG, "Auto-detect (logits): value=%.1f confidence=%.6f", result.value, result.confidence);
    }

  } else {
    ESP_LOGE(TAG, "Unknown output processing method: %s", config.output_processing.c_str());

    result.value = static_cast<float>(max_idx);
    result.confidence = max_val_output;
    ESP_LOGW(TAG, "Fallback to direct classification - Value: %.1f, Confidence: %.6f", result.value, result.confidence);
  }

  return result;
}

}  // namespace esphome::tflite_output_processor
