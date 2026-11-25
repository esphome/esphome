#include "input_image.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "esphome/core/log.h"

namespace esphome::tflite_image {

ESPHOME_LOG_TAG(TAG, "tflite_image");

namespace {
/// A pixel value after inversion, still on the 0-255 scale.
float unit_value(uint8_t value, bool invert) {
  return invert ? 255.0f - static_cast<float>(value) : static_cast<float>(value);
}

/// Gray level of an RGB888 pixel (Rec. 601 weights, integer arithmetic).
uint8_t luminance(const uint8_t *rgb) {
  return static_cast<uint8_t>((77 * rgb[0] + 150 * rgb[1] + 29 * rgb[2] + 128) >> 8);
}

/// A quantized tensor stores scaled values, so 0-1 normalized input must be scaled back.
/// The result is clamped to the range of the tensor's integer type.
int quantized_value(const TfLiteTensor *input, float unit, bool normalize, int min_value, int max_value) {
  const float scaled = normalize ? unit / 255.0f : unit;
  const float scale = input->params.scale != 0.0f ? input->params.scale : 1.0f;
  const int value = static_cast<int>(std::lround(scaled / scale)) + input->params.zero_point;
  return std::clamp(value, min_value, max_value);
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
    ESP_LOGE(TAG, "The input tensor is %zu bytes, the configured %dx%dx%d needs %zu", static_cast<size_t>(input->bytes),
             this->width_, this->height_, this->channels_, needed);
    return false;
  }
  const int frame_width = this->frame_width_ > 0 ? this->frame_width_ : this->width_;
  const int frame_height = this->frame_height_ > 0 ? this->frame_height_ : this->height_;
  const size_t frame_pixels = static_cast<size_t>(frame_width) * frame_height;
  if (size < frame_pixels * 3) {
    ESP_LOGE(TAG, "Frame is %zu bytes, shorter than the %zu an %dx%d RGB888 frame needs", size, frame_pixels * 3,
             frame_width, frame_height);
    return false;
  }
  const bool scaled = frame_width != this->width_ || frame_height != this->height_;
  // Nearest-neighbour sampling of the source frame into the model's input size.
  auto source_pixel = [&](size_t pixel) -> size_t {
    if (!scaled)
      return pixel;
    const int x = pixel % this->width_;
    const int y = pixel / this->width_;
    return static_cast<size_t>(y * frame_height / this->height_) * frame_width +
           static_cast<size_t>(x * frame_width / this->width_);
  };
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
      const uint8_t *rgb = src + source_pixel(pixel) * 3;
      if (gray) {
        const float unit = unit_value(luminance(rgb), this->invert_);
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
      const uint8_t *rgb = src + source_pixel(pixel) * 3;
      if (gray) {
        dst[pixel] = static_cast<uint8_t>(
            quantized_value(input, unit_value(luminance(rgb), this->invert_), this->normalize_, 0, 255));
      } else {
        for (int channel = 0; channel < 3; channel++) {
          const int source = swap ? 2 - channel : channel;
          dst[pixel * 3 + channel] = static_cast<uint8_t>(
              quantized_value(input, unit_value(rgb[source], this->invert_), this->normalize_, 0, 255));
        }
      }
    }
    return true;
  }

  if (input->type == kTfLiteInt8) {
    int8_t *dst = input->data.int8;
    if (dst == nullptr) {
      ESP_LOGE(TAG, "Input tensor has no data");
      return false;
    }
    for (size_t pixel = 0; pixel < pixels; pixel++) {
      const uint8_t *rgb = src + source_pixel(pixel) * 3;
      if (gray) {
        dst[pixel] = static_cast<int8_t>(
            quantized_value(input, unit_value(luminance(rgb), this->invert_), this->normalize_, INT8_MIN, INT8_MAX));
      } else {
        for (int channel = 0; channel < 3; channel++) {
          const int source = swap ? 2 - channel : channel;
          dst[pixel * 3 + channel] = static_cast<int8_t>(
              quantized_value(input, unit_value(rgb[source], this->invert_), this->normalize_, INT8_MIN, INT8_MAX));
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
