#include "sendspin_image.h"

#if defined(USE_ESP_IDF) && defined(USE_SENDSPIN_IMAGE)

#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include <utility>

namespace esphome {
namespace sendspin {

static const char *const TAG = "sendspin.generic_image";

// Stack size for the decode task - TODO: Determine how much stack is actually necessary
static const size_t DECODE_TASK_STACK_SIZE = 8192;
static const UBaseType_t DECODE_TASK_PRIORITY = 1;

SendspinImage::SendspinImage(int fixed_width, int fixed_height, runtime_image::ImageFormat format,
                             image::ImageType type, image::Transparency transparency, bool is_big_endian,
                             image::Image *placeholder)
    : runtime_image::RuntimeImage(format, type, transparency, placeholder, is_big_endian, fixed_width, fixed_height) {}

void SendspinImage::setup() {
  // Map runtime_image format to SendspinImageFormat
  switch (this->get_format()) {
    case runtime_image::BMP:
      this->sendspin_format_ = SendspinImageFormat::BMP;
      break;
    case runtime_image::JPEG:
      this->sendspin_format_ = SendspinImageFormat::JPEG;
      break;
    case runtime_image::PNG:
      this->sendspin_format_ = SendspinImageFormat::PNG;
      break;
  }

  // Register image preference with the hub
  ImageSlotPreference preference = {
      .slot = this->slot_,
      .source = this->source_,
      .format = this->sendspin_format_,
      .width = static_cast<uint16_t>(this->fixed_width_),
      .height = static_cast<uint16_t>(this->fixed_height_),
  };
  this->parent_->add_image_preferred_format(preference);

  // Register slot-specific callback
  this->parent_->add_image_slot_callback(
      this->slot_, [this](const uint8_t *data, size_t length, SendspinImageFormat format) {
        if ((this->sendspin_format_ != format) || (this->encoded_data_.size() > 0)) {
          // Not encoded in this format or we haven't finished decoding the previous image, return early
          return;
        }

        if (length == 0) {
          this->release();
        }

        // Temporarily store encoded image data by copy, image decoders don't work with const pointers
        this->encoded_data_.assign(data, data + length);

        // Create a FreeRTOS task to decode the image without blocking the main loop
        xTaskCreate(SendspinImage::decode_task, "image_decode", DECODE_TASK_STACK_SIZE, (void *) this,
                    DECODE_TASK_PRIORITY, &this->decode_task_handle_);
      });
}

void SendspinImage::add_on_image_received_callback(std::function<void()> &&callback) {
  this->image_received_callback_.add(std::move(callback));
}

void SendspinImage::add_on_image_decoded_callback(std::function<void()> &&callback) {
  this->image_decoded_callback_.add(std::move(callback));
}

void SendspinImage::add_on_image_error_callback(std::function<void()> &&callback) {
  this->image_error_callback_.add(std::move(callback));
}

void SendspinImage::decode_task(void *params) {
  SendspinImage *this_image = (SendspinImage *) params;

  // Trigger the image received callback (thread-safe via defer)
  this_image->defer("image_received", [this_image]() { this_image->image_received_callback_.call(); });
  // Wake the main loop immediately to process the deferred callback (~12μs latency vs 0-16ms)
#if defined(USE_SOCKET_SELECT_SUPPORT) && defined(USE_WAKE_LOOP_THREADSAFE)
  App.wake_loop_threadsafe();
#endif

  this_image->begin_decode(this_image->encoded_data_.size());

  // Feed the encoded data to the decoder
  this_image->feed_data(this_image->encoded_data_.data(), this_image->encoded_data_.size());

  bool decode_success = this_image->end_decode();

  // Clear the encoded data after decoding
  this_image->encoded_data_.clear();

  // Trigger appropriate callback based on decode result (thread-safe via defer)
  this_image->defer("image_processed", [this_image, decode_success]() {
    if (!decode_success) {
      this_image->image_error_callback_.call();
    } else {
      this_image->image_decoded_callback_.call();
    }
    this_image->decode_task_handle_ = nullptr;
  });
  // Wake the main loop immediately to process the deferred callback (~12μs latency vs 0-16ms)
#if defined(USE_SOCKET_SELECT_SUPPORT) && defined(USE_WAKE_LOOP_THREADSAFE)
  App.wake_loop_threadsafe();
#endif

  // Delete this task
  vTaskDelete(nullptr);
}

}  // namespace sendspin
}  // namespace esphome

#endif
