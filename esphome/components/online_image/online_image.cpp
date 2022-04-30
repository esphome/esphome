#ifdef USE_ARDUINO

#include "online_image.h"
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

void OnlineImage::draw(int x, int y, DisplayBuffer *display) {
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

  http.begin(url_);

  int http_code = http.GET();
  if (http_code != HTTP_CODE_OK) {
    http.end();
    return;
  }
  WiFiClient *stream = http.getStreamPtr();

  decoder->set_offset(x, y);

  decoder->prepare(stream);
  ESP_LOGD(TAG, "Downloading image from %s", url_);
  size_t size = decoder->decode(http, stream);
  ESP_LOGD(TAG, "Decoded %d bytes", size);

  http.end();
}

}  // namespace online_image
}  // namespace esphome

#endif  // USE_ARDUINO
