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

/**
 * @brief Callback method that will be called by the JPEGDEC engine when a chunk
 * of the image is decoded.
 *
 * @param jpeg  The JPEGDRAW object, including the context data.
 */
static int draw_callback(JPEGDRAW *jpeg) {
  ImageDecoder *decoder = (ImageDecoder *) jpeg->pUser;

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
  for (size_t y = 0; y < height; y++) {
    for (size_t x = 0; x < width; x++) {
      auto rg = decode_value(jpeg->pPixels[position++]);
      auto ba = decode_value(jpeg->pPixels[position++]);
      Color color(rg[1], rg[0], ba[1], ba[0]);

      if (!decoder) {
        ESP_LOGE(TAG, "Decoder pointer is null!");
        return 0;
      }
      decoder->draw(jpeg->x + x, jpeg->y + y, 1, 1, color);
    }
  }
  return 1;
}

int JpegDecoder::prepare(size_t expected_size) {
  ImageDecoder::prepare(expected_size);
  // JPEG decoder needs complete data before decoding
  return 0;
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
  this->jpeg_.setPixelType(RGB8888);
  if (!this->set_size(this->jpeg_.getWidth(), this->jpeg_.getHeight())) {
    return DECODE_ERROR_OUT_OF_MEMORY;
  }
  if (!this->jpeg_.decode(0, 0, 0)) {
    ESP_LOGE(TAG, "Error while decoding.");
    this->jpeg_.close();
    return DECODE_ERROR_UNSUPPORTED_FORMAT;
  }
  this->decoded_bytes_ = size;
  this->jpeg_.close();
  return size;
}

}  // namespace esphome::runtime_image

#endif  // USE_RUNTIME_IMAGE_JPEG
