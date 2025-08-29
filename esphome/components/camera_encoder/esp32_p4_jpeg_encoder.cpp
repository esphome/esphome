#ifdef USE_P4_CAMERA_JPEG_ENCODER

#include "esp32_p4_jpeg_encoder.h"

namespace esphome {
namespace camera_encoder {

#ifdef USE_ESP32_VARIANT_ESP32P4
#endif

static const char *const TAG = "camera_encoder";

ESP32P4JPEGEncoder::ESP32P4JPEGEncoder(uint8_t quality, camera::EncoderSubsampling subsampling,
                                       camera::EncoderBuffer *output) {
  this->quality_ = quality;
  this->subsampling_ = subsampling;
  this->output_ = output;

  jpeg_encode_engine_cfg_t encode_eng_cfg = {
      .intr_priority = 0,
      .timeout_ms = 100,
  };

  ESP_ERROR_CHECK(jpeg_new_encoder_engine(&encode_eng_cfg, &this->encoder_engine_));
}

camera::EncoderError ESP32P4JPEGEncoder::encode_pixels(camera::CameraImageSpec *spec, camera::Buffer *pixels) {
  uint8_t *buffer = this->output_->get_data();
  size_t buffer_length = this->output_->get_max_size();
  uint32_t bytes_written = 0;

  jpeg_encode_cfg_t enc_config = {
      .height = spec->height,
      .width = spec->width,
      .src_type = this->to_internal_(spec->format),
      .sub_sample = this->to_internal_(this->subsampling_),
      .image_quality = this->quality_,
  };

  esp_err_t error = jpeg_encoder_process(this->encoder_engine_, &enc_config, pixels->get_data_buffer(),
                                         pixels->get_data_length(), buffer, buffer_length, &bytes_written);
  this->output_->set_buffer_size(bytes_written);
  if (error != ESP_OK)
    return camera::ENCODER_ERROR_CONFIGURATION;

  return camera::ENCODER_ERROR_SUCCESS;
}

void ESP32P4JPEGEncoder::dump_config() {
  ESP_LOGCONFIG(TAG,
                "ESP32-P4 JPEG Encoder:\n"
                "  Size: %d\n"
                "  Quality: %d\n"
                "  %s\n",
                this->output_->get_max_size(), this->quality_, to_string(this->subsampling_));
}

jpeg_enc_input_format_t ESP32P4JPEGEncoder::to_internal_(camera::ImageFormat format) {
  switch (format) {
    case camera::IMAGE_FORMAT_GRAYSCALE:
      return JPEG_ENCODE_IN_FORMAT_GRAY;
    case camera::IMAGE_FORMAT_RGB565:
      return JPEG_ENCODE_IN_FORMAT_RGB565;
    // Internal representation for RGB is in byte order: B, G, R
    case camera::IMAGE_FORMAT_BGR888:
      return JPEG_ENCODE_IN_FORMAT_RGB888;
  }

  return JPEG_ENCODE_IN_FORMAT_GRAY;
}

jpeg_down_sampling_type_t ESP32P4JPEGEncoder::to_internal_(camera::EncoderSubsampling sampling) {
  switch (sampling) {
    case camera::SUBSAMPLING_444:
      return JPEG_DOWN_SAMPLING_YUV444;
    case camera::SUBSAMPLING_422:
      return JPEG_DOWN_SAMPLING_YUV422;
    case camera::SUBSAMPLING_420:
      return JPEG_DOWN_SAMPLING_YUV420;
  }

  return JPEG_DOWN_SAMPLING_YUV444;
}

}  // namespace camera_encoder
}  // namespace esphome

#endif
