#include "esp32_camera_jpeg_decoder.h"

#ifdef USE_CAMERA_DECODER

#include "esphome/core/log.h"

#include <climits>
#include <utility>

namespace esphome::camera_decoder {

ESPHOME_LOG_TAG(TAG, "camera_decoder");

namespace {

/// Maps the configured rotation to the library's enum once, at construction.
jpeg_rotate_t rotation_to_jpeg(int degrees) {
  switch (degrees) {
    case 90:
      return JPEG_ROTATE_90D;
    case 180:
      return JPEG_ROTATE_180D;
    case 270:
      return JPEG_ROTATE_270D;
    default:
      return JPEG_ROTATE_0D;
  }
}

}  // namespace

JpegBuffer::JpegBuffer(size_t capacity) : capacity_(capacity) {
  this->data_ = static_cast<uint8_t *>(jpeg_calloc_align(capacity, 16));
}

JpegBuffer::~JpegBuffer() {
  if (this->data_ != nullptr) {
    jpeg_free_align(this->data_);
  }
}

ESP32CameraJPEGDecoder::ESP32CameraJPEGDecoder(camera::PixelFormat output_format, int rotation)
    : output_format_(output_format), rotation_(rotation_to_jpeg(rotation)) {}

ESP32CameraJPEGDecoder::~ESP32CameraJPEGDecoder() {
  if (this->decoder_ != nullptr) {
    jpeg_dec_close(this->decoder_);
  }
}

bool ESP32CameraJPEGDecoder::open_decoder_() {
  jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
  switch (this->output_format_) {
    case camera::PIXEL_FORMAT_RGB565:
      // Big-endian, matching the layout esp32-camera produces for RGB565.
      config.output_type = JPEG_PIXEL_FORMAT_RGB565_BE;
      break;
    case camera::PIXEL_FORMAT_CBYCRY:
      config.output_type = JPEG_PIXEL_FORMAT_CbYCrY;
      break;
    default:
      config.output_type = JPEG_PIXEL_FORMAT_RGB888;
      break;
  }
  config.block_enable = false;
  config.rotate = this->rotation_;

  if (jpeg_dec_open(&config, &this->decoder_) != JPEG_ERR_OK) {
    this->decoder_ = nullptr;
    ESP_LOGE(TAG, "Failed to open the JPEG decoder");
    return false;
  }
  return true;
}

camera::DecoderError ESP32CameraJPEGDecoder::decode(const uint8_t *data, size_t length, camera::CameraImageSpec *spec) {
  // Nothing is published until this frame has been decoded successfully.
  this->has_output_ = false;

  if (data == nullptr || length == 0 || spec == nullptr) {
    return camera::DECODER_ERROR_FAILED;
  }
  if (length > static_cast<size_t>(INT_MAX)) {
    ESP_LOGE(TAG, "JPEG input is too large: %zu bytes", length);
    return camera::DECODER_ERROR_FAILED;
  }

  if (this->decoder_ == nullptr && !this->open_decoder_()) {
    return camera::DECODER_ERROR_FAILED;
  }

  jpeg_dec_io_t io{};
  io.inbuf = const_cast<uint8_t *>(data);
  io.inbuf_len = static_cast<int>(length);
  io.inbuf_remain = static_cast<int>(length);

  jpeg_dec_header_info_t header{};
  if (jpeg_dec_parse_header(this->decoder_, &io, &header) != JPEG_ERR_OK) {
    ESP_LOGE(TAG, "Failed to parse the JPEG header");
    return camera::DECODER_ERROR_FAILED;
  }

  int output_length = 0;
  if (jpeg_dec_get_outbuf_len(this->decoder_, &output_length) != JPEG_ERR_OK || output_length <= 0) {
    ESP_LOGE(TAG, "Failed to size the JPEG output buffer");
    return camera::DECODER_ERROR_FAILED;
  }

  if (this->output_ == nullptr || this->output_->capacity() < static_cast<size_t>(output_length)) {
    // Let the old buffer go first: a bigger frame should not need both in PSRAM
    // at the same time.
    this->output_.reset();
    this->output_ = std::make_unique<JpegBuffer>(static_cast<size_t>(output_length));
    if (!this->output_->valid()) {
      ESP_LOGE(TAG, "Failed to allocate %d bytes for the JPEG output", output_length);
      this->output_.reset();
      return camera::DECODER_ERROR_OUT_OF_MEMORY;
    }
  }

  io.outbuf = this->output_->get_data_buffer();
  io.out_size = output_length;

  if (jpeg_dec_process(this->decoder_, &io) != JPEG_ERR_OK) {
    ESP_LOGE(TAG, "JPEG decoding failed");
    return camera::DECODER_ERROR_FAILED;
  }

  // The buffer becomes readable only now that it holds a complete frame.
  this->output_->set_length(static_cast<size_t>(output_length));
  this->has_output_ = true;

  spec->width = header.width;
  spec->height = header.height;
  spec->format = this->output_format_;
  if (this->rotation_ == JPEG_ROTATE_90D || this->rotation_ == JPEG_ROTATE_270D) {
    std::swap(spec->width, spec->height);
  }

  return camera::DECODER_ERROR_SUCCESS;
}

}  // namespace esphome::camera_decoder

#endif  // USE_CAMERA_DECODER
