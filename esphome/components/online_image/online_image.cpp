#ifdef USE_ARDUINO

#include "online_image.h"
#include "esphome/core/application.h"
#include "esphome/core/color.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32
#include <HTTPClient.h>
#endif
#ifdef USE_ESP8266
#include <ESP8266HTTPClient.h>
#ifdef USE_HTTP_REQUEST_ESP8266_HTTPS
#include <WiFiClientSecure.h>
#endif
#endif

#include <memory>

static const char *const TAG = "online_image";

#ifdef ONLINE_IMAGE_PNG_SUPPORT
#include "png_image.h"
#endif

namespace esphome {
namespace online_image {

using display::DisplayBuffer;

void OnlineImage::draw(int x, int y, DisplayBuffer *display, Color color_on, Color color_off) {
  HTTPClient http;

  std::unique_ptr<ImageDecoder> decoder;

#ifdef ONLINE_IMAGE_PNG_SUPPORT
  if (format_ == ImageFormat::PNG) {
    decoder = esphome::make_unique<PngDecoder>(display);
  }
#endif  // ONLINE_IMAGE_PNG_SUPPORT

  if (!decoder) {
    ESP_LOGE(TAG, "Could not instantiate decoder. Image format unsupported.");
    return;
  }

  decoder->set_monochrome_colors(color_on, color_off);

  int begin_status = http.begin(url_);
  if (!begin_status) {
    ESP_LOGE(TAG, "Could not download image from %s. Connection failed: %i", url_, begin_status);
    return;
  }

  int http_code = http.GET();
  if (http_code != HTTP_CODE_OK) {
    App.feed_wdt();
    ESP_LOGE(TAG, "Could not download image from %s. Error code: %i", url_, http_code);
    http.end();
    return;
  }
  WiFiClient *stream = http.getStreamPtr();

  decoder->set_offset(x, y);

  decoder->prepare(stream);
  ESP_LOGD(TAG, "Downloading image from %s", url_);
  std::vector<uint8_t> buffer(this->buffer_size_);
  size_t size = decoder->decode(http, stream, buffer);
  ESP_LOGD(TAG, "Decoded %d bytes", size);

  http.end();
}

bool ImageDecoder::is_color_on(const Color &color) {
  // This produces the most accurate monochrome conversion, but is slightly slower.
  //  return (0.2125 * color.r + 0.7154 * color.g + 0.0721 * color.b) > 127;

  // Approximation using fast integer computations; produces acceptable results
  // Equivalent to 0.25 * R + 0.5 * G + 0.25 * B
  return ((color.r >> 2) + (color.g >> 1) + (color.b >> 2)) & 0x80;
}

void ImageDecoder::draw(uint32_t x, uint32_t y, uint32_t w, uint32_t h, Color color) {
  if (display_->get_display_type() == display::DisplayType::DISPLAY_TYPE_BINARY) {
    color = is_color_on(color) ? color_on() : color_off();
  }

  static uint16_t display_width = display_->get_width();
  static uint16_t display_height = display_->get_height();

#ifdef ONLINE_IMAGE_WATCHDOG_ON_DECODE
  App.feed_wdt();
#endif

  if (color.w) {
    x += x0();
    y += y0();
    if (x >= display_width || y >= display_height) {
      return;
    }
    if (x + w >= display_width) {
      w = display_width - x;
    }
    if (y + h >= display_height) {
      h = display_height - y;
    }
    display_->filled_rectangle(x, y, w, h, color);
  }
}

}  // namespace online_image
}  // namespace esphome

#endif  // USE_ARDUINO
