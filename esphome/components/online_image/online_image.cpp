#ifdef USE_ARDUINO

#include "online_image.h"

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
      download_buffer_(download_buffer_size),
      format_(format),
      bits_per_pixel_(bits_per_pixel(type)),
      fixed_width_(width),
      fixed_height_(height) {}

void OnlineImage::draw(int x, int y, DisplayBuffer *display, Color color_on, Color color_off) {
  switch (type_) {
    case IMAGE_TYPE_BINARY: {
      for (int img_x = 0; img_x < width_; img_x++) {
        for (int img_y = 0; img_y < height_; img_y++) {
          if (this->get_binary_pixel_(img_x, img_y)) {
            display->draw_pixel_at(x + img_x, y + img_y, color_on);
          } else if (!this->transparent_) {
            display->draw_pixel_at(x + img_x, y + img_y, color_off);
          }
        }
      }
      break;
    }
    case IMAGE_TYPE_GRAYSCALE:
      for (int img_x = 0; img_x < width_; img_x++) {
        for (int img_y = 0; img_y < height_; img_y++) {
          auto color = this->get_grayscale_pixel_(img_x, img_y);
          if (color.w >= 0x80) {
            display->draw_pixel_at(x + img_x, y + img_y, color);
          }
        }
      }
      break;
    case IMAGE_TYPE_RGB565:
      for (int img_x = 0; img_x < width_; img_x++) {
        for (int img_y = 0; img_y < height_; img_y++) {
          auto color = this->get_rgb565_pixel_(img_x, img_y);
          if (color.w >= 0x80) {
            display->draw_pixel_at(x + img_x, y + img_y, color);
          }
        }
      }
      break;
    case IMAGE_TYPE_RGB24:
      for (int img_x = 0; img_x < width_; img_x++) {
        for (int img_y = 0; img_y < height_; img_y++) {
          auto color = this->get_rgb24_pixel_(img_x, img_y);
          if (color.w >= 0x80) {
            display->draw_pixel_at(x + img_x, y + img_y, color);
          }
        }
      }
      break;
    case IMAGE_TYPE_RGBA:
      for (int img_x = 0; img_x < width_; img_x++) {
        for (int img_y = 0; img_y < height_; img_y++) {
          auto color = this->get_rgba_pixel_(img_x, img_y);
          if (color.w >= 0x80) {
            display->draw_pixel_at(x + img_x, y + img_y, color);
          }
        }
      }
      break;
  }
}

void OnlineImage::set_follow_redirects(bool follow, int limit) {
#if defined(USE_ESP32) || defined(USE_ESP8266)
  if (follow) {
    http_.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  } else {
    http_.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  }
  http_.setRedirectLimit(limit);
#endif
}

void OnlineImage::set_useragent(const char *useragent) {
  if (useragent != nullptr) {
    http_.setUserAgent(useragent);
  }
}

void OnlineImage::release() {
  if (buffer_) {
    ESP_LOGD(TAG, "Deallocating old buffer...");
    allocator_.deallocate(buffer_, get_buffer_size_());
    buffer_ = nullptr;
    width_ = 0;
    height_ = 0;
    buffer_width_ = 0;
    buffer_height_ = 0;
    etag_ = "";
    end_connection_();
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
    buffer_width_ = width;
    buffer_height_ = height;
    width_ = width;
    ESP_LOGD(TAG, "New size: (%d, %d)", width, height);
  } else {
#if defined(USE_ESP8266)
    // NOLINTNEXTLINE(readability-static-accessed-through-instance)
    int max_block = ESP.getMaxFreeBlockSize();
#elif defined(USE_ESP32)
    int max_block = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
#else
    int max_block = -1;
#endif
    ESP_LOGE(TAG, "allocation failed. Biggest block in heap: %d Bytes", max_block);
    end_connection_();
    return false;
  }
  return true;
}

void OnlineImage::loop() {
  PollingComponent::loop();

  if (!decoder_) {
    // Not decoding at the moment => nothing to do.
    return;
  }
  if (!http_.connected() || decoder_->is_finished()) {
    ESP_LOGD(TAG, "Image fully downloaded");
    data_start_ = buffer_;
    width_ = buffer_width_;
    height_ = buffer_height_;
    end_connection_();
    download_finished_callback_.call();
    return;
  }
  WiFiClient *stream = http_.getStreamPtr();
  size_t available = stream->available();
  if (available) {
    if (available > download_buffer_.free_capacity()) {
      available = download_buffer_.free_capacity();
    }

    auto len = stream->readBytes(download_buffer_.append(), available);
    if (len > 0) {
      download_buffer_.write(len);
      auto fed = decoder_->decode(download_buffer_.data(), download_buffer_.unread());
      if (fed < 0) {
        ESP_LOGE(TAG, "Error when decoding image.");
        etag_ = "";  // Need to re-download
        end_connection_();
        download_error_callback_.call();
        return;
      }
      download_buffer_.read(fed);
    }
  }
}

void OnlineImage::draw_pixel_(int x, int y, Color color) {
  if (!buffer_) {
    ESP_LOGE(TAG, "Buffer not allocated!");
    return;
  }
  if (x < 0 || y < 0 || x >= buffer_width_ || y >= buffer_height_) {
    ESP_LOGE(TAG, "Tried to paint a pixel (%d,%d) outside the image!", x, y);
    return;
  }
  uint32_t pos = get_position_(x, y);
  switch (type_) {
    case display::ImageType::IMAGE_TYPE_BINARY: {
      uint32_t byte_num = pos;
      uint8_t bit_num = (x + y * buffer_width_) % 8;
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

bool OnlineImage::get_binary_pixel_(int x, int y) const {
  uint32_t pos = x + y * width_;
  uint8_t position_offset = pos % 8;
  uint8_t mask = 1 << position_offset;
  return buffer_[pos / 8] & mask;
}

Color OnlineImage::get_grayscale_pixel_(int x, int y) const {
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

Color OnlineImage::get_rgb565_pixel_(int x, int y) const {
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

Color OnlineImage::get_color_pixel_(int x, int y) const {
  auto pos = get_position_(x, y);
  auto r = buffer_[pos + 0];
  auto g = buffer_[pos + 1];
  auto b = buffer_[pos + 2];
  auto a = (b == 1 && r == 0 && g == 0 && has_transparency()) ? 0 : 0xFF;
  return Color(r, g, b, a);
}

Color OnlineImage::get_rgba_pixel_(int x, int y) const {
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

void OnlineImage::end_connection_() {
  http_.end();
  decoder_.reset();
  download_buffer_.reset();
}

void OnlineImage::update() {
  if (decoder_) {
    ESP_LOGW(TAG, "Image already being updated.");
    return;
  } else {
    ESP_LOGI(TAG, "Updating image");
  }

  int begin_status = http_.begin(url_);
  if (!begin_status) {
    ESP_LOGE(TAG, "Could not download image from %s. Connection failed: %i", url_, begin_status);
    return;
  }

  http_.setTimeout(timeout_);
#if defined(USE_ESP32)
  http_.setConnectTimeout(timeout_);
#endif
  if (etag_ != "") {
    http_.addHeader("If-None-Match", etag_, false, true);
  }

  const char *header_keys[] = {"Content-Type", "ETag"};
  http_.collectHeaders(header_keys, 2);

  int http_code = http_.GET();
  if (http_code == HTTP_CODE_NOT_MODIFIED) {
    ESP_LOGI(TAG, "Image hasn't changed on server. Skipping download.");
    end_connection_();
    return;
  }
  if (http_code != HTTP_CODE_OK) {
    ESP_LOGE(TAG, "Could not download image from %s. Error code: %i", url_, http_code);
    end_connection_();
    download_error_callback_.call();
    return;
  }

  String content_type = http_.header("Content-Type");
  String etag = http_.header("ETag");
  size_t total_size = http_.getSize();
  ESP_LOGD(TAG, "Content Type: %s", content_type.c_str());
  ESP_LOGD(TAG, "Content Length: %d", total_size);
  ESP_LOGD(TAG, "ETag: %s", etag.c_str());

  if (etag != "" && etag == etag_) {
    ESP_LOGI(TAG, "Image hasn't changed on server. Skipping download.");
    end_connection_();
    return;
  }
  etag_ = etag;

#ifdef ONLINE_IMAGE_PNG_SUPPORT
  if (format_ == ImageFormat::PNG) {
    decoder_ = esphome::make_unique<PngDecoder>(this);
  }
#endif  // ONLINE_IMAGE_PNG_SUPPORT

  if (!decoder_) {
    ESP_LOGE(TAG, "Could not instantiate decoder. Image format unsupported.");
    end_connection_();
    download_error_callback_.call();
    return;
  }
  decoder_->prepare(total_size);
  ESP_LOGI(TAG, "Downloading image from %s", url_);
}

void OnlineImage::add_on_finished_callback(std::function<void()> &&callback) {
  download_finished_callback_.add(std::move(callback));
}

void OnlineImage::add_on_error_callback(std::function<void()> &&callback) {
  download_error_callback_.add(std::move(callback));
}

}  // namespace online_image
}  // namespace esphome

#endif  // USE_ARDUINO
