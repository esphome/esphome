#include "runtime_image.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <cstring>

#ifdef USE_RUNTIME_IMAGE_BMP
#include "bmp_decoder.h"
#endif
#ifdef USE_RUNTIME_IMAGE_JPEG
#include "jpeg_decoder.h"
#endif
#ifdef USE_RUNTIME_IMAGE_PNG
#include "png_decoder.h"
#endif

namespace esphome {
namespace runtime_image {

static const char *const TAG = "runtime_image";

inline bool is_color_on(const Color &color) {
  // This produces the most accurate monochrome conversion, but is slightly slower.
  //  return (0.2125 * color.r + 0.7154 * color.g + 0.0721 * color.b) > 127;

  // Approximation using fast integer computations; produces acceptable results
  // Equivalent to 0.25 * R + 0.5 * G + 0.25 * B
  return ((color.r >> 2) + (color.g >> 1) + (color.b >> 2)) & 0x80;
}

RuntimeImage::RuntimeImage(ImageFormat format, image::ImageType type, image::Transparency transparency,
                           image::Image *placeholder, bool is_big_endian, int fixed_width, int fixed_height)
    : Image(nullptr, 0, 0, type, transparency),
      format_(format),
      is_big_endian_(is_big_endian),
      fixed_width_(fixed_width),
      fixed_height_(fixed_height),
      placeholder_(placeholder) {}

RuntimeImage::~RuntimeImage() { this->release(); }

int RuntimeImage::resize(int width, int height) {
  // Use fixed dimensions if specified (0 means auto-resize)
  int target_width = this->fixed_width_ ? this->fixed_width_ : width;
  int target_height = this->fixed_height_ ? this->fixed_height_ : height;

  size_t result = this->resize_buffer_(target_width, target_height);
  if (result > 0 && this->progressive_display_) {
    // Update display dimensions for progressive display
    this->width_ = this->buffer_width_;
    this->height_ = this->buffer_height_;
    this->data_start_ = this->buffer_;
  }
  return result;
}

void RuntimeImage::draw_pixel(int x, int y, const Color &color) {
  if (!this->buffer_) {
    ESP_LOGE(TAG, "Buffer not allocated!");
    return;
  }
  if (x < 0 || y < 0 || x >= this->buffer_width_ || y >= this->buffer_height_) {
    ESP_LOGE(TAG, "Tried to paint a pixel (%d,%d) outside the image!", x, y);
    return;
  }

  switch (this->type_) {
    case image::IMAGE_TYPE_BINARY: {
      int pos = (x + y * this->buffer_width_) / 8;
      int bit = 7 - (x % 8);
      // Set or clear bit based on color
      if (is_color_on(color)) {
        this->buffer_[pos] |= (1 << bit);
      } else {
        this->buffer_[pos] &= ~(1 << bit);
      }
      break;
    }
    case image::IMAGE_TYPE_GRAYSCALE: {
      int pos = x + y * this->buffer_width_;
      // Convert RGB to grayscale using the more accurate luminance formula
      auto gray = static_cast<uint8_t>(0.2125 * color.r + 0.7154 * color.g + 0.0721 * color.b);
      this->buffer_[pos] = gray;
      break;
    }
    case image::IMAGE_TYPE_RGB: {
      int pos = (x + y * this->buffer_width_) * 3;
      this->buffer_[pos + 0] = color.r;
      this->buffer_[pos + 1] = color.g;
      this->buffer_[pos + 2] = color.b;
      break;
    }
    case image::IMAGE_TYPE_RGB565: {
      int pos = (x + y * this->buffer_width_) * 2;
      uint16_t rgb565 = ((color.r & 0xF8) << 8) | ((color.g & 0xFC) << 3) | (color.b >> 3);
      if (this->is_big_endian_) {
        this->buffer_[pos + 0] = (rgb565 >> 8) & 0xFF;
        this->buffer_[pos + 1] = rgb565 & 0xFF;
      } else {
        this->buffer_[pos + 0] = rgb565 & 0xFF;
        this->buffer_[pos + 1] = (rgb565 >> 8) & 0xFF;
      }
      break;
    }
  }
}

void RuntimeImage::draw(int x, int y, display::Display *display, Color color_on, Color color_off) {
  if (this->data_start_) {
    // If we have a complete image, use the base class draw method
    Image::draw(x, y, display, color_on, color_off);
  } else if (this->placeholder_) {
    // Show placeholder while the runtime image is not available
    this->placeholder_->draw(x, y, display, color_on, color_off);
  }
  // If no image is loaded and no placeholder, nothing to draw
}

bool RuntimeImage::begin_decode(size_t expected_size) {
  if (this->decoder_) {
    ESP_LOGW(TAG, "Decoding already in progress");
    return false;
  }

  this->decoder_ = this->create_decoder_();
  if (!this->decoder_) {
    ESP_LOGE(TAG, "Failed to create decoder for format %d", this->format_);
    return false;
  }

  this->total_size_ = expected_size;
  this->decoded_bytes_ = 0;

  // Initialize decoder
  int result = this->decoder_->prepare(expected_size);
  if (result < 0) {
    ESP_LOGE(TAG, "Failed to prepare decoder: %d", result);
    this->decoder_ = nullptr;
    return false;
  }

  return true;
}

int RuntimeImage::feed_data(uint8_t *data, size_t len) {
  if (!this->decoder_) {
    ESP_LOGE(TAG, "No decoder initialized");
    return -1;
  }

  int consumed = this->decoder_->decode(data, len);
  if (consumed > 0) {
    this->decoded_bytes_ += consumed;
  }

  return consumed;
}

bool RuntimeImage::end_decode() {
  if (!this->decoder_) {
    return false;
  }

  // Finalize the image for display
  if (!this->progressive_display_) {
    // Only now make the image visible
    this->width_ = this->buffer_width_;
    this->height_ = this->buffer_height_;
    this->data_start_ = this->buffer_;
  }

  // Clean up decoder
  this->decoder_ = nullptr;

  ESP_LOGD(TAG, "Decoding complete: %dx%d, %zu bytes", this->width_, this->height_, this->decoded_bytes_);
  return true;
}

void RuntimeImage::release() {
  if (this->buffer_) {
    ESP_LOGV(TAG, "Releasing buffer of size %zu", this->get_buffer_size_(this->buffer_width_, this->buffer_height_));
    this->allocator_.deallocate(this->buffer_, this->get_buffer_size_(this->buffer_width_, this->buffer_height_));
    this->buffer_ = nullptr;
    this->data_start_ = nullptr;
    this->width_ = 0;
    this->height_ = 0;
    this->buffer_width_ = 0;
    this->buffer_height_ = 0;
  }
  this->decoder_ = nullptr;
}

size_t RuntimeImage::resize_buffer_(int width, int height) {
  size_t new_size = this->get_buffer_size_(width, height);

  if (this->buffer_ && this->buffer_width_ == width && this->buffer_height_ == height) {
    // Buffer already allocated with correct size
    return new_size;
  }

  // Release old buffer if dimensions changed
  if (this->buffer_) {
    this->release();
  }

  ESP_LOGD(TAG, "Allocating buffer: %dx%d, %zu bytes", width, height, new_size);
  this->buffer_ = this->allocator_.allocate(new_size);

  if (!this->buffer_) {
    ESP_LOGE(TAG, "Failed to allocate %zu bytes. Largest free block: %zu", new_size,
             this->allocator_.get_max_free_block_size());
    return 0;
  }

  // Clear buffer
  memset(this->buffer_, 0, new_size);

  this->buffer_width_ = width;
  this->buffer_height_ = height;

  return new_size;
}

size_t RuntimeImage::get_buffer_size_(int width, int height) const {
  return (this->get_bpp() * width + 7u) / 8u * height;
}

int RuntimeImage::get_position_(int x, int y) const { return (x + y * this->buffer_width_) * this->get_bpp() / 8; }

std::unique_ptr<ImageDecoder> RuntimeImage::create_decoder_() {
  switch (this->format_) {
#ifdef USE_RUNTIME_IMAGE_BMP
    case BMP:
      return make_unique<BmpDecoder>(this);
#endif
#ifdef USE_RUNTIME_IMAGE_JPEG
    case JPEG:
      return make_unique<JpegDecoder>(this);
#endif
#ifdef USE_RUNTIME_IMAGE_PNG
    case PNG:
      return make_unique<PngDecoder>(this);
#endif
    default:
      ESP_LOGE(TAG, "Unsupported image format: %d", this->format_);
      return nullptr;
  }
}

}  // namespace runtime_image
}  // namespace esphome
