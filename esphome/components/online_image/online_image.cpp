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

static const char *const TAG = "online_image";

#include "image_decoder.h"

#ifdef ONLINE_IMAGE_PNG_SUPPORT
#include "png_image.h"
#endif

namespace esphome {
namespace online_image {

using namespace display;

OnlineImage::OnlineImage(const char *url, uint16_t width, uint16_t height, ImageFormat format, ImageType type, uint32_t download_buffer_size)
  : buffer_(width, height, type), url_(url), download_buffer_size_(download_buffer_size), format_(format),
   Image(nullptr, 0, 0, type) {
    createDefaultImage(width, height);
}

uint8_t *OnlineImage::createDefaultImage(uint16_t width, uint16_t height) {
  auto buf = buffer_.resize(width, height);

  if (!buf) {
    ESP_LOGE(TAG, "Could not allocate buffer for image.");
    return buf;
  }

  data_start_ = buf;
  width_ = buffer_.get_width();
  height_ = buffer_.get_height();

  uint16_t *buffer = (uint16_t *) buf;

  for (uint16_t i = 0; i < width; i++) {
    for (uint16_t j = 0; j < height; j++) {
      buffer_.draw_pixel(i, j, Color::BLACK);
      if (i == 0 || i == width - 1 || j == 0 || j == height - 1) {
        buffer_.draw_pixel(i, j, Color::WHITE);
      }
      if (i == j || width - i == j) {
        buffer_.draw_pixel(i, j, Color(255, 0, 0, 255));
      }
    }
  }
  return buf;
}

bool OnlineImage::get_pixel(int x, int y) const {
  if (x < 0 || x >= this->width_ || y < 0 || y >= this->height_)
    return false;
  return buffer_.get_pixel(x, y).is_on();
}

Color OnlineImage::get_color_pixel(int x, int y) const {
  if (x < 0 || x >= this->width_ || y < 0 || y >= this->height_)
    return Color::BLACK;
  Color color = buffer_.get_pixel(x, y);
  return color;
}

void OnlineImage::update() {
  ESP_LOGI(TAG, "Updating image");
  HTTPClient http;

  std::unique_ptr<ImageDecoder> decoder;
  ESP_LOGD(TAG, "Free heap: %d", ESP.getFreeHeap());
#ifdef ONLINE_IMAGE_PNG_SUPPORT
  if (format_ == ImageFormat::PNG) {
    decoder = esphome::make_unique<PngDecoder>(&buffer_);
  }
#endif  // ONLINE_IMAGE_PNG_SUPPORT

  ESP_LOGD(TAG, "Free heap: %d", ESP.getFreeHeap());
  if (!decoder) {
    ESP_LOGE(TAG, "Could not instantiate decoder. Image format unsupported.");
    return;
  }

  int begin_status = http.begin(url_);
  if (!begin_status) {
    ESP_LOGE(TAG, "Could not download image from %s. Connection failed: %i", url_, begin_status);
    return;
  }

  int http_code = http.GET();
  if (http_code != HTTP_CODE_OK) {
    ESP_LOGE(TAG, "Could not download image from %s. Error code: %i", url_, http_code);
    http.end();
    return;
  }
  WiFiClient *stream = http.getStreamPtr();

  decoder->prepare(stream);
  ESP_LOGD(TAG, "Downloading image from %s", url_);
  std::vector<uint8_t> buffer(this->download_buffer_size_);
  size_t size = decoder->decode(http, stream, buffer);
  ESP_LOGI(TAG, "Decoded %d bytes", size);
  http.end();

  data_start_ = buffer_.get_buffer();
  width_ = buffer_.get_width();
  height_ = buffer_.get_height();
}

void ImageDecoder::set_size(uint16_t width, uint16_t height) {
  if (buffer_->get_width() == 0 || buffer_->get_height() == 0) {
    ESP_LOGD(TAG, "resizing buffer to (%d, %d)", width, height);
    buffer_->resize(width, height);
    x_scale = 1.0;
    y_scale = 1.0;
  } else {
    ESP_LOGD(TAG, "Not resizing buffer (%d, %d)", buffer_->get_width(), buffer_->get_height());
    x_scale = (double)buffer_->get_width() / width;
    y_scale = (double)buffer_->get_height() / height;
  }
}

bool ImageDecoder::is_color_on(const Color &color) {
  // This produces the most accurate monochrome conversion, but is slightly slower.
  //  return (0.2125 * color.r + 0.7154 * color.g + 0.0721 * color.b) > 127;

  // Approximation using fast integer computations; produces acceptable results
  // Equivalent to 0.25 * R + 0.5 * G + 0.25 * B
  return ((color.r >> 2) + (color.g >> 1) + (color.b >> 2)) & 0x80;
}

void ImageDecoder::draw(uint32_t x, uint32_t y, uint32_t w, uint32_t h, Color color) {

  buffer_->draw_rectangle(x * x_scale, y * y_scale, std::ceil(w * x_scale), std::ceil(h * y_scale), color);
}

}  // namespace online_image
}  // namespace esphome

#endif  // USE_ARDUINO
