#include "input_image.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include "esphome/core/log.h"

namespace esphome::tflite_image {

static const char *const TAG = "tflite_image";

namespace {
/// A pixel after the channel order, normalization and inversion are applied.
float pixel_value(uint8_t value, bool normalize, bool invert) {
  float scaled = normalize ? static_cast<float>(value) / 255.0f : static_cast<float>(value);
  if (!invert)
    return scaled;
  return normalize ? 1.0f - scaled : 255.0f - scaled;
}
}  // namespace

bool ImageInputPipeline::fill(TfLiteTensor *input, const uint8_t *src, size_t size) {
  if (input == nullptr || src == nullptr) {
    ESP_LOGE(TAG, "fill called without a tensor or a frame");
    return false;
  }
  if (this->width_ <= 0 || this->height_ <= 0 || this->channels_ < 1 || this->channels_ > 3) {
    ESP_LOGE(TAG, "Unusable geometry %dx%dx%d", this->width_, this->height_, this->channels_);
    return false;
  }
  const size_t pixels = static_cast<size_t>(this->width_) * this->height_;
  if (size < pixels * 3) {
    ESP_LOGE(TAG, "Frame is %zu bytes, shorter than the %zu an %dx%d RGB888 frame needs", size, pixels * 3,
             this->width_, this->height_);
    return false;
  }
  const bool gray = this->channels_ == 1 || this->channel_order_ == ChannelOrder::CHANNEL_ORDER_GRAY;
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
        dst[pixel] = pixel_value(rgb[1], this->normalize_, this->invert_);
      } else {
        for (int channel = 0; channel < 3; channel++) {
          const int source = swap ? 2 - channel : channel;
          dst[pixel * 3 + channel] = pixel_value(rgb[source], this->normalize_, this->invert_);
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
        dst[pixel] = static_cast<uint8_t>(pixel_value(rgb[1], this->normalize_, this->invert_));
      } else {
        for (int channel = 0; channel < 3; channel++) {
          const int source = swap ? 2 - channel : channel;
          dst[pixel * 3 + channel] = static_cast<uint8_t>(pixel_value(rgb[source], this->normalize_, this->invert_));
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
