#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_SENDSPIN_ARTWORK)

#include "esphome/components/sendspin/sendspin_hub.h"
#include "esphome/components/sendspin/sendspin_protocol.h"
#include "esphome/components/runtime_image/runtime_image.h"
#include "esphome/core/automation.h"

#include "esphome/core/component.h"

#include <atomic>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace esphome {
namespace sendspin {

class SendspinImage : public Component, public runtime_image::RuntimeImage, public Parented<SendspinHub> {
 public:
  SendspinImage(int fixed_width, int fixed_height, runtime_image::ImageFormat format, image::ImageType type,
                image::Transparency transparency, bool is_big_endian = false, image::Image *placeholder = nullptr);

  void setup() override;

  // Callback methods for triggers
  void add_on_image_received_callback(std::function<void()> &&callback);
  void add_on_image_decoded_callback(std::function<void()> &&callback);
  void add_on_image_error_callback(std::function<void()> &&callback);

  // Setters for image source and slot
  void set_image_source(SendspinImageSource source) { this->source_ = source; }
  void set_slot(uint8_t slot) { this->slot_ = slot; }

 protected:
  static void decode_task(void *params);

  CallbackManager<void()> image_received_callback_{};
  CallbackManager<void()> image_decoded_callback_{};
  CallbackManager<void()> image_error_callback_{};

  std::vector<uint8_t, RAMAllocator<uint8_t>> encoded_data_;
  SendspinImageFormat sendspin_format_;

  SendspinImageSource source_{SendspinImageSource::ALBUM};
  uint8_t slot_{0};

  TaskHandle_t decode_task_handle_{nullptr};
  int64_t server_timestamp_{0};
  std::atomic<bool> decode_active_{false};
};

// Automation trigger classes
class SendspinImageReceivedTrigger : public Trigger<> {
 public:
  explicit SendspinImageReceivedTrigger(SendspinImage *parent) {
    parent->add_on_image_received_callback([this]() { this->trigger(); });
  }
};

class SendspinImageDecodedTrigger : public Trigger<> {
 public:
  explicit SendspinImageDecodedTrigger(SendspinImage *parent) {
    parent->add_on_image_decoded_callback([this]() { this->trigger(); });
  }
};

class SendspinImageErrorTrigger : public Trigger<> {
 public:
  explicit SendspinImageErrorTrigger(SendspinImage *parent) {
    parent->add_on_image_error_callback([this]() { this->trigger(); });
  }
};

}  // namespace sendspin
}  // namespace esphome
#endif
