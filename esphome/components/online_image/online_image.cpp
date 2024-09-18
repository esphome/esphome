#include "online_image.h"

#include "esphome/core/log.h"

static const char *const TAG = "online_image";

#include "image_decoder.h"

#ifdef USE_ONLINE_IMAGE_PNG_SUPPORT
#include "png_image.h"
#endif

namespace esphome {
namespace online_image {

using image::ImageType;

inline bool is_color_on(const Color &color) {
  // This produces the most accurate monochrome conversion, but is slightly slower.
  //  return (0.2125 * color.r + 0.7154 * color.g + 0.0721 * color.b) > 127;

  // Approximation using fast integer computations; produces acceptable results
  // Equivalent to 0.25 * R + 0.5 * G + 0.25 * B
  return ((color.r >> 2) + (color.g >> 1) + (color.b >> 2)) & 0x80;
}

OnlineImage::OnlineImage(const std::string &url, int width, int height, ImageFormat format, ImageType type,
                         uint32_t download_buffer_size)
    : Image(nullptr, 0, 0, type),
      buffer_(nullptr),
      download_buffer_(download_buffer_size),
      format_(format),
      fixed_width_(width),
      fixed_height_(height) {
  this->set_url(url);
}

void OnlineImage::draw(int x, int y, display::Display *display, Color color_on, Color color_off) {
  if (this->data_start_) {
    Image::draw(x, y, display, color_on, color_off);
  } else if (this->placeholder_) {
    this->placeholder_->draw(x, y, display, color_on, color_off);
  }
}

void OnlineImage::release() {
  if (this->buffer_) {
    ESP_LOGD(TAG, "Deallocating old buffer...");
    this->allocator_.deallocate(this->buffer_, this->get_buffer_size_());
    this->data_start_ = nullptr;
    this->buffer_ = nullptr;
    this->width_ = 0;
    this->height_ = 0;
    this->buffer_width_ = 0;
    this->buffer_height_ = 0;
    this->end_connection_();
  }
}

bool OnlineImage::resize_(int width_in, int height_in) {
  int width = this->fixed_width_;
  int height = this->fixed_height_;
  if (this->auto_resize_()) {
    width = width_in;
    height = height_in;
    if (this->width_ != width && this->height_ != height) {
      this->release();
    }
  }
  if (this->buffer_) {
    return false;
  }
  auto new_size = this->get_buffer_size_(width, height);
  ESP_LOGD(TAG, "Allocating new buffer of %d Bytes...", new_size);
  delay_microseconds_safe(2000);
  this->buffer_ = this->allocator_.allocate(new_size);
  if (this->buffer_) {
    this->buffer_width_ = width;
    this->buffer_height_ = height;
    this->width_ = width;
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
    this->end_connection_();
    return false;
  }
  return true;
}

void OnlineImage::update() {
  if (this->decoder_) {
    ESP_LOGW(TAG, "Image already being updated.");
    return;
  } else {
    ESP_LOGI(TAG, "Updating image");
  }

  this->downloader_ = this->parent_->get(this->url_);

  if (this->downloader_ == nullptr) {
    ESP_LOGE(TAG, "Download failed.");
    this->end_connection_();
    this->download_error_callback_.call();
    return;
  }

  int http_code = this->downloader_->status_code;
  if (http_code == HTTP_CODE_NOT_MODIFIED) {
    // Image hasn't changed on server. Skip download.
    this->end_connection_();
    return;
  }
  if (http_code != HTTP_CODE_OK) {
    ESP_LOGE(TAG, "HTTP result: %d", http_code);
    this->end_connection_();
    this->download_error_callback_.call();
    return;
  }

  ESP_LOGD(TAG, "Starting download");
  size_t total_size = this->downloader_->content_length;

#ifdef USE_ONLINE_IMAGE_PNG_SUPPORT
  if (this->format_ == ImageFormat::PNG) {
    this->decoder_ = esphome::make_unique<PngDecoder>(this);
  }
#endif  // ONLINE_IMAGE_PNG_SUPPORT

  if (!this->decoder_) {
    ESP_LOGE(TAG, "Could not instantiate decoder. Image format unsupported.");
    this->end_connection_();
    this->download_error_callback_.call();
    return;
  }
  this->decoder_->prepare(total_size);
  ESP_LOGI(TAG, "Downloading image");
}

void OnlineImage::loop() {
  if (!this->decoder_) {
    // Not decoding at the moment => nothing to do.
    return;
  }
  if (!this->downloader_ || this->decoder_->is_finished()) {
    ESP_LOGD(TAG, "Image fully downloaded");
    this->data_start_ = buffer_;
    this->width_ = buffer_width_;
    this->height_ = buffer_height_;
    this->end_connection_();
    this->download_finished_callback_.call();
    return;
  }
  if (this->downloader_ == nullptr) {
    ESP_LOGE(TAG, "Downloader not instantiated; cannot download");
    return;
  }
  size_t available = this->download_buffer_.free_capacity();
  if (available) {
    auto len = this->downloader_->read(this->download_buffer_.append(), available);
    if (len > 0) {
      this->download_buffer_.write(len);
      auto fed = this->decoder_->decode(this->download_buffer_.data(), this->download_buffer_.unread());
      if (fed < 0) {
        ESP_LOGE(TAG, "Error when decoding image.");
        this->end_connection_();
        this->download_error_callback_.call();
        return;
      }
      this->download_buffer_.read(fed);
    }
  }
}

void OnlineImage::draw_pixel_(int x, int y, Color color) {
  if (!this->buffer_) {
    ESP_LOGE(TAG, "Buffer not allocated!");
    return;
  }
  if (x < 0 || y < 0 || x >= this->buffer_width_ || y >= this->buffer_height_) {
    ESP_LOGE(TAG, "Tried to paint a pixel (%d,%d) outside the image!", x, y);
    return;
  }
  uint32_t pos = this->get_position_(x, y);
  switch (this->type_) {
    case ImageType::IMAGE_TYPE_BINARY: {
      const uint32_t width_8 = ((this->width_ + 7u) / 8u) * 8u;
      const uint32_t pos = x + y * width_8;
      if ((this->has_transparency() && color.w > 127) || is_color_on(color)) {
        this->buffer_[pos / 8u] |= (0x80 >> (pos % 8u));
      } else {
        this->buffer_[pos / 8u] &= ~(0x80 >> (pos % 8u));
      }
      break;
    }
    case ImageType::IMAGE_TYPE_GRAYSCALE: {
      uint8_t gray = static_cast<uint8_t>(0.2125 * color.r + 0.7154 * color.g + 0.0721 * color.b);
      if (this->has_transparency()) {
        if (gray == 1) {
          gray = 0;
        }
        if (color.w < 0x80) {
          gray = 1;
        }
      }
      this->buffer_[pos] = gray;
      break;
    }
    case ImageType::IMAGE_TYPE_RGB565: {
      uint16_t col565 = display::ColorUtil::color_to_565(color);
      if (this->has_transparency()) {
        if (col565 == 0x0020) {
          col565 = 0;
        }
        if (color.w < 0x80) {
          col565 = 0x0020;
        }
      }
      this->buffer_[pos + 0] = static_cast<uint8_t>((col565 >> 8) & 0xFF);
      this->buffer_[pos + 1] = static_cast<uint8_t>(col565 & 0xFF);
      break;
    }
    case ImageType::IMAGE_TYPE_RGBA: {
      this->buffer_[pos + 0] = color.r;
      this->buffer_[pos + 1] = color.g;
      this->buffer_[pos + 2] = color.b;
      this->buffer_[pos + 3] = color.w;
      break;
    }
    case ImageType::IMAGE_TYPE_RGB24:
    default: {
      if (this->has_transparency()) {
        if (color.b == 1 && color.r == 0 && color.g == 0) {
          color.b = 0;
        }
        if (color.w < 0x80) {
          color.r = 0;
          color.g = 0;
          color.b = 1;
        }
      }
      this->buffer_[pos + 0] = color.r;
      this->buffer_[pos + 1] = color.g;
      this->buffer_[pos + 2] = color.b;
      break;
    }
  }
}

void OnlineImage::end_connection_() {
  if (this->downloader_) {
    this->downloader_->end();
    this->downloader_ = nullptr;
  }
  this->decoder_.reset();
  this->download_buffer_.reset();
}

bool OnlineImage::validate_url_(const std::string &url) {
  if ((url.length() < 8) || (url.find("http") != 0) || (url.find("://") == std::string::npos)) {
    ESP_LOGE(TAG, "URL is invalid and/or must be prefixed with 'http://' or 'https://'");
    return false;
  }
  return true;
}

void OnlineImage::add_on_finished_callback(std::function<void()> &&callback) {
  this->download_finished_callback_.add(std::move(callback));
}

void OnlineImage::add_on_error_callback(std::function<void()> &&callback) {
  this->download_error_callback_.add(std::move(callback));
}

}  // namespace online_image
}  // namespace esphome
