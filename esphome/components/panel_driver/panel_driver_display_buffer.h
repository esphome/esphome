#pragma once
#include "panel_driver.h"
#include "esphome/components/display/display_buffer.h"

namespace esphome {
namespace panel_driver {

class PanelDriverBuffer : public display::DisplayBuffer, public Parented<PanelDriver> {
 public:
  void setup() override {
    size_t size = this->get_height_internal() * this->get_width_internal() * this->parent_->get_pixel_bits() / 8;
    this->init_internal_(size);
    if (this->buffer_ == nullptr) {
      esph_log_e(TAG, "Buffer allocate failed  - width %u, height %u, %u bytes requested)", this->get_width_internal(),
                 this->get_height_internal(), size);
      this->mark_failed();
    }
  }

  void update() override {
    this->do_update_();
    auto width = this->parent_->get_width();
    auto height = this->parent_->get_height();
    if (this->x_high_ >= height)
      this->x_high_ = height - 1;
    if (this->y_high_ >= height)
      this->y_high_ = height - 1;
    if (this->x_high_ < this->x_low_ || this->y_high_ < this->y_low_)
      return;
    uint8_t *ptr = this->buffer_ + (this->x_low_ + this->y_low_ * width) * this->parent_->get_pixel_bits() / 8;
    if (this->x_low_ == 0 && this->x_high_ == width - 1) {
      // can copy in one go.
      this->parent_->draw_pixels_at(0, this->y_low_, width, this->y_high_ - this->y_low_ + 1, ptr);
    } else {
      // copy line-by line since they are not contiguous
      for (size_t y = this->y_low_; y <= this->y_high_; y++) {
        this->parent_->draw_pixels_at(this->x_low_, y, this->x_high_ - this->x_low_ + 1, 1, ptr);
        ptr += width * 2;
      }
    }

    this->x_low_ = width;
    this->y_low_ = height;
    this->x_high_ = 0;
    this->y_high_ = 0;
  }

  void fill(Color color) override {
    uint16_t new_color = 0;
    this->x_low_ = 0;
    this->y_low_ = 0;
    this->x_high_ = this->get_width_internal() - 1;
    this->y_high_ = this->get_height_internal() - 1;
    auto buffer_length = this->get_width_internal() * this->get_height_internal() * this->parent_->get_pixel_bits() / 8;
    new_color = this->map_color_(color);
    if (this->parent_->get_pixel_bits() == 8 || (uint8_t) (new_color >> 8) == (uint8_t) new_color) {
      // Upper and lower is equal can use quicker memset operation. Takes ~20ms.
      memset(this->buffer_, (uint8_t) new_color, buffer_length);
      return;
    }
    // Slower set of both buffers. Takes ~30ms.
    for (uint32_t i = 0; i != buffer_length; i += 2) {
      this->buffer_[i] = (uint8_t) (new_color >> 8);
      this->buffer_[i + 1] = (uint8_t) new_color;
    }
  }

  int get_height_internal() override { return this->parent_->height_; }
  int get_width_internal() override { return this->parent_->width_; }
  display::DisplayType get_display_type() override {
    return this->parent_->get_color_mode() == COLOR_MODE_MONO ? display::DisplayType::DISPLAY_TYPE_COLOR
                                                              : display::DISPLAY_TYPE_GRAYSCALE;
  }

 protected:
  uint16_t map_color_(Color color) {
    switch (this->parent_->get_pixel_bits()) {
      case 16:
        switch (this->parent_->get_color_mode()) {
          default:
            // our color mode maps to display ColorOrder by design.
            return display::ColorUtil::color_to_565(color, (display::ColorOrder) this->parent_->get_color_mode());

          case COLOR_MODE_MONO:
            return color.white << 8;
        }

      case 8:
        switch (this->parent_->get_color_mode()) {
          default:
            // our color mode maps to display ColorOrder by design.
            return display::ColorUtil::color_to_332(color, (display::ColorOrder) this->parent_->get_color_mode());

          case COLOR_MODE_MONO:
            return color.white;
            break;
        }
        break;
    }
    return 0;
  }

  virtual void draw_absolute_pixel_internal(int x, int y, Color color) override {
    if (x >= this->parent_->width_ || x < 0 || y >= this->parent_->height_ || y < 0) {
      return;
    }
    size_t pos = (y * this->get_width_internal()) + x;
    uint16_t new_color = this->map_color_(color);

    if (this->parent_->get_pixel_bits() == 16) {
      pos *= 2;
      if (this->buffer_[pos] == new_color >> 8 && this->buffer_[pos + 1] == (new_color & 0xFF))
        return;
      this->buffer_[pos] = new_color >> 8;
      this->buffer_[pos + 1] = new_color;
    } else {
      if (this->buffer_[pos] == new_color)
        return;
      this->buffer_[pos] = new_color;
    }

    // low and high watermark may speed up drawing from buffer
    if (x < this->x_low_)
      this->x_low_ = x;
    if (y < this->y_low_)
      this->y_low_ = y;
    if (x > this->x_high_)
      this->x_high_ = x;
    if (y > this->y_high_)
      this->y_high_ = y;
  }
  uint16_t x_low_{0};
  uint16_t y_low_{0};
  uint16_t x_high_{0};
  uint16_t y_high_{0};
};

}  // namespace panel_driver
}  // namespace esphome
