#include "sendspin_generic_image.h"

#if defined(USE_ESP32) && defined(USE_SENDSPIN_ARTWORK)

#include "esphome/core/log.h"

namespace esphome::sendspin_ {

static const char *const TAG = "sendspin.generic_image";

SendspinImage::SendspinImage(int fixed_width, int fixed_height, runtime_image::ImageFormat format,
                             image::ImageType type, image::Transparency transparency, bool is_big_endian,
                             image::Image *placeholder)
    : runtime_image::RuntimeImage(format, type, transparency, placeholder, is_big_endian, fixed_width, fixed_height) {}

// THREAD CONTEXT: Main loop. The decode callback registered below fires on the artwork
// decode thread; the display and clear callbacks fire on the main loop.
void SendspinImage::setup() {
  this->parent_->add_image_decode_callback(
      [this](uint8_t slot, const uint8_t *data, size_t length, sendspin::SendspinImageFormat format) {
        if (slot == this->slot_)
          this->on_decode_(data, length);
      });
  this->parent_->add_image_display_callback([this](uint8_t slot) {
    if (slot == this->slot_)
      this->on_display_();
  });
  this->parent_->add_image_clear_callback([this](uint8_t slot) {
    if (slot == this->slot_)
      this->on_clear_();
  });
}

// THREAD CONTEXT: Dedicated artwork decode thread (via SendspinHub's decode callback).
// Decode synchronously into the back buffer; heavy CPU work is allowed here.
void SendspinImage::on_decode_(const uint8_t *data, size_t length) {
  this->begin_decode(length);
  size_t total_consumed = 0;
  while (total_consumed < length) {
    int consumed = this->feed_data(const_cast<uint8_t *>(data) + total_consumed, length - total_consumed);
    if (consumed < 0) {
      ESP_LOGE(TAG, "Error decoding image data at offset %zu", total_consumed);
      this->image_error_callback_.call();
      return;
    }
    total_consumed += consumed;
  }
  if (!this->end_decode()) {
    ESP_LOGE(TAG, "Failed to finalize image after decoding");
    this->image_error_callback_.call();
    return;
  }
}

// THREAD CONTEXT: Main loop (fired once the server display timestamp is reached)
void SendspinImage::on_display_() { this->image_display_callback_.call(); }

// THREAD CONTEXT: Main loop
void SendspinImage::on_clear_() {
  this->release();
  this->image_display_callback_.call();
}

}  // namespace esphome::sendspin_

#endif
