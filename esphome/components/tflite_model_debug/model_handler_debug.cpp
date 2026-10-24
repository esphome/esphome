// Out-of-line definitions of ModelHandler debug methods. The class lives in the
// tflite_model_loader namespace (declared in its model_handler.h), and both
// sides are gated on USE_TFLITE_MODEL_DEBUG: the declaration in that header and
// the definitions here, which this component's to_code emits.
// namespace esphome::tflite_model_debug

#include "esphome/core/defines.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include "esphome/components/tflite_model_loader/model_handler.h"
#include "esphome/core/log.h"

#ifdef USE_TFLITE_MODEL_DEBUG

#include "debug_utils.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
#include "esp_task_wdt.h"

namespace esphome::tflite_model_loader {

using tflite_model_debug::debug_utils::tflite_type_to_string;

ESPHOME_LOG_TAG(TAG, "tflite_model_debug");

void ModelHandler::debug_input_quantization_analysis(const uint8_t *input_data, size_t input_size,
                                                     const std::string &stage) {
  TfLiteTensor *input = this->input_tensor();
  if (!input || !input_data || input_size == 0)
    return;

  ESP_LOGI(TAG, "=== INPUT QUANTIZATION ANALYSIS: %s ===", stage.c_str());
  ESP_LOGI(TAG, "Input quantization: scale=%.6f, zp=%d", input->params.scale, input->params.zero_point);
  ESP_LOGI(TAG, "Input size: %zu bytes", input_size);

  // Analyze input data statistics
  if (input->type == kTfLiteUInt8) {
    uint8_t min_val = 255;
    uint8_t max_val = 0;
    uint32_t sum = 0;
    int zero_count = 0;

    for (size_t i = 0; i < input_size; i++) {
      min_val = std::min(min_val, input_data[i]);
      max_val = std::max(max_val, input_data[i]);
      sum += input_data[i];
      if (input_data[i] == 0)
        zero_count++;
    }

    float mean = static_cast<float>(sum) / input_size;
    ESP_LOGI(TAG, "Input data stats: min=%u, max=%u, mean=%.1f", min_val, max_val, mean);
    ESP_LOGI(TAG, "Zero values: %d/%zu (%.1f%%)", zero_count, input_size, (zero_count * 100.0f) / input_size);

    // Informational only: 1/255 with zero point 0 is the usual full-range uint8
    // image quantization, but the model's own scale is what applies, so a
    // different one is not a problem to warn about.
    const bool full_range = std::fabs(input->params.scale - 1.0f / 255.0f) < 0.0001f && input->params.zero_point == 0;
    ESP_LOGI(TAG, "Full-range uint8 quantization (scale=1/255, zp=0): %s",
             full_range ? LOG_STR_LITERAL("yes") : LOG_STR_LITERAL("no"));
  }
  ESP_LOGI(TAG, "=== END INPUT ANALYSIS ===");
}

void ModelHandler::debug_input_tensor_details() {
  TfLiteTensor *input = this->input_tensor();
  if (!input)
    return;

  ESP_LOGI(TAG, "=== INPUT TENSOR DETAILS ===");
  ESP_LOGI(TAG, "Type: %s", tflite_type_to_string(input->type));
  ESP_LOGI(TAG, "Dimensions: %d", input->dims->size);
  for (int i = 0; i < input->dims->size; i++) {
    ESP_LOGI(TAG, "  dim[%d]: %d", i, input->dims->data[i]);
  }
  ESP_LOGI(TAG, "Bytes: %zu", input->bytes);
  ESP_LOGI(TAG, "Quantization: scale=%.6f, zero_point=%d", input->params.scale, input->params.zero_point);
  ESP_LOGI(TAG, "=== END TENSOR DETAILS ===");
}

void ModelHandler::debug_tensor_types() {
  TfLiteTensor *input = this->input_tensor();
  TfLiteTensor *output = this->output_tensor();

  ESP_LOGI(TAG, "=== TENSOR TYPE VERIFICATION ===");
  if (input) {
    ESP_LOGI(TAG, "INPUT: type=%s, scale=%.6f, zp=%d", tflite_type_to_string(input->type), input->params.scale,
             input->params.zero_point);
  }
  if (output) {
    ESP_LOGI(TAG, "OUTPUT: type=%s, scale=%.6f, zp=%d", tflite_type_to_string(output->type), output->params.scale,
             output->params.zero_point);
  }
}

void ModelHandler::debug_input_data_stats(const uint8_t *input_data, size_t input_size) const {
  ESP_LOGI(TAG, "=== INPUT DATA STATISTICS ===");
  if (!input_data || input_size == 0)
    return;

  uint32_t sum = 0;
  uint8_t min_val = 255;
  uint8_t max_val = 0;
  int zero_count = 0;

  for (size_t i = 0; i < input_size; i++) {
    uint8_t val = input_data[i];
    sum += val;
    if (val < min_val)
      min_val = val;
    if (val > max_val)
      max_val = val;
    if (val == 0)
      zero_count++;
  }

  ESP_LOGI(TAG, "Range: [%u, %u], Mean: %.2f, Zeros: %d (%.1f%%)", min_val, max_val,
           static_cast<float>(sum) / input_size, zero_count, (zero_count * 100.0f) / input_size);
}

void ModelHandler::debug_quantized_input_details(TfLiteTensor *input, size_t input_size) const {
  if (!input || input->type != kTfLiteUInt8)
    return;
  ESP_LOGI(TAG, "First 10 input values (q -> deq):");
  for (size_t i = 0; i < 10 && i < input_size; i++) {
    uint8_t q = input->data.uint8[i];
    float deq = (q - input->params.zero_point) * input->params.scale;
    ESP_LOGI(TAG, "  [%zu]: %u -> %.6f", i, q, deq);
  }
}

void ModelHandler::debug_int8_conversion_details(TfLiteTensor *input, const uint8_t *input_data,
                                                 size_t input_size) const {
  if (!input || input->type != kTfLiteInt8)
    return;
  ESP_LOGI(TAG, "First 10 INT8 conversions:");
  for (size_t i = 0; i < 10 && i < input_size; i++) {
    uint8_t orig = input_data[i];
    int8_t conv = input->data.int8[i];
    float deq = (conv - input->params.zero_point) * input->params.scale;
    ESP_LOGI(TAG, "  [%zu]: %u -> %d -> %.6f", i, orig, conv, deq);
  }
}

void ModelHandler::debug_pre_inference_state() {
  TfLiteTensor *input = this->input_tensor();
  if (input) {
    ESP_LOGI(TAG, "Pre-inference: Input type %s, bytes %zu", tflite_type_to_string(input->type), input->bytes);
  }
}

void ModelHandler::debug_output_tensor_details(TfLiteTensor *output) const {
  if (!output)
    return;
  ESP_LOGI(TAG, "=== OUTPUT TENSOR DETAILS ===");
  ESP_LOGI(TAG, "Type: %s, Scale: %.6f, ZP: %d, Size: %d", tflite_type_to_string(output->type), output->params.scale,
           output->params.zero_point, this->output_size());
}

void ModelHandler::debug_raw_outputs(TfLiteTensor *output) const {
  ESP_LOGI(TAG, "=== RAW OUTPUTS ===");
  if (output->type == kTfLiteUInt8) {
    for (int i = 0; i < this->output_size() && i < 10; i++) {
      ESP_LOGI(TAG, "  [%d]: %u", i, output->data.uint8[i]);
    }
  } else if (output->type == kTfLiteFloat32) {
    for (int i = 0; i < this->output_size() && i < 10; i++) {
      ESP_LOGI(TAG, "  [%d]: %.6f", i, output->data.f[i]);
    }
  }
}

void ModelHandler::debug_qat_model_output() {
  // Basic implementation for QAT debugging
  TfLiteTensor *output = this->output_tensor();
  if (!output)
    return;
  this->debug_raw_outputs(output);
}

std::vector<ImageModelConfig> ModelHandler::generate_debug_configs() const {
  // The candidates come from the loaded model: an image model only accepts the
  // input size it was converted with, so those are the only sizes worth testing.
  // The axes that can still be wrong are the channel order, whether the model
  // wants normalized input and its input type.
  const int width = this->get_input_width();
  const int height = this->get_input_height();
  const int channels = this->get_input_channels();
  if (width <= 0 || height <= 0 || channels <= 0)
    return {};

  const std::vector<std::string> input_orders = {"RGB", "BGR"};
  const std::vector<bool> normalize_options = {true, false};
  const std::vector<std::string> input_types = {"float32", "uint8"};

  std::vector<ImageModelConfig> configs;
  for (const auto &order : input_orders) {
    for (bool norm : normalize_options) {
      for (const auto &type : input_types) {
        ImageModelConfig cfg;
        cfg.input_order = order;
        cfg.input_size.assign({width, height});
        cfg.normalize = norm;
        cfg.input_type = type;
        cfg.input_channels = channels;
        configs.push_back(cfg);
      }
    }
  }
  return configs;
}

bool ModelHandler::invoke_model(const uint8_t *data, size_t len) {
  if (this->interpreter() == nullptr)
    return false;
  TfLiteTensor *input = this->input_tensor();
  if (input == nullptr || data == nullptr)
    return false;

  const int width = this->get_input_width();
  const int height = this->get_input_height();
  const int channels = this->get_input_channels();
  if (width <= 0 || height <= 0 || channels <= 0 || channels > 3)
    return false;

  // The zone buffer is RGB888: three bytes per pixel, in RGB order.
  const size_t pixel_count = static_cast<size_t>(width) * height;
  if (len < pixel_count * 3)
    return false;

  const ImageModelConfig &config = this->image_config_;
  const bool swap_red_blue = config.input_order == "BGR" && channels == 3;
  const float value_scale = config.normalize ? 1.0f / 255.0f : 1.0f;

  // Source channel for one output channel: RGB order, red/blue swapped for BGR,
  // and the green channel when the model wants a single grayscale channel.
  auto source_channel = [&](int channel) -> int {
    if (channels == 1)
      return 1;
    return swap_red_blue ? 2 - channel : channel;
  };
  auto pixel_value = [&](size_t pixel, int channel) -> float {
    return static_cast<float>(data[pixel * 3 + source_channel(channel)]) * value_scale;
  };

  if (input->type == kTfLiteFloat32) {
    float *out = input->data.f;
    if (out == nullptr)
      return false;
    for (size_t pixel = 0; pixel < pixel_count; pixel++)
      for (int channel = 0; channel < channels; channel++)
        out[pixel * channels + channel] = pixel_value(pixel, channel);
  } else if (input->type == kTfLiteUInt8) {
    uint8_t *out = input->data.uint8;
    if (out == nullptr)
      return false;
    for (size_t pixel = 0; pixel < pixel_count; pixel++)
      for (int channel = 0; channel < channels; channel++)
        out[pixel * channels + channel] = static_cast<uint8_t>(pixel_value(pixel, channel));
  } else if (input->type == kTfLiteInt8) {
    int8_t *out = input->data.int8;
    if (out == nullptr)
      return false;
    const float scale = input->params.scale != 0.0f ? input->params.scale : 1.0f;
    const int32_t zero_point = input->params.zero_point;
    for (size_t pixel = 0; pixel < pixel_count; pixel++) {
      for (int channel = 0; channel < channels; channel++) {
        const float quantized = std::round(pixel_value(pixel, channel) / scale) + zero_point;
        out[pixel * channels + channel] = static_cast<int8_t>(std::clamp(quantized, -128.0f, 127.0f));
      }
    }
  } else {
    ESP_LOGW(TAG, "Unsupported input type for the parameter sweep: %s", tflite_type_to_string(input->type));
    return false;
  }

  return this->interpreter()->Invoke() == kTfLiteOk;
}

void ModelHandler::test_configuration(const ImageModelConfig &config,
                                      const std::vector<std::vector<uint8_t>> &zone_data,
                                      std::vector<ConfigTestResult> &results) {
  ESP_LOGI(TAG, "Testing Config: Order=%s Size=%dx%d Norm=%d Type=%s", config.input_order.c_str(), config.input_size[0],
           config.input_size[1], config.normalize, config.input_type.c_str());

  // Only test if input size matches model expectation
  if (this->get_input_width() != config.input_size[0] || this->get_input_height() != config.input_size[1]) {
    ESP_LOGD(TAG, "Skipping config due to size mismatch with loaded model");
    return;
  }

  // Apply the candidate: invoke_model() converts the zone with the active config,
  // so without this every candidate would run the same conversion.
  this->set_image_config(config);

  ConfigTestResult res;
  res.config = config;
  res.avg_confidence = 0.0f;

  float total_conf = 0;
  int success = 0;

  for (const auto &zone : zone_data) {
    if (!this->invoke_model(zone.data(), zone.size()))
      continue;
    TfLiteTensor *output = this->output_tensor();
    if (output == nullptr)
      continue;
    // The tensor overload of process_output() dequantizes uint8/int8 outputs, so
    // a fully quantized model is scored like a float one.
    const ProcessedOutput out = this->process_output(output);
    res.zone_confidences.push_back(out.confidence);
    res.zone_values.push_back(out.value);
    total_conf += out.confidence;
    success++;
  }

  if (success > 0)
    res.avg_confidence = total_conf / success;
  results.push_back(res);
}

void ModelHandler::debug_test_parameters(const std::vector<std::vector<uint8_t>> &zone_data) {
  ESP_LOGI(TAG, "=== DEBUG PARAMETER TESTING ===");
  ModelHandler::feed_watchdog();

  // test_configuration() applies each candidate, so the active configuration has
  // to be put back when the sweep is done.
  const ImageModelConfig original = this->get_image_config();

  std::vector<ConfigTestResult> results;
  auto configs = this->generate_debug_configs();

  int i = 0;
  for (const auto &cfg : configs) {
    this->test_configuration(cfg, zone_data, results);
    if (++i % 5 == 0)
      ModelHandler::feed_watchdog();
  }

  this->set_image_config(original);

  std::sort(results.begin(), results.end(),
            [](const ConfigTestResult &a, const ConfigTestResult &b) { return a.avg_confidence > b.avg_confidence; });

  ESP_LOGI(TAG, "=== TOP CONFIGURATIONS ===");
  for (size_t j = 0; j < std::min(static_cast<size_t>(10), results.size()); j++) {
    const auto &r = results[j];
    ESP_LOGI(TAG, "#%zu: Conf=%.4f [%s %dx%d %s]", j + 1, r.avg_confidence, r.config.input_order.c_str(),
             r.config.input_size[0], r.config.input_size[1], r.config.input_type.c_str());
  }
}

void ModelHandler::feed_watchdog() {
  esp_task_wdt_reset();
  ESP_LOGV(TAG, "Watchdog fed");
}

void ModelHandler::log_input_stats() const {
  const TfLiteTensor *input = this->input_tensor();
  if (input == nullptr)
    return;

  // Bound the sample by what the tensor actually holds: the configured
  // dimensions are exactly what is wrong when someone reaches for these logs.
  const int element_size = input->type == kTfLiteFloat32 ? sizeof(float) : sizeof(uint8_t);
  const int element_count = input->bytes / element_size;
  const int sample_size = std::min(20, element_count);  // Show first 20 values
  if (sample_size <= 0)
    return;

  ESP_LOGD(TAG, "First %d %s inputs (%s):", sample_size,
           input->type == kTfLiteFloat32 ? LOG_STR_LITERAL("float32") : LOG_STR_LITERAL("uint8"),
           this->image_config_.normalize ? LOG_STR_LITERAL("normalized") : LOG_STR_LITERAL("raw"));

  if (input->type == kTfLiteFloat32) {
    const float *data = input->data.f;
    for (int i = 0; i < sample_size; i++) {
      ESP_LOGD(TAG, "  [%d]: %.4f", i, data[i]);
      // Log channel groups for RGB/BGR
      if (this->image_config_.input_channels >= 3 && i % this->image_config_.input_channels == 2) {
        ESP_LOGD(TAG, "    -> %s: [%.3f, %.3f, %.3f]", this->image_config_.input_order.c_str(), data[i - 2],
                 data[i - 1], data[i]);
      }
    }
  } else {
    const uint8_t *data = input->data.uint8;
    for (int i = 0; i < sample_size; i++) {
      ESP_LOGD(TAG, "  [%d]: %u", i, data[i]);
      // Log channel groups for RGB/BGR
      if (this->image_config_.input_channels >= 3 && i % this->image_config_.input_channels == 2) {
        ESP_LOGD(TAG, "    -> %s: [%u, %u, %u]", this->image_config_.input_order.c_str(), data[i - 2], data[i - 1],
                 data[i]);
      }
    }
  }
}

void ModelHandler::debug_input_pattern() const {
  const TfLiteTensor *input = this->input_tensor();
  if (!input || input->type != kTfLiteFloat32)
    return;

  const float *data = input->data.f;
  const int total_elements = input->bytes / sizeof(float);
  const int height = this->get_input_height();
  const int width = this->get_input_width();
  // A model can have more channels than an RGB image, so the per-channel arrays
  // are clamped to what a colour image needs and the read is bounded by the
  // buffer instead of assuming three channels.
  static constexpr int MAX_CHANNELS = 4;
  const int channels = std::min(this->get_input_channels(), MAX_CHANNELS);
  if (channels <= 0 || width <= 0 || height <= 0)
    return;

  ESP_LOGD(TAG, "Input pattern analysis: %dx%dx%d", width, height, channels);

  // Check if this looks like a proper image (not all zeros or uniform)
  float channel_sums[MAX_CHANNELS] = {};
  float channel_mins[MAX_CHANNELS];
  float channel_maxs[MAX_CHANNELS];
  for (int c = 0; c < MAX_CHANNELS; c++) {
    channel_mins[c] = std::numeric_limits<float>::max();
    channel_maxs[c] = std::numeric_limits<float>::lowest();
  }
  int pixel_count = 0;

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      const int pos = (y * width + x) * channels;
      if (pos + channels > total_elements)
        continue;
      for (int c = 0; c < channels; c++) {
        const float val = data[pos + c];
        channel_sums[c] += val;
        channel_mins[c] = std::min(channel_mins[c], val);
        channel_maxs[c] = std::max(channel_maxs[c], val);
      }
      pixel_count++;
    }
  }

  if (pixel_count > 0) {
    ESP_LOGD(TAG, "Channel statistics:");
    for (int c = 0; c < channels; c++) {
      float mean = channel_sums[c] / pixel_count;
      ESP_LOGD(TAG, "  Channel %d: min=%.3f, max=%.3f, mean=%.3f", c, channel_mins[c], channel_maxs[c], mean);
    }
  }

  // Check for common preprocessing patterns
  bool looks_normalized_0_1 = true;
  bool looks_normalized_neg1_1 = true;

  for (int i = 0; i < total_elements; i++) {
    if (data[i] < 0.0f || data[i] > 1.0f)
      looks_normalized_0_1 = false;
    if (data[i] < -1.0f || data[i] > 1.0f)
      looks_normalized_neg1_1 = false;
  }

  ESP_LOGD(TAG, "Data range analysis:");
  ESP_LOGD(TAG, "  Looks like 0-1 normalized: %s",
           looks_normalized_0_1 ? LOG_STR_LITERAL("YES") : LOG_STR_LITERAL("NO"));
  ESP_LOGD(TAG, "  Looks like -1 to 1 normalized: %s",
           looks_normalized_neg1_1 ? LOG_STR_LITERAL("YES") : LOG_STR_LITERAL("NO"));
}

void ModelHandler::debug_model_architecture() const {
  const tflite::Model *model = this->loader_.tflite_model();
  if (!model)
    return;

  auto *subgraphs = model->subgraphs();
  if (!subgraphs)
    return;

  ESP_LOGD(TAG, "Model Architecture:");
  ESP_LOGD(TAG, "  Subgraphs: %zu", static_cast<size_t>(subgraphs->size()));

  for (size_t i = 0; i < subgraphs->size(); i++) {
    auto *subgraph = subgraphs->Get(i);
    ESP_LOGD(TAG, "  Subgraph %zu:", i);
    ESP_LOGD(TAG, "    Tensors: %zu", static_cast<size_t>(subgraph->tensors()->size()));
    ESP_LOGD(TAG, "    Operators: %zu", static_cast<size_t>(subgraph->operators()->size()));
    ESP_LOGD(TAG, "    Inputs: %zu", static_cast<size_t>(subgraph->inputs()->size()));
    ESP_LOGD(TAG, "    Outputs: %zu", static_cast<size_t>(subgraph->outputs()->size()));
  }
}

}  // namespace esphome::tflite_model_loader

#endif  // USE_TFLITE_MODEL_DEBUG

#endif  // USE_TFLITE_MICRO_HELPER
