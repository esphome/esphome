#include "tflite_waveform.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <cstring>

#include "esphome/core/log.h"

namespace esphome::tflite_waveform {

ESPHOME_LOG_TAG(TAG, "tflite_waveform");

bool WaveformInputPipeline::fill(TfLiteTensor *input, const uint8_t *src, size_t size) {
  if (input == nullptr || input->data.raw == nullptr || src == nullptr) {
    ESP_LOGE(TAG, "fill called without a tensor or samples");
    return false;
  }
  const bool is_float = input->type == kTfLiteFloat32;
  if (!is_float && input->type != kTfLiteInt8 && input->type != kTfLiteUInt8) {
    ESP_LOGE(TAG, "Unsupported input tensor type: %d", input->type);
    return false;
  }
  if (!is_float && input->params.scale == 0.0f) {
    ESP_LOGE(TAG, "The quantized input tensor has no scale");
    return false;
  }
  const size_t samples = is_float ? input->bytes / sizeof(float) : input->bytes;
  if (size != samples * sizeof(int16_t)) {
    ESP_LOGE(TAG, "The frame is %zu bytes, the model wants %zu", size, samples * sizeof(int16_t));
    return false;
  }
  for (size_t i = 0; i < samples; i++) {
    int16_t sample;
    // The source buffer need not be aligned.
    memcpy(&sample, src + i * sizeof(int16_t), sizeof(int16_t));
    const float value = pcm_to_float(sample);
    if (is_float) {
      input->data.f[i] = value;
    } else if (input->type == kTfLiteInt8) {
      input->data.int8[i] =
          static_cast<int8_t>(quantize(value, input->params.scale, input->params.zero_point, -128, 127));
    } else {
      input->data.uint8[i] =
          static_cast<uint8_t>(quantize(value, input->params.scale, input->params.zero_point, 0, 255));
    }
  }
  return true;
}

bool WaveformDecoder::decode(TfLiteTensor *const *outputs, size_t count) {
  this->count_ = 0;
  if (count != 1) {
    ESP_LOGE(TAG, "A waveform model needs 1 output, this one has %zu", count);
    return false;
  }
  const TfLiteTensor *tensor = outputs[0];
  if (tensor->type != kTfLiteFloat32 && tensor->type != kTfLiteInt8 && tensor->type != kTfLiteUInt8) {
    ESP_LOGE(TAG, "Unsupported output tensor type: %d", tensor->type);
    return false;
  }
  const size_t samples = tensor->type == kTfLiteFloat32 ? tensor->bytes / sizeof(float) : tensor->bytes;
  if (samples == 0 || samples > this->capacity_) {
    ESP_LOGE(TAG, "The output has %zu samples, the buffer holds %zu", samples, this->capacity_);
    return false;
  }
  for (size_t i = 0; i < samples; i++) {
    float value;
    if (!tflite_output_processor::read_tensor_value(tensor, i, &value)) {
      ESP_LOGE(TAG, "Sample %zu could not be read", i);
      return false;
    }
    this->samples_[i] = float_to_pcm(value);
  }
  this->count_ = samples;
  return true;
}

}  // namespace esphome::tflite_waveform

#endif  // USE_TFLITE_MICRO_HELPER
