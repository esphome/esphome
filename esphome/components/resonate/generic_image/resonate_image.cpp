#include "resonate_image.h"

#if defined(USE_ESP_IDF) && defined(USE_RESONATE_IMAGE)

#include "esphome/core/log.h"
#include <utility>

namespace esphome {
namespace resonate {

static const char *const TAG = "resonate.generic_image";

// Stack size for the decode task - TODO: Determine how much stack is actually necessary
static const size_t DECODE_TASK_STACK_SIZE = 8192;
static const UBaseType_t DECODE_TASK_PRIORITY = 1;

ResonateImage::ResonateImage(int fixed_width, int fixed_height, runtime_image::ImageFormat format,
                             image::ImageType type, image::Transparency transparency, bool is_big_endian,
                             image::Image *placeholder)
    : runtime_image::RuntimeImage(format, type, transparency, placeholder, is_big_endian, fixed_width, fixed_height) {}

void ResonateImage::setup() {
  std::string friendly_format;
  switch (this->get_format()) {
    case runtime_image::BMP:
      this->resonate_format_ = RESONATE_IMAGE_BMP;
      friendly_format = "bmp";
      break;
    case runtime_image::JPEG:
      this->resonate_format_ = RESONATE_IMAGE_JPG;
      friendly_format = "jpeg";
      break;
    case runtime_image::PNG:
      this->resonate_format_ = RESONATE_IMAGE_PNG;
      friendly_format = "png";
      break;
  }

  this->parent_->add_image_preferred_format(std::make_pair(
      friendly_format, std::to_string(this->buffer_width_) + "x" + std::to_string(this->buffer_height_)));

  this->parent_->add_image_callback([this](const uint8_t *data, size_t length, ResonateImageFormat format) {
    if ((this->resonate_format_ != format) || (this->encoded_data_.size() > 0)) {
      // Not encoded in this format or we haven't finished ecoding the previous image, return early
      return;
    }

    // Temporarily store encoded image data by copy, image decoders don't work with const pointers
    this->encoded_data_.assign(data, data + length);

    // // this->defer([this]() {
    // this->begin_decode(this->encoded_data_.size());

    // // Trigger the image received callback
    //   this->defer("received_image", [this]() {this->image_received_callback_.call();});

    // // Feed the encoded data to the decoder
    // this->feed_data(this->encoded_data_.data(), this->encoded_data_.size());

    // bool decode_success = this->end_decode();

    // if (!decode_success) {
    //   this->defer("decoded_image", [this]() { this->image_error_callback_.call(); });
    // } else {
    //   this->image_decoded_callback_.call();
    // }

    // // Clear the encoded data after decoding
    // this->encoded_data_.clear();
    // // });

    // Create a FreeRTOS task to decode the image without blocking the main loop
    xTaskCreate(ResonateImage::decode_task, "image_decode", DECODE_TASK_STACK_SIZE, (void *) this, DECODE_TASK_PRIORITY,
                &this->decode_task_handle_);
  });
}

void ResonateImage::add_on_image_received_callback(std::function<void()> &&callback) {
  this->image_received_callback_.add(std::move(callback));
}

void ResonateImage::add_on_image_decoded_callback(std::function<void()> &&callback) {
  this->image_decoded_callback_.add(std::move(callback));
}

void ResonateImage::add_on_image_error_callback(std::function<void()> &&callback) {
  this->image_error_callback_.add(std::move(callback));
}

void ResonateImage::decode_task(void *params) {
  ResonateImage *this_image = (ResonateImage *) params;

  // Trigger the image received callback (thread-safe via defer)
  this_image->defer("image_received", [this_image]() { this_image->image_received_callback_.call(); });

  this_image->release();
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

  // Delete this task
  vTaskDelete(nullptr);
}

}  // namespace resonate
}  // namespace esphome

#endif
