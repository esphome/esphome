#include "runtime_image.h"
#include "image_decoder.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <algorithm>
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

namespace esphome::runtime_image {

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
      fixed_width_(fixed_width),
      fixed_height_(fixed_height),
      placeholder_(placeholder),
      is_big_endian_(is_big_endian) {}

RuntimeImage::~RuntimeImage() { this->release(); }

int RuntimeImage::resize(int width, int height) {
  // Use fixed dimensions if specified (0 means auto-resize)
  int target_width = this->fixed_width_ ? this->fixed_width_ : width;
  int target_height = this->fixed_height_ ? this->fixed_height_ : height;

  // When both fixed dimensions are set, scale uniformly to preserve aspect ratio
  if (this->fixed_width_ && this->fixed_height_ && width > 0 && height > 0) {
    float scale =
        std::min(static_cast<float>(this->fixed_width_) / width, static_cast<float>(this->fixed_height_) / height);
    target_width = static_cast<int>(width * scale);
    target_height = static_cast<int>(height * scale);
  }

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
      const uint32_t width_8 = ((this->buffer_width_ + 7u) / 8u) * 8u;
      uint32_t pos = x + y * width_8;
      auto bitno = 0x80 >> (pos % 8u);
      pos /= 8u;
      auto on = is_color_on(color);
      if (this->has_transparency() && color.w < 0x80)
        on = false;
      if (on) {
        this->buffer_[pos] |= bitno;
      } else {
        this->buffer_[pos] &= ~bitno;
      }
      break;
    }
    case image::IMAGE_TYPE_GRAYSCALE: {
      uint32_t pos = this->get_position_(x, y);
      auto gray = static_cast<uint8_t>(0.2125 * color.r + 0.7154 * color.g + 0.0721 * color.b);
      if (this->transparency_ == image::TRANSPARENCY_CHROMA_KEY) {
        if (gray == 1) {
          gray = 0;
        }
        if (color.w < 0x80) {
          gray = 1;
        }
      } else if (this->transparency_ == image::TRANSPARENCY_ALPHA_CHANNEL) {
        if (color.w != 0xFF)
          gray = color.w;
      }
      this->buffer_[pos] = gray;
      break;
    }
    case image::IMAGE_TYPE_RGB565: {
      uint32_t pos = this->get_position_(x, y);
      Color mapped_color = color;
      this->map_chroma_key(mapped_color);
      uint16_t rgb565 = display::ColorUtil::color_to_565(mapped_color);
      if (this->is_big_endian_) {
        this->buffer_[pos + 0] = static_cast<uint8_t>((rgb565 >> 8) & 0xFF);
        this->buffer_[pos + 1] = static_cast<uint8_t>(rgb565 & 0xFF);
      } else {
        this->buffer_[pos + 0] = static_cast<uint8_t>(rgb565 & 0xFF);
        this->buffer_[pos + 1] = static_cast<uint8_t>((rgb565 >> 8) & 0xFF);
      }
      if (this->transparency_ == image::TRANSPARENCY_ALPHA_CHANNEL) {
        this->buffer_[pos + 2] = color.w;
      }
      break;
    }
    case image::IMAGE_TYPE_RGB: {
      uint32_t pos = this->get_position_(x, y);
      Color mapped_color = color;
      this->map_chroma_key(mapped_color);
      this->buffer_[pos + 0] = mapped_color.r;
      this->buffer_[pos + 1] = mapped_color.g;
      this->buffer_[pos + 2] = mapped_color.b;
      if (this->transparency_ == image::TRANSPARENCY_ALPHA_CHANNEL) {
        this->buffer_[pos + 3] = color.w;
      }
      break;
    }
  }
}

void RuntimeImage::map_chroma_key(Color &color) {
  if (this->transparency_ == image::TRANSPARENCY_CHROMA_KEY) {
    if (color.g == 1 && color.r == 0 && color.b == 0) {
      color.g = 0;
    }
    if (color.w < 0x80) {
      color.r = 0;
      color.g = this->type_ == image::IMAGE_TYPE_RGB565 ? 4 : 1;
      color.b = 0;
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

bool RuntimeImage::is_decode_finished() const {
  if (!this->decoder_) {
    return false;
  }
  return this->decoder_->is_finished();
}

void RuntimeImage::release() {
  this->release_buffer_();
  // Reset decoder separately — release() can be called from within the decoder
  // (via set_size -> resize -> resize_buffer_), so we must not destroy the decoder here.
  // The decoder lifecycle is managed by begin_decode()/end_decode().
  this->decoder_ = nullptr;
}

void RuntimeImage::release_buffer_() {
  if (this->buffer_) {
    ESP_LOGV(TAG, "Releasing buffer of size %zu", this->get_buffer_size_(this->buffer_width_, this->buffer_height_));
    RAMAllocator<uint8_t> allocator;
    allocator.deallocate(this->buffer_, this->get_buffer_size_(this->buffer_width_, this->buffer_height_));
    this->buffer_ = nullptr;
    this->data_start_ = nullptr;
    this->width_ = 0;
    this->height_ = 0;
    this->buffer_width_ = 0;
    this->buffer_height_ = 0;
  }
}

size_t RuntimeImage::resize_buffer_(int width, int height) {
  size_t new_size = this->get_buffer_size_(width, height);

  if (this->buffer_ && this->buffer_width_ == width && this->buffer_height_ == height) {
    // Buffer already allocated with correct size
    return new_size;
  }

  // Release old buffer if dimensions changed
  if (this->buffer_) {
    this->release_buffer_();
  }

  ESP_LOGD(TAG, "Allocating buffer: %dx%d, %zu bytes", width, height, new_size);
  RAMAllocator<uint8_t> allocator;
  this->buffer_ = allocator.allocate(new_size);

  if (!this->buffer_) {
    ESP_LOGE(TAG, "Failed to allocate %zu bytes. Largest free block: %zu", new_size,
             allocator.get_max_free_block_size());
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

}  // namespace esphome::runtime_image
