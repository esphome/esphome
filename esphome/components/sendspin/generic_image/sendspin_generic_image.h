#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_SENDSPIN_ARTWORK)

#include "esphome/components/runtime_image/runtime_image.h"
#include "esphome/components/sendspin/sendspin_hub.h"
#include "esphome/core/automation.h"

#include <sendspin/artwork_role.h>

namespace esphome::sendspin_ {

class SendspinImage : public SendspinChild, public runtime_image::RuntimeImage {
 public:
  SendspinImage(int fixed_width, int fixed_height, runtime_image::ImageFormat format, image::ImageType type,
                image::Transparency transparency, bool is_big_endian = false, image::Image *placeholder = nullptr);

  void setup() override;

  template<typename F> void add_on_image_display_callback(F &&callback) {
    this->image_display_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_image_error_callback(F &&callback) {
    this->image_error_callback_.add(std::forward<F>(callback));
  }

  void set_image_source(sendspin::SendspinImageSource source) { this->source_ = source; }
  void set_slot(uint8_t slot) { this->slot_ = slot; }

 protected:
  // Artwork thread. Decodes encoded bytes synchronously; buffer is valid only for this call.
  void on_decode_(const uint8_t *data, size_t length);
  // Main loop thread. Trigger when art should be displayed.
  void on_display_();
  // Main loop thread. Releases the decoded image and refires the display trigger so listeners re-render.
  void on_clear_();

  LazyCallbackManager<void()> image_display_callback_{};
  LazyCallbackManager<void()> image_error_callback_{};

  sendspin::SendspinImageSource source_{sendspin::SendspinImageSource::ALBUM};
  uint8_t slot_{0};
};

class SendspinImageDisplayTrigger : public Trigger<> {
 public:
  explicit SendspinImageDisplayTrigger(SendspinImage *parent) {
    parent->add_on_image_display_callback([this]() { this->trigger(); });
  }
};

class SendspinImageErrorTrigger : public Trigger<> {
 public:
  explicit SendspinImageErrorTrigger(SendspinImage *parent) {
    parent->add_on_image_error_callback([this]() { this->trigger(); });
  }
};

}  // namespace esphome::sendspin_

#endif
