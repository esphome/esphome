#ifdef USE_ARDUINO

#include "png_image.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"

#ifdef ONLINE_IMAGE_PNG_SUPPORT

static const char *const TAG = "online_image.png";

namespace esphome {
namespace online_image {

/**
 * @brief Converts an RGB color (ignoring alpha) to a 1-bit grayscale color.
 *
 * @param color The color to convert from; the alpha channel will be ignored.
 * @return Whether the 8-bit grayscale equivalent color is brighter than average (i.e. brighter than 0x7F).
 */
static bool isColorOn(Color color) {
  // This produces the most accurate monochrome conversion, but is slightly slower.
  //  return (0.2125 * color.r + 0.7154 * color.g + 0.0721 * color.b) > 127;

  // Approximation using fast integer computations; produces acceptable results
  // Would be equivalent to 0.25 * R + 0.5 * G + 0.25 * B
  return ((color.r >> 2) + (color.g >> 1) + (color.b >> 2)) & 0x80;
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
static void drawCallback(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4]) {
  PngDecoder *decoder = (PngDecoder *) pngle_get_user_data(pngle);
  Color color(rgba[0], rgba[1], rgba[2], rgba[3]);
  if (decoder->display()->get_display_type() == display::DisplayType::DISPLAY_TYPE_BINARY) {
    color = isColorOn(color) ? decoder->color_on() : decoder->color_off();
  }

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

size_t HOT PngDecoder::decode(HTTPClient &http, WiFiClient *stream, std::vector<uint8_t> &buf) {
  int remain = 0;
  int total = 0;
  uint8_t *buffer = buf.data();
  ESP_LOGD(TAG, "Buffer size: %d", buf.size());

  while (http.connected()) {
    App.feed_wdt();
    size_t size = stream->available();
    if (!size) {
      delay(1);
      continue;
    }

    if (size > buf.size() - remain) {
      size = buf.size() - remain;
    }

    int len = stream->readBytes(buffer + remain, size);
    total += len;
    if (len > 0) {
      int fed = pngle_feed(pngle, buffer, remain + len);
      if (fed < 0) {
        ESP_LOGE(TAG, "Error decoding image: %s", pngle_error(pngle));
        break;
      }

      remain = remain + len - fed;
      if (remain > 0) {
        memmove(buffer, buffer + fed, remain);
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
