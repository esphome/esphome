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

#include <utility>
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

inline uint8_t bits_per_pixel(ImageType type) {
  switch (type) {
    case display::ImageType::IMAGE_TYPE_BINARY:
      return 1;
    case display::ImageType::IMAGE_TYPE_GRAYSCALE:
      return 8;
    case display::ImageType::IMAGE_TYPE_RGB565:
      return 16;
    case display::ImageType::IMAGE_TYPE_RGB24:
      return 24;
    case display::ImageType::IMAGE_TYPE_RGBA:
      return 32;
    default:
      ESP_LOGE(TAG, "Unsupported image type %d.", type);
      return 0;
  }
}

inline bool is_color_on(const Color &color) {
  // This produces the most accurate monochrome conversion, but is slightly slower.
  //  return (0.2125 * color.r + 0.7154 * color.g + 0.0721 * color.b) > 127;

  // Approximation using fast integer computations; produces acceptable results
  // Equivalent to 0.25 * R + 0.5 * G + 0.25 * B
  return ((color.r >> 2) + (color.g >> 1) + (color.b >> 2)) & 0x80;
}

OnlineImage::OnlineImage(const char *url, int width, int height, ImageFormat format, ImageType type,
                         uint32_t download_buffer_size)
    : Image(nullptr, 0, 0, type),
      buffer_(nullptr),
      url_(url),
      download_buffer_size_(download_buffer_size),
      format_(format),
      bits_per_pixel_(bits_per_pixel(type)),
      fixed_width_(width),
      fixed_height_(height) {}

void OnlineImage::release() {
  if (buffer_) {
    ESP_LOGD(TAG, "Deallocating old buffer...");
    allocator_.deallocate(buffer_, get_buffer_size_());
    buffer_ = nullptr;
    width_ = 0;
    height_ = 0;
    etag_ = "";
  }
}

bool OnlineImage::resize_(int width_in, int height_in) {
  int width = fixed_width_;
  int height = fixed_height_;
  if (auto_resize_()) {
    width = width_in;
    height = height_in;
    if (width_ != width && height_ != height) {
      release();
    }
  }
  if (buffer_) {
    return false;
  }
  auto new_size = get_buffer_size_(width, height);
  ESP_LOGD(TAG, "Allocating new buffer of %d Bytes...", new_size);
  ESP_LOGD(TAG, "Bits per pixel: %d", bits_per_pixel_);
  delay_microseconds_safe(2000);
  buffer_ = allocator_.allocate(new_size);
  if (buffer_) {
    width_ = width;
    height_ = height;
    ESP_LOGD(TAG, "New size: (%d, %d)", this->width_, this->height_);
  } else {
    ESP_LOGE(TAG, "allocation failed. Free Heap: %d Bytes", ESP.getFreeHeap());
    return false;
  }
  return true;
}

void OnlineImage::draw_pixel_(int x, int y, Color color) {
  if (!buffer_) {
    ESP_LOGE(TAG, "Buffer not allocated!");
    return;
  }
  if (x < 0 || y < 0 || x >= width_ || y >= height_) {
    ESP_LOGE(TAG, "Tried to paint a pixel (%d,%d) outside the image!", x, y);
    return;
  }
  uint32_t pos = get_position_(x, y);
  switch (type_) {
    case display::ImageType::IMAGE_TYPE_BINARY: {
      uint32_t byte_num = pos;
      uint8_t bit_num = (x + y * width_) % 8;
      if ((has_transparency() && color.w > 127) || is_color_on(color)) {
        buffer_[byte_num] |= 1 << bit_num;
      } else {
        buffer_[byte_num] &= ~(1 << bit_num);
      }
      break;
    }
    case display::ImageType::IMAGE_TYPE_GRAYSCALE: {
      uint8_t gray = static_cast<uint8_t>(0.2125 * color.r + 0.7154 * color.g + 0.0721 * color.b);
      if (has_transparency()) {
        if (gray == 1) {
          gray = 0;
        }
        if (color.w < 0x80) {
          gray = 1;
        }
      }
      buffer_[pos] = gray;
      break;
    }
    case display::ImageType::IMAGE_TYPE_RGB565: {
      uint16_t col565 = display::ColorUtil::color_to_565(color);
      if (has_transparency()) {
        if (col565 == 0x0020) {
          col565 = 0;
        }
        if (color.w < 0x80) {
          col565 = 0x0020;
        }
      }
      buffer_[pos + 0] = static_cast<uint8_t>((col565 >> 8) & 0xFF);
      buffer_[pos + 1] = static_cast<uint8_t>(col565 & 0xFF);
      break;
    }
    case display::ImageType::IMAGE_TYPE_RGBA: {
      buffer_[pos + 0] = color.r;
      buffer_[pos + 1] = color.g;
      buffer_[pos + 2] = color.b;
      buffer_[pos + 3] = color.w;
      break;
    }
    case display::ImageType::IMAGE_TYPE_RGB24:
    default: {
      if (has_transparency()) {
        if (color.b == 1 && color.r == 0 && color.g == 0) {
          color.b = 0;
        }
        if (color.w < 0x80) {
          color.r = 0;
          color.g = 0;
          color.b = 1;
        }
      }
      buffer_[pos + 0] = color.r;
      buffer_[pos + 1] = color.g;
      buffer_[pos + 2] = color.b;
      break;
    }
  }
}

bool OnlineImage::get_pixel(int x, int y) const {
  if (x < 0 || x >= this->width_ || y < 0 || y >= this->height_) {
    return false;
  }
  uint32_t pos = x + y * width_;
  uint8_t position_offset = pos % 8;
  uint8_t mask = 1 << position_offset;
  return buffer_[pos / 8] & mask;
}

Color OnlineImage::get_grayscale_pixel(int x, int y) const {
  auto pos = get_position_(x, y);
  uint8_t grey = buffer_[pos];
  uint8_t alpha;
  if (grey == 1 && has_transparency()) {
    alpha = 0;
  } else {
    alpha = 0xFF;
  }
  return Color(grey, grey, grey, alpha);
}

Color OnlineImage::get_rgb565_pixel(int x, int y) const {
  auto pos = get_position_(x, y);
  uint16_t col565 = encode_uint16(buffer_[pos], buffer_[pos + 1]);
  uint8_t alpha;
  if (col565 == 0x0020 && has_transparency()) {
    alpha = 0;
  } else {
    alpha = 0xFF;
  }
  return Color(static_cast<uint8_t>((col565 >> 8) & 0xF8), static_cast<uint8_t>((col565 & 0x7E0) >> 3),
               static_cast<uint8_t>((col565 & 0x1F) << 3), alpha);
}

Color OnlineImage::get_color_pixel(int x, int y) const {
  if (x < 0 || x >= this->width_ || y < 0 || y >= this->height_) {
    ESP_LOGE(TAG, "Requested pixel (%d, %d) out of bounds!", x, y);
    return Color(0);
  }
  auto pos = get_position_(x, y);
  auto r = buffer_[pos + 0];
  auto g = buffer_[pos + 1];
  auto b = buffer_[pos + 2];
  auto a = (b == 1 && r == 0 && g == 0 && has_transparency()) ? 0 : 0xFF;
  return Color(r, g, b, a);
}

Color OnlineImage::get_rgba_pixel(int x, int y) const {
  if (x < 0 || x >= this->width_ || y < 0 || y >= this->height_) {
    return Color(0);
  }
  auto pos = get_position_(x, y);
  auto r = buffer_[pos + 0];
  auto g = buffer_[pos + 1];
  auto b = buffer_[pos + 2];
  auto a = buffer_[pos + 3];
  return Color(r, g, b, a);
}

void OnlineImage::update() {
  ESP_LOGI(TAG, "Updating image");
  HTTPClient http;

  std::unique_ptr<ImageDecoder> decoder;

#if defined(USE_ESP32) || defined(USE_ESP8266)
  if (follow_redirects_) {
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  } else {
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  }
  http.setRedirectLimit(redirect_limit_);
#endif

  int begin_status = http.begin(url_);
  if (!begin_status) {
    ESP_LOGE(TAG, "Could not download image from %s. Connection failed: %i", url_, begin_status);
    return;
  }

  http.setTimeout(timeout_);
#if defined(USE_ESP32)
  http.setConnectTimeout(timeout_);
#endif
  if (useragent_ != nullptr) {
    http.setUserAgent(useragent_);
  }
  if (etag_ != "") {
    http.addHeader("If-None-Match", etag_, false, true);
  }

  const char *header_keys[] = {"Content-Type", "ETag"};
  http.collectHeaders(header_keys, 2);

  int http_code = http.GET();
  if (http_code == HTTP_CODE_NOT_MODIFIED) {
    ESP_LOGI(TAG, "Image hasn't changed on server. Skipping download.");
    http.end();
    return;
  }
  if (http_code != HTTP_CODE_OK) {
    ESP_LOGE(TAG, "Could not download image from %s. Error code: %i", url_, http_code);
    http.end();
    return;
  }

  String content_type = http.header("Content-Type");
  String etag = http.header("ETag");
  size_t total_size = http.getSize();
  ESP_LOGD(TAG, "Content Type: %s", content_type.c_str());
  ESP_LOGD(TAG, "Content Length: %d", total_size);
  ESP_LOGD(TAG, "ETag: %s", etag.c_str());

  if (etag != "" && etag == etag_) {
    ESP_LOGI(TAG, "Image hasn't changed on server. Skipping download.");
    http.end();
    return;
  }

#ifdef ONLINE_IMAGE_PNG_SUPPORT
  if (format_ == ImageFormat::PNG) {
    decoder = esphome::make_unique<PngDecoder>(this);
  }
#endif  // ONLINE_IMAGE_PNG_SUPPORT

  if (!decoder) {
    ESP_LOGE(TAG, "Could not instantiate decoder. Image format unsupported.");
    return;
  }
  WiFiClient *stream = http.getStreamPtr();

  decoder->prepare(stream, total_size);
  ESP_LOGI(TAG, "Downloading image from %s", url_);
  std::vector<uint8_t> download_buffer(this->download_buffer_size_);
  size_t size = decoder->decode(http, stream, download_buffer);
  ESP_LOGI(TAG, "Decoded %d bytes", size);
  http.end();
  if (size != total_size) {
    // The download was not successful; retry next time.
    etag_ = "";
  } else {
    etag_ = etag;
  }

  data_start_ = buffer_;
}

}  // namespace online_image
}  // namespace esphome

#endif  // USE_ARDUINO
