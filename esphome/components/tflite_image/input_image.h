#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <cstddef>
#include <cstdint>

#include "esphome/components/tflite_micro_runtime/tflite_micro_runtime.h"

namespace esphome::tflite_image {

enum class ChannelOrder : uint8_t {
  CHANNEL_ORDER_RGB,
  CHANNEL_ORDER_BGR,
  CHANNEL_ORDER_GRAY,
};

/// @brief Fills an image model's input tensor from an RGB888 frame.
class ImageInputPipeline final : public tflite_micro_runtime::InputPipeline {
 public:
  void set_geometry(int width, int height, int channels) {
    this->width_ = width;
    this->height_ = height;
    this->channels_ = channels;
  }
  /// Size of the incoming frame when it differs from the model's input; the
  /// pipeline scales it with nearest-neighbour sampling.
  void set_frame_geometry(int width, int height) {
    this->frame_width_ = width;
    this->frame_height_ = height;
  }
  void set_channel_order(ChannelOrder order) { this->channel_order_ = order; }
  void set_normalize(bool normalize) { this->normalize_ = normalize; }
  void set_invert(bool invert) { this->invert_ = invert; }

  bool fill(TfLiteTensor *input, const uint8_t *src, size_t size) override;
  const char *name() const override { return "image"; }

 protected:
  int width_{0};
  int height_{0};
  int channels_{0};
  int frame_width_{0};   // 0 = the frame matches the model's input size
  int frame_height_{0};  // 0 = the frame matches the model's input size
  ChannelOrder channel_order_{ChannelOrder::CHANNEL_ORDER_RGB};
  bool normalize_{false};
  bool invert_{false};
};

}  // namespace esphome::tflite_image

#endif  // USE_TFLITE_MICRO_HELPER
