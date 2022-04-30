#ifdef USE_ARDUINO

#include "png_image.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"

#ifdef ONLINE_IMAGE_PNG_SUPPORT
namespace esphome {
namespace online_image {

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
void drawCallback(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4]) {
  PngDecoder *decoder = (PngDecoder *) pngle_get_user_data(pngle);
  Color color(rgba[0], rgba[1], rgba[2], rgba[3]);

  if (rgba[3]) {
    x += decoder->x0();
    y += decoder->y0();
    decoder->display()->filled_rectangle(x, y, w, h, color);
  }
}

PngDecoder::PngDecoder(display::DisplayBuffer *display) : ImageDecoder(display), pngle(pngle_new()) {}

PngDecoder::~PngDecoder() { pngle_destroy(pngle); }

void PngDecoder::prepare(WiFiClient *stream) {
  pngle_set_user_data(pngle, this);
  pngle_set_draw_callback(pngle, drawCallback);
}

size_t HOT PngDecoder::decode(HTTPClient &http, WiFiClient *stream) {
  uint8_t buf[2048];
  int remain = 0;
  int total = 0;
  while (http.connected()) {
    App.feed_wdt();
    size_t size = stream->available();
    if (!size) {
      delay(1);
      continue;
    }

    if (size > sizeof(buf) - remain) {
      size = sizeof(buf) - remain;
    }

    int len = stream->readBytes(buf + remain, size);
    total += len;
    if (len > 0) {
      int fed = pngle_feed(pngle, buf, remain + len);
      if (fed < 0) {
        break;
      }

      remain = remain + len - fed;
      if (remain > 0) {
        memmove(buf, buf + fed, remain);
      }
    } else {
      delay(1);
    }
  }
  App.feed_wdt();
  return total;
}

}  // namespace online_image
}  // namespace esphome

#endif  // ONLINE_IMAGE_PNG_SUPPORT

#endif  // USE_ARDUINO
