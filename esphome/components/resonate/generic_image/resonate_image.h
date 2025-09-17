#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP_IDF) && defined(USE_RESONATE_IMAGE)

#include "esphome/components/resonate/resonate_hub.h"
#include "esphome/components/resonate/resonate_protocol.h"
#include "esphome/components/runtime_image/runtime_image.h"
#include "esphome/core/automation.h"

#include "esphome/core/component.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace esphome {
namespace resonate {

class ResonateImage : public Component, public runtime_image::RuntimeImage, public Parented<ResonateHub> {
 public:
  ResonateImage(int fixed_width, int fixed_height, runtime_image::ImageFormat format, image::ImageType type,
                image::Transparency transparency, bool is_big_endian = false, image::Image *placeholder = nullptr);

  void setup() override;

  // Callback methods for triggers
  void add_on_image_received_callback(std::function<void()> &&callback);
  void add_on_image_decoded_callback(std::function<void()> &&callback);
  void add_on_image_error_callback(std::function<void()> &&callback);

 protected:
  static void decode_task(void *params);

  CallbackManager<void()> image_received_callback_{};
  CallbackManager<void()> image_decoded_callback_{};
  CallbackManager<void()> image_error_callback_{};

  std::vector<uint8_t, RAMAllocator<uint8_t>> encoded_data_;
  ResonateImageFormat resonate_format_;

  TaskHandle_t decode_task_handle_{nullptr};
};

// Automation trigger classes
class ResonateImageReceivedTrigger : public Trigger<> {
 public:
  explicit ResonateImageReceivedTrigger(ResonateImage *parent) {
    parent->add_on_image_received_callback([this]() { this->trigger(); });
  }
};

class ResonateImageDecodedTrigger : public Trigger<> {
 public:
  explicit ResonateImageDecodedTrigger(ResonateImage *parent) {
    parent->add_on_image_decoded_callback([this]() { this->trigger(); });
  }
};

class ResonateImageErrorTrigger : public Trigger<> {
 public:
  explicit ResonateImageErrorTrigger(ResonateImage *parent) {
    parent->add_on_image_error_callback([this]() { this->trigger(); });
  }
};

}  // namespace resonate
}  // namespace esphome
#endif
