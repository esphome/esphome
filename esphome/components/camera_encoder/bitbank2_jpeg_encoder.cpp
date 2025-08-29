#ifdef USE_BITBANK2_JPEG_ENCODER

#include "bitbank2_jpeg_encoder.h"

namespace esphome {
namespace camera_encoder {

static const char *const TAG = "camera_encoder";

Bitbank2JPEGEncoder::Bitbank2JPEGEncoder(Bitbank2Quality quality, camera::EncoderSubsampling subsampling,
                                         size_t mcu_count, camera::EncoderBuffer *output) {
  this->quality_ = quality;
  this->subsampling_ = subsampling;
  this->mcu_count_ = mcu_count;
  this->output_ = output;
}

camera::EncoderError Bitbank2JPEGEncoder::encode_pixels(camera::CameraImageSpec *spec, camera::Buffer *pixels) {
  uint8_t *buffer = this->output_->get_data();
  size_t buffer_length = this->output_->get_max_size();

  // Starts encoding of a new image.
  if (!this->incremental_) {
    if (encoder_.open(buffer, buffer_length) != JPEGE_SUCCESS)
      return camera::ENCODER_ERROR_CONFIGURATION;

    if (encoder_.encodeBegin(&encoder_state_, spec->width, spec->height, to_internal_(spec->format),
                             to_internal_(this->subsampling_), to_internal_(this->quality_)) != JPEGE_SUCCESS)
      return camera::ENCODER_ERROR_CONFIGURATION;

    this->incremental_ = this->mcu_count_ > 0;
  }

  int bpr = spec->bytes_per_row();
  int bpp = spec->bytes_per_pixel();
  int rc = JPEGE_SUCCESS;
  ssize_t mcus = this->mcu_count_;
  // Encodes until done or until the MCU limit per call is reached.
  while (encoder_state_.y < spec->height && rc == JPEGE_SUCCESS && (this->mcu_count_ == 0 || mcus > 0)) {
    rc = encoder_.addMCU(&encoder_state_,
                         &pixels->get_data_buffer()[(encoder_state_.x * bpp) + (encoder_state_.y * bpr)], bpr);
    --mcus;
  }

  // Out of memory in the output buffer.
  if (rc == JPEGE_NO_BUFFER) {
    this->incremental_ = false;
    if (this->buffer_expand_size_ <= 0)
      return camera::ENCODER_ERROR_SKIP_FRAME;

    size_t current_size = this->output_->get_max_size();
    size_t new_size = this->output_->get_max_size() + this->buffer_expand_size_;
    if (!this->output_->set_buffer_size(new_size)) {
      this->buffer_expand_size_ = 0;
      ESP_LOGE(TAG, "Failed to expand output buffer.");
      return camera::ENCODER_ERROR_SKIP_FRAME;
    }

    ESP_LOGD(TAG, "Output buffer expanded (%u -> %u).", current_size, this->output_->get_max_size());
    return camera::ENCODER_ERROR_RETRY_FRAME;
  }

  if (rc != JPEGE_SUCCESS) {
    this->incremental_ = false;
    return camera::ENCODER_ERROR_SKIP_FRAME;
  }

  if (this->incremental_)
    this->incremental_ = encoder_state_.y < spec->height;

  // MCU limit per call is reached.
  if (this->incremental_)
    return camera::ENCODER_ERROR_RETRY_FRAME;

  // Encoding done.
  size_t written = encoder_.close();
  this->output_->set_buffer_size(written);
  return camera::ENCODER_ERROR_SUCCESS;
}

void Bitbank2JPEGEncoder::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Bitbank2 JPEG Encoder:\n"
                "  Size: %d\n"
                "  %s\n"
                "  %s\n"
                "  Expand: %d\n"
                "  MCUs: %d\n",
                this->output_->get_max_size(), to_string(this->quality_), to_string(this->subsampling_),
                this->buffer_expand_size_, this->mcu_count_);
}

int Bitbank2JPEGEncoder::to_internal_(Bitbank2Quality quality) {
  switch (quality) {
    case QUALITY_BEST:
      return JPEGE_Q_BEST;
    case QUALITY_HIGH:
      return JPEGE_Q_HIGH;
    case QUALITY_MED:
      return JPEGE_Q_MED;
    case QUALITY_LOW:
      return JPEGE_Q_LOW;
  }

  return JPEGE_Q_BEST;
}

int Bitbank2JPEGEncoder::to_internal_(camera::ImageFormat format) {
  switch (format) {
    case camera::IMAGE_FORMAT_GRAYSCALE:
      return JPEGE_PIXEL_GRAYSCALE;
    case camera::IMAGE_FORMAT_RGB565:
      return JPEGE_PIXEL_RGB565;
    case camera::IMAGE_FORMAT_BGR888:
      return JPEGE_PIXEL_BGR888;
  }

  return JPEGE_PIXEL_GRAYSCALE;
}

int Bitbank2JPEGEncoder::to_internal_(camera::EncoderSubsampling sampling) {
  switch (sampling) {
    case camera::SUBSAMPLING_444:
      return JPEGE_SUBSAMPLE_444;
    case camera::SUBSAMPLING_420:
      return JPEGE_SUBSAMPLE_420;
  }

  return JPEGE_SUBSAMPLE_444;
}

}  // namespace camera_encoder
}  // namespace esphome

#endif
