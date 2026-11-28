#include "esp32_camera_jpeg_decoder.h"

#ifdef USE_CAMERA_DECODER

#include "esphome/core/log.h"

#include "jpeg_grayscale.h"

#include <climits>
#include <utility>

namespace esphome::camera_decoder {

ESPHOME_LOG_TAG(TAG, "camera_decoder");

namespace {

/// The decoder writes into a 16-byte aligned block.
constexpr size_t JPEG_BUFFER_ALIGNMENT = 16;

/// esp_new_jpeg rotates only frames whose width and height are multiples of this many pixels.
constexpr unsigned JPEG_ROTATION_BLOCK = 8;

/// Maps the configured rotation to the library's enum when the decoder opens.
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
  this->data_ = static_cast<uint8_t *>(jpeg_calloc_align(capacity, JPEG_BUFFER_ALIGNMENT));
}

JpegBuffer::~JpegBuffer() {
  if (this->data_ != nullptr) {
    jpeg_free_align(this->data_);
  }
}

ESP32CameraJPEGDecoder::ESP32CameraJPEGDecoder(camera::PixelFormat output_format, uint16_t rotation, size_t preallocate)
    : output_format_(output_format), rotation_degrees_(rotation) {
  if (preallocate == 0)
    return;
  this->output_ = std::make_unique<JpegBuffer>(preallocate);
  if (!this->output_->valid()) {
    // The first frame retries the allocation and reports the failure.
    ESP_LOGW(TAG, "Failed to reserve %zu bytes for the JPEG output", preallocate);
    this->output_.reset();
  }
}

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
    case camera::PIXEL_FORMAT_RGB888:
      config.output_type = JPEG_PIXEL_FORMAT_RGB888;
      break;
    // Every enumerator is listed and there is no default, so -Wswitch reports a
    // PixelFormat that grows without this mapping being updated. BGR888 has no
    // layout the library takes, so it fails the open instead of being decoded
    // as a different format.
    case camera::PIXEL_FORMAT_BGR888:
      ESP_LOGE(TAG, "Cannot decode to %s", camera::to_string(this->output_format_));
      this->open_failed_ = true;
      return false;
  }
  config.block_enable = false;
  config.rotate = rotation_to_jpeg(this->rotation_degrees_);

  // Open into a local handle: an open that fails must not leave a half-initialized
  // handle in the member, and a decoder that cannot open is not retried per frame.
  jpeg_dec_handle_t decoder = nullptr;
  if (jpeg_dec_open(&config, &decoder) != JPEG_ERR_OK) {
    this->open_failed_ = true;
    ESP_LOGE(TAG, "Failed to open the JPEG decoder");
    return false;
  }
  this->decoder_ = decoder;
  return true;
}

camera::DecoderError ESP32CameraJPEGDecoder::decode(const uint8_t *data, size_t length, camera::CameraImageSpec *spec) {
  // Nothing is published until this frame has been decoded successfully.
  if (this->output_ != nullptr) {
    this->output_->set_length(0);
  }

  if (data == nullptr || length == 0 || spec == nullptr) {
    ESP_LOGE(TAG, "decode() needs input data and an output spec");
    return camera::DECODER_ERROR_FAILED;
  }
  if (length > static_cast<size_t>(INT_MAX)) {
    ESP_LOGE(TAG, "JPEG input is too large: %zu bytes", length);
    return camera::DECODER_ERROR_FAILED;
  }

  if (this->decoder_ == nullptr && (this->open_failed_ || !this->open_decoder_())) {
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
  // esp_new_jpeg rotates only when both dimensions are multiples of 8. Report
  // that here instead of letting the decode fail with a generic error after the
  // output buffer has been allocated.
  if (this->rotation_degrees_ != 0 &&
      (header.width % JPEG_ROTATION_BLOCK != 0 || header.height % JPEG_ROTATION_BLOCK != 0)) {
    ESP_LOGE(TAG, "Cannot rotate a %ux%u JPEG: both dimensions must be multiples of %u",
             static_cast<unsigned>(header.width), static_cast<unsigned>(header.height), JPEG_ROTATION_BLOCK);
    return camera::DECODER_ERROR_UNSUPPORTED;
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
    // still sized for RGB888, so peak RAM is 3x the grayscale frame size.
    const uint64_t pixel_count = static_cast<uint64_t>(header.width) * static_cast<uint64_t>(header.height);
    if (pixel_count == 0 || pixel_count * RGB888_BYTES_PER_PIXEL > static_cast<uint64_t>(output_bytes)) {
      ESP_LOGE(TAG, "Decoded image does not fit in the JPEG output buffer");
      return camera::DECODER_ERROR_FAILED;
    }
    rgb888_to_grayscale_in_place(this->output_->get_data_buffer(), static_cast<size_t>(pixel_count));
    output_bytes = static_cast<size_t>(pixel_count);
  }
  this->output_->set_length(output_bytes);

  spec->width = header.width;
  spec->height = header.height;
  spec->format = this->output_format_;
  if (this->rotation_degrees_ == 90 || this->rotation_degrees_ == 270) {
    std::swap(spec->width, spec->height);
  }

  return camera::DECODER_ERROR_SUCCESS;
}

void ESP32CameraJPEGDecoder::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Camera Decoder:\n"
                "  Output: %s\n"
                "  Rotation: %u",
                camera::to_string(this->output_format_), static_cast<unsigned>(this->rotation_degrees_));
  if (this->output_format_ == camera::PIXEL_FORMAT_GRAYSCALE) {
    ESP_LOGCONFIG(TAG, "  Grayscale: decoded as RGB888 and compacted in place, 3x frame RAM");
  }
}

}  // namespace esphome::camera_decoder

#endif  // USE_CAMERA_DECODER
