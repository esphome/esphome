#include "online_image.h"
#include "esphome/core/color.h"

#ifdef USE_ESP32
#include <HTTPClient.h>
#endif
#ifdef USE_ESP8266
#include <ESP8266HTTPClient.h>
#ifdef USE_HTTP_REQUEST_ESP8266_HTTPS
#include <WiFiClientSecure.h>
#endif
#endif

#include "pngle.h"

namespace esphome {
namespace online_image {

using display::DisplayBuffer;

struct Context {
  int x;
  int y;
  uint16_t w;
  uint16_t h;
  DisplayBuffer* display;
};

void drawCallback(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4]) {
    Context* context = (Context*)pngle_get_user_data(pngle);
  Color color(rgba[0], rgba[1], rgba[2], rgba[3]);

  float scale_x = 1;
  float scale_y = 1;
  uint32_t width = pngle_get_width(pngle);
  uint32_t height = pngle_get_height(pngle);
  if (width && width <= context->w) {
    scale_x = context->w / width;
  }
  if (height && height <= context->h) {
    scale_y = context->h / height;
  }

  if (rgba[3]) {
    x = context->x + ceil(x * scale_x);
    y = context->y + ceil(y * scale_y);
    w = ceil(w * scale_x);
    h = ceil(h * scale_y);
    context->display->filled_rectangle(x, y, w, h, color);
  }
}

void OnlineImage::draw(int x, int y, DisplayBuffer* display) {
  HTTPClient http;

  http.begin(url);

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    http.end();
    return ;
  }

  WiFiClient *stream = http.getStreamPtr();

  pngle_t *pngle = pngle_new();
  Context context_data {.x=x, .y=y, .w=width, .h=height, .display=display};
  pngle_set_user_data(pngle, &context_data);
  pngle_set_draw_callback(pngle, drawCallback);

  uint8_t buf[2048];
  int remain = 0;
  while (http.connected()) {
    size_t size = stream->available();
    if (!size) { delay(1); continue; }

    if (size > sizeof(buf) - remain) {
      size = sizeof(buf) - remain;
    }

    int len = stream->readBytes(buf + remain, size);
    if (len > 0) {
      int fed = pngle_feed(pngle, buf, remain + len);
      if (fed < 0) {
        break;
      }

      remain = remain + len - fed;
      if (remain > 0) memmove(buf, buf + fed, remain);
    } else {
      delay(1);
    }
  }

  pngle_destroy(pngle);

  http.end();

}

} // namespace online_image
} // namespace esphome
