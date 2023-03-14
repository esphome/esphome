#pragma once

#include "esphome/components/display/display_buffer.h"
#include "esphome/components/display/display_color_utils.h"
#include "esphome/core/color.h"
#include "esphome/core/log.h"

namespace esphome {
namespace online_image {

class Buffer {

 using BufferType = uint8_t;
 static constexpr uint8_t bytes_per_pixel = 3;

 public:
  Buffer(uint16_t width, uint16_t height, display::ImageType type): width_(width), height_(height), type_(type) {
    resize(width, height);
  }

  uint8_t *resize(uint16_t width, uint16_t height) {
    if (buffer_ && (width == width_ || height == height_)) {
        // Buffer already has the right size; nothing to do.
        ESP_LOGD("Buffer", "Buffer already has the right size");
        return get_buffer();
    }
    if (buffer_) {
        ESP_LOGD("Buffer", "Deallocating old buffer...");
        allocator_.deallocate(buffer_, get_size());
    }
    auto new_size = get_size(width, height);
    ESP_LOGD("Buffer", "Allocating new buffer of %d Bytes...", new_size);
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

  void draw_rectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, Color color) {
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
    // TODO: Handle different image types
    buffer_[bytes_per_pixel * (x + y * width_) + 0] = color.r;
    buffer_[bytes_per_pixel * (x + y * width_) + 1] = color.g;
    buffer_[bytes_per_pixel * (x + y * width_) + 2] = color.b;
    buffer_[bytes_per_pixel * (x + y * width_) + 3] = color.w;
  }

  uint16_t get_width() const { return width_; }
  uint16_t get_height() const { return height_; }
  uint8_t *get_buffer() const { return reinterpret_cast<uint8_t*>(buffer_); }
 protected:
  using Allocator = ExternalRAMAllocator<BufferType>;
  Allocator allocator_{  Allocator::Flags::ALLOW_FAILURE };

  uint32_t get_size(uint16_t width, uint16_t height) const { return bytes_per_pixel * width * height; }
  uint32_t get_size() const { return get_size(width_, height_); }
 private:
  uint16_t width_;
  uint16_t height_;
  display::ImageType type_;
  BufferType *buffer_ = nullptr;
};

}  // namespace online_image
}  // namespace esphome
