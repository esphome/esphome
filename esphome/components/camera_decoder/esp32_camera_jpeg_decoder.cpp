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

/// Compacts decoded RGB888 pixels to 8-bit luma in place.
///
/// esp_new_jpeg has no one-channel decoder output, so grayscale frames are
/// decoded as RGB888 and reduced here. The integer weights match the
/// conversion used by the camera image processors.
void rgb888_to_grayscale_in_place(uint8_t *buffer, size_t pixels) {
  // esp_jpeg_common.h documents RGB888 as R, G, B from low to high address.
  // dst index i is always <= 3 * i, so a forward pass never reads a pixel it
  // has already overwritten.
  for (size_t i = 0; i < pixels; i++) {
    const uint8_t *pixel = buffer + i * 3;
    buffer[i] = static_cast<uint8_t>((pixel[0] * 77 + pixel[1] * 150 + pixel[2] * 29) >> 8);
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
    case camera::PIXEL_FORMAT_GRAYSCALE:
      // WARNING: esp_new_jpeg has no one-channel decode output:
      // JPEG_PIXEL_FORMAT_GRAY is encoder-only in 1.0.2. The RGB888 frame is
      // compacted to luma in decode(), but the buffer must still hold the full
      // RGB888 image, so peak RAM is 3x the grayscale frame size.
      config.output_type = JPEG_PIXEL_FORMAT_RGB888;
      break;
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
  size_t output_bytes = static_cast<size_t>(output_length);
  if (this->output_format_ == camera::PIXEL_FORMAT_GRAYSCALE) {
    // Compacting in place avoids a second buffer, but the allocation above is
    // still sized for RGB888 (3 bytes per pixel), so peak RAM is 3x the
    // grayscale frame size.
    const uint64_t pixel_count = static_cast<uint64_t>(header.width) * static_cast<uint64_t>(header.height);
    if (pixel_count == 0 || pixel_count * 3 > static_cast<uint64_t>(output_bytes)) {
      ESP_LOGE(TAG, "Decoded image does not fit in the JPEG output buffer");
      return camera::DECODER_ERROR_FAILED;
    }
    rgb888_to_grayscale_in_place(this->output_->get_data_buffer(), static_cast<size_t>(pixel_count));
    output_bytes = static_cast<size_t>(pixel_count);
  }
  this->output_->set_length(output_bytes);
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
