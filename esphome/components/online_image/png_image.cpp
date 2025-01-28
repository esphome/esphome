#include "png_image.h"
#ifdef USE_ONLINE_IMAGE_PNG_SUPPORT

#include "esphome/components/display/display_buffer.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

static const char *const TAG = "online_image.png";

namespace esphome {
namespace online_image {

/**
 * @brief Callback method that will be called by the PNGLE engine when the basic
 * data of the image is received (i.e. width and height);
 *
 * @param pngle The PNGLE object, including the context data.
 * @param w The width of the image.
 * @param h The height of the image.
 */
static void init_callback(pngle_t *pngle, uint32_t w, uint32_t h) {
  PngDecoder *decoder = (PngDecoder *) pngle_get_user_data(pngle);
  decoder->set_size(w, h);
}

/**
 * @brief Callback method that will be called by the PNGLE engine when a chunk
 * of the image is decoded.
 *
 * @param pngle The PNGLE object, including the context data.
 * @param x The X coordinate to draw the rectangle on.
 * @param y The Y coordinate to draw the rectangle on.
 * @param w The width of the rectangle to draw.
 * @param h The height of the rectangle to draw.
 * @param rgba The color to paint the rectangle in.
 */
static void draw_callback(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4]) {
  PngDecoder *decoder = (PngDecoder *) pngle_get_user_data(pngle);
  Color color(rgba[0], rgba[1], rgba[2], rgba[3]);
  decoder->draw(x, y, w, h, color);
}

void PngDecoder::prepare(size_t download_size) {
  ImageDecoder::prepare(download_size);
  pngle_set_user_data(this->pngle_, this);
  pngle_set_init_callback(this->pngle_, init_callback);
  pngle_set_draw_callback(this->pngle_, draw_callback);
}

int HOT PngDecoder::decode(uint8_t *buffer, size_t size) {
  if (!this->pngle_) {
    ESP_LOGE(TAG, "PNG decoder engine not initialized!");
    return DECODE_ERROR_OUT_OF_MEMORY;
  }
  if (size < 256 && size < this->download_size_ - this->decoded_bytes_) {
    ESP_LOGD(TAG, "Waiting for data");
    return 0;
  }
  auto fed = pngle_feed(this->pngle_, buffer, size);
  if (fed < 0) {
    ESP_LOGE(TAG, "Error decoding image: %s", pngle_error(this->pngle_));
  } else {
    this->decoded_bytes_ += fed;
  }
  return fed;
}

}  // namespace online_image
}  // namespace esphome

#endif  // USE_ONLINE_IMAGE_PNG_SUPPORT
