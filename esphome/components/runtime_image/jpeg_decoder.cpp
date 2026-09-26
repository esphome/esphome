#include "jpeg_decoder.h"
#ifdef USE_RUNTIME_IMAGE_JPEG

#include "esphome/components/display/display_buffer.h"
#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP_IDF
#include "esp_task_wdt.h"
#endif

static const char *const TAG = "image_decoder.jpeg";

namespace esphome::runtime_image {

bool JpegDecoder::prepare_rgb565_scaling_(int width, int height) {
  if (this->x_boundaries_.capacity() < static_cast<size_t>(width + 1) && !this->x_boundaries_.try_init(width + 1)) {
    return false;
  }
  if (this->y_boundaries_.capacity() < static_cast<size_t>(height + 1) && !this->y_boundaries_.try_init(height + 1)) {
    return false;
  }

  this->source_width_ = width;
  this->source_height_ = height;
  this->x_boundaries_.clear();
  this->y_boundaries_.clear();
  const uint32_t target_width = this->image_->get_buffer_width();
  const uint32_t target_height = this->image_->get_buffer_height();
  for (uint32_t x = 0; x <= static_cast<uint32_t>(width); x++) {
    this->x_boundaries_.push_back(static_cast<uint16_t>((x * target_width + width - 1) / width));
  }
  for (uint32_t y = 0; y <= static_cast<uint32_t>(height); y++) {
    this->y_boundaries_.push_back(static_cast<uint16_t>((y * target_height + height - 1) / height));
  }
  return true;
}

void JpegDecoder::draw_rgb565(int x, int y, int w, int h, const uint8_t *pixels) {
  if (pixels == nullptr) {
    return;
  }
  for (int source_y = 0; source_y < h; source_y++) {
    const int absolute_y = y + source_y;
    if (absolute_y < 0 || absolute_y >= this->source_height_) {
      continue;
    }
    const uint16_t y1 = this->y_boundaries_[absolute_y];
    const uint16_t y2 = this->y_boundaries_[absolute_y + 1];
    for (int source_x = 0; source_x < w; source_x++) {
      const int absolute_x = x + source_x;
      if (absolute_x < 0 || absolute_x >= this->source_width_) {
        continue;
      }
      const uint16_t x1 = this->x_boundaries_[absolute_x];
      const uint16_t x2 = this->x_boundaries_[absolute_x + 1];
      const uint8_t *pixel = pixels + (source_y * w + source_x) * 2;
      for (uint16_t destination_y = y1; destination_y < y2; destination_y++) {
        uint8_t *destination = this->image_->buffer_ + (destination_y * this->image_->get_buffer_width() + x1) * 2;
        for (uint16_t destination_x = x1; destination_x < x2; destination_x++) {
          destination[0] = pixel[0];
          destination[1] = pixel[1];
          destination += 2;
        }
      }
    }
  }
}

/**
 * @brief Callback method that will be called by the JPEGDEC engine when a chunk
 * of the image is decoded.
 *
 * @param jpeg  The JPEGDRAW object, including the context data.
 */
static int draw_callback(JPEGDRAW *jpeg) {
  JpegDecoder *decoder = (JpegDecoder *) jpeg->pUser;

  if (decoder == nullptr) {
    ESP_LOGE(TAG, "Decoder pointer is null!");
    return 0;
  }

  // Some very big images take too long to decode, so feed the watchdog on each callback
  // to avoid crashing if the executing task has a watchdog enabled.
#ifdef USE_ESP_IDF
  if (esp_task_wdt_status(nullptr) == ESP_OK) {
#endif
    App.feed_wdt();
#ifdef USE_ESP_IDF
  }
#endif
  size_t position = 0;
  size_t height = static_cast<size_t>(jpeg->iHeight);
  size_t width = static_cast<size_t>(jpeg->iWidth);
  if (decoder->uses_direct_rgb565()) {
    decoder->draw_rgb565(jpeg->x, jpeg->y, jpeg->iWidth, jpeg->iHeight,
                         reinterpret_cast<const uint8_t *>(jpeg->pPixels));
    return 1;
  }
  for (size_t y = 0; y < height; y++) {
    for (size_t x = 0; x < width; x++) {
      auto rg = decode_value(jpeg->pPixels[position++]);
      auto ba = decode_value(jpeg->pPixels[position++]);
      Color color(rg[1], rg[0], ba[1], ba[0]);

      decoder->draw(jpeg->x + x, jpeg->y + y, 1, 1, color);
    }
  }
  return 1;
}

int HOT JpegDecoder::decode(uint8_t *buffer, size_t size) {
  // JPEG decoder requires complete data
  // If we know the expected size, wait for it
  if (this->expected_size_ > 0 && size < this->expected_size_) {
    ESP_LOGV(TAG, "Download not complete. Size: %zu/%zu", size, this->expected_size_);
    return 0;
  }

  // If size unknown, try to decode and see if it's valid
  // The JPEGDEC library will fail gracefully if data is incomplete

  if (!this->jpeg_.openRAM(buffer, size, draw_callback)) {
    ESP_LOGE(TAG, "Could not open image for decoding: %d", this->jpeg_.getLastError());
    return DECODE_ERROR_INVALID_TYPE;
  }
  auto jpeg_type = this->jpeg_.getJPEGType();
  if (jpeg_type == JPEG_MODE_INVALID) {
    ESP_LOGE(TAG, "Unsupported JPEG image");
    return DECODE_ERROR_INVALID_TYPE;
  } else if (jpeg_type == JPEG_MODE_PROGRESSIVE) {
    ESP_LOGE(TAG, "Progressive JPEG images not supported");
    return DECODE_ERROR_INVALID_TYPE;
  }
  ESP_LOGD(TAG, "Image size: %d x %d, bpp: %d", this->jpeg_.getWidth(), this->jpeg_.getHeight(), this->jpeg_.getBpp());

  this->jpeg_.setUserPointer(this);
  this->direct_rgb565_ = this->image_->get_type() == image::IMAGE_TYPE_RGB565 && !this->image_->has_transparency();
  if (this->direct_rgb565_) {
    this->jpeg_.setPixelType(this->image_->is_big_endian_ ? RGB565_BIG_ENDIAN : RGB565_LITTLE_ENDIAN);
  } else {
    this->jpeg_.setPixelType(RGB8888);
  }
  if (!this->set_size(this->jpeg_.getWidth(), this->jpeg_.getHeight())) {
    return DECODE_ERROR_OUT_OF_MEMORY;
  }
  if (this->direct_rgb565_ && !this->prepare_rgb565_scaling_(this->jpeg_.getWidth(), this->jpeg_.getHeight())) {
    return DECODE_ERROR_OUT_OF_MEMORY;
  }
  if (!this->jpeg_.decode(0, 0, 0)) {
    auto error = this->jpeg_.getLastError();
    ESP_LOGE(TAG, "Error while decoding: %d", error);
    this->jpeg_.close();
    switch (error) {
      case JPEG_ERROR_MEMORY:
        return DECODE_ERROR_OUT_OF_MEMORY;
      case JPEG_UNSUPPORTED_FEATURE:
        return DECODE_ERROR_UNSUPPORTED_FORMAT;
      case JPEG_INVALID_FILE:
      case JPEG_INVALID_PARAMETER:
        return DECODE_ERROR_INVALID_TYPE;
      case JPEG_DECODE_ERROR:
      default:
        return DECODE_ERROR_INTERNAL_DECODER_ERROR;
    }
  }
  this->decoded_bytes_ = size;
  this->jpeg_.close();
  return size;
}

}  // namespace esphome::runtime_image

#endif  // USE_RUNTIME_IMAGE_JPEG
