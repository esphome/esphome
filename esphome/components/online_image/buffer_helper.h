#pragma once

#include "esphome/components/display/display_buffer.h"
#include "esphome/components/display/display_color_utils.h"
#include "esphome/core/color.h"
#include "esphome/core/log.h"

namespace esphome {
namespace online_image {

class Buffer {

 using BufferType = uint8_t;

 public:
  Buffer(uint16_t width, uint16_t height, display::ImageType type)
    : width_(width),
      height_(height),
      type_(type),
      bits_per_pixel_(get_bits_per_pixel(type)) {
    resize(width, height);
  }

  uint8_t *resize(uint16_t width, uint16_t height) {
    if (buffer_ && (width == width_ || height == height_)) {
        // Buffer already has the right size; nothing to do.
        ESP_LOGD("Buffer", "Buffer already has the right size");
        return get_buffer();
    }
    release();

    auto new_size = get_size(width, height);
    ESP_LOGD("Buffer", "Allocating new buffer of %d Bytes...", new_size);
    ESP_LOGD("Buffer", "Bits per pixel: %d", bits_per_pixel_);
    delay_microseconds_safe(2000);
    buffer_ = allocator_.allocate(new_size);
    memset(buffer_, 0, new_size);

    if (buffer_) {
        this->width_ = width;
        this->height_ = height;
      ESP_LOGD("Buffer", "New size: (%d, %d)", this->width_, this->height_);
    } else {
      ESP_LOGW("Buffer", "allocation failed");
    }
    return get_buffer();
  }

  void release() {
    if (buffer_) {
      ESP_LOGD("Buffer", "Deallocating old buffer...");
      allocator_.deallocate(buffer_, get_size());
      buffer_ = nullptr;
    }
  }

  void draw_rectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, Color color) {
    if (x >= width_ || y >= height_) {
      return;
    }
    if (x + w >= width_) {
      w = width_ - x;
    }
    if (y + h >= height_) {
      h = height_ - y;
    }
    for (auto i = x; i < x + w ; i++) {
      for (auto j = y; j < y + h ; j++) {
        draw_pixel(i, j, color);
      }
    }
  }

  void draw_pixel(uint16_t x, uint16_t y, Color color) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
      ESP_LOGE("Buffer", "Tried to paint a pixel (%d,%d) outside the image!", x, y);
    }
    uint32_t pos = (x + y * width_) * get_bytes_per_pixel(type_);
    switch (type_) {
      case display::ImageType::IMAGE_TYPE_TRANSPARENT_BINARY:
      case display::ImageType::IMAGE_TYPE_BINARY: {
        uint32_t byte_num = pos / 8;
        uint8_t bit_num = pos % 8;
        if (color.is_on()) {
          buffer_[byte_num] |= 1 << bit_num;
        } else {
          buffer_[byte_num] &= !(1 << bit_num);
        }
        break;
      }
      case display::ImageType::IMAGE_TYPE_GRAYSCALE: {
        buffer_[pos] = static_cast<uint8_t>(0.2125 * color.r + 0.7154 * color.g + 0.0721 * color.b);
        break;
      }
      case display::ImageType::IMAGE_TYPE_RGB565: {
        uint16_t col565 = display::ColorUtil::color_to_565(color);

        buffer_[pos + 0] = static_cast<uint8_t>((col565 >> 8) & 0xFF);
        buffer_[pos + 1] = static_cast<uint8_t>(col565 & 0xFF);
        break;
      }
      case display::ImageType::IMAGE_TYPE_RGB24: {
      default:
        buffer_[pos + 0] = static_cast<uint8_t>(color.r);
        buffer_[pos + 1] = static_cast<uint8_t>(color.g);
        buffer_[pos + 2] = static_cast<uint8_t>(color.b);
        break;
      }
    }
  }

  Color get_pixel(uint16_t x, uint16_t y) const {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
      ESP_LOGE("Buffer", "Tried to read a pixel (%d,%d) outside the image!", x, y);
      return Color::BLACK;
    }
    uint32_t pos = (x + y * width_) * get_bytes_per_pixel(type_);
    switch (type_) {
      case display::ImageType::IMAGE_TYPE_TRANSPARENT_BINARY:
      case display::ImageType::IMAGE_TYPE_BINARY: {
        uint32_t byte_num = pos / 8;
        uint8_t bit_num = pos % 8;
        return buffer_[byte_num] & bit_num ? Color::WHITE : Color::BLACK; 
      }
      case display::ImageType::IMAGE_TYPE_GRAYSCALE: {
        uint8_t color = buffer_[pos];
        return Color(color, color, color, 0xFF);
      }
      case display::ImageType::IMAGE_TYPE_RGB565: {
        uint16_t col565 = encode_uint16(buffer_[pos], buffer_[pos + 1]);

        return Color( static_cast<uint8_t>((col565 >> 8) & 0xF8),
                      static_cast<uint8_t>((col565 & 0x7E0) >> 3), 
                      static_cast<uint8_t>((col565 & 0x1F) << 3));
      } 
      case display::ImageType::IMAGE_TYPE_RGBA: {
        return Color(buffer_[pos], buffer_[pos + 1], buffer_[pos + 2], buffer_[pos + 3]);
      }
      case display::ImageType::IMAGE_TYPE_RGB24:
      default: {
        return Color(buffer_[pos], buffer_[pos + 1], buffer_[pos + 2], 0xFF);
      }
    }
  }

  uint16_t get_width() const { return width_; }
  uint16_t get_height() const { return height_; }
  uint8_t *get_buffer() const { return reinterpret_cast<uint8_t*>(buffer_); }
 protected:
  using Allocator = ExternalRAMAllocator<BufferType>;
  Allocator allocator_{  Allocator::Flags::ALLOW_FAILURE };

  uint32_t get_size(uint16_t width, uint16_t height) const { return std::ceil(bits_per_pixel_ * width * height / 8.0); }
  uint32_t get_size() const { return get_size(width_, height_); }

 private:
  static uint8_t get_bits_per_pixel(display::ImageType type) {
    switch (type) {
      case display::ImageType::IMAGE_TYPE_BINARY:
        return 1;
      case display::ImageType::IMAGE_TYPE_GRAYSCALE:
        return 8;
      case display::ImageType::IMAGE_TYPE_RGB565:
        return 16;
      default:
      case display::ImageType::IMAGE_TYPE_RGB24:
        return 24;
      case display::ImageType::IMAGE_TYPE_TRANSPARENT_BINARY:
        return 32;
    }
  }
  static uint8_t get_bytes_per_pixel(display::ImageType type) {
    return std::ceil(get_bits_per_pixel(type) / 8);
  }

  const uint8_t bits_per_pixel_;
  uint16_t width_;
  uint16_t height_;
  display::ImageType type_;
  BufferType *buffer_ = nullptr;
};

}  // namespace online_image
}  // namespace esphome
