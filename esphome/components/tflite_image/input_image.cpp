#include "input_image.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <algorithm>
#include <cmath>

#include "esphome/core/log.h"

namespace esphome::tflite_image {

static const char *const TAG = "tflite_image";

namespace {
/// A pixel value after inversion, still on the 0-255 scale.
float unit_value(uint8_t value, bool invert) {
  return invert ? 255.0f - static_cast<float>(value) : static_cast<float>(value);
}

/// A uint8 tensor stores quantized values, so 0-1 normalized input must be scaled back.
uint8_t quantized_value(const TfLiteTensor *input, float unit, bool normalize) {
  const float scaled = normalize ? unit / 255.0f : unit;
  const float scale = input->params.scale != 0.0f ? input->params.scale : 1.0f;
  const int value = static_cast<int>(std::lround(scaled / scale)) + input->params.zero_point;
  return static_cast<uint8_t>(std::clamp(value, 0, 255));
}
}  // namespace

bool ImageInputPipeline::fill(TfLiteTensor *input, const uint8_t *src, size_t size) {
  if (input == nullptr || src == nullptr) {
    ESP_LOGE(TAG, "fill called without a tensor or a frame");
    return false;
  }
  if (this->width_ <= 0 || this->height_ <= 0 || (this->channels_ != 1 && this->channels_ != 3)) {
    ESP_LOGE(TAG, "Unusable geometry %dx%dx%d", this->width_, this->height_, this->channels_);
    return false;
  }
  const size_t pixels = static_cast<size_t>(this->width_) * this->height_;
  const size_t needed = pixels * this->channels_ * (input->type == kTfLiteFloat32 ? 4 : 1);
  if (static_cast<size_t>(input->bytes) < needed) {
    ESP_LOGE(TAG, "The input tensor is %d bytes, the configured %dx%dx%d needs %zu", input->bytes, this->width_,
             this->height_, this->channels_, needed);
    return false;
  }
  if (size < pixels * 3) {
    ESP_LOGE(TAG, "Frame is %zu bytes, shorter than the %zu an %dx%d RGB888 frame needs", size, pixels * 3,
             this->width_, this->height_);
    return false;
  }
  if (this->channel_order_ == ChannelOrder::CHANNEL_ORDER_GRAY && this->channels_ != 1) {
    ESP_LOGE(TAG, "A GRAY model needs a single-channel tensor");
    return false;
  }
  const bool gray = this->channels_ == 1;
  const bool swap = this->channel_order_ == ChannelOrder::CHANNEL_ORDER_BGR;

  if (input->type == kTfLiteFloat32) {
    float *dst = input->data.f;
    if (dst == nullptr) {
      ESP_LOGE(TAG, "Input tensor has no data");
      return false;
    }
    for (size_t pixel = 0; pixel < pixels; pixel++) {
      const uint8_t *rgb = src + pixel * 3;
      if (gray) {
        const float unit = unit_value(rgb[1], this->invert_);
        dst[pixel] = this->normalize_ ? unit / 255.0f : unit;
      } else {
        for (int channel = 0; channel < 3; channel++) {
          const int source = swap ? 2 - channel : channel;
          const float unit = unit_value(rgb[source], this->invert_);
          dst[pixel * 3 + channel] = this->normalize_ ? unit / 255.0f : unit;
        }
      }
    }
    return true;
  }

  if (input->type == kTfLiteUInt8) {
    uint8_t *dst = input->data.uint8;
    if (dst == nullptr) {
      ESP_LOGE(TAG, "Input tensor has no data");
      return false;
    }
    for (size_t pixel = 0; pixel < pixels; pixel++) {
      const uint8_t *rgb = src + pixel * 3;
      if (gray) {
        dst[pixel] = quantized_value(input, unit_value(rgb[1], this->invert_), this->normalize_);
      } else {
        for (int channel = 0; channel < 3; channel++) {
          const int source = swap ? 2 - channel : channel;
          dst[pixel * 3 + channel] = quantized_value(input, unit_value(rgb[source], this->invert_), this->normalize_);
        }
      }
    }
    return true;
  }

  ESP_LOGE(TAG, "Unsupported input tensor type %d", input->type);
  return false;
}

}  // namespace esphome::tflite_image

#endif  // USE_TFLITE_MICRO_HELPER
