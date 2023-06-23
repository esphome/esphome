#pragma once

#include "esphome/core/component.h"
#include "esphome/components/display/display.h"

namespace esphome {
namespace display_buffer {

template<typename PixelFormat>
class Buffer : public PollingComponent,
                    public display::Display {
 public:
  void dump_config() override;
  void setup() override;
  void update() override;

  int get_width() override { return this->width_; }
  int get_height() override { return this->height_; }
  display::DisplayType get_display_type() override;
  display::PixelFormat get_native_pixel_format() override { return PixelFormat::FORMAT; }

  void set_width(int width) { this->width_ = width; }
  void set_height(int height) { this->height_ = height; }
  void set_output(display::Display *output) { this->output_ = output; }

  void draw_pixel_at(int x, int y, Color color) override;
  void draw(display::Display *display);

 protected:
  uint8_t *get_native_pixels_(int y) override;
  PixelFormat *get_native_pixels_(int x, int y);

 protected:
  display::Display *output_{nullptr};
  PixelFormat *buffer_{nullptr};
  int buffer_length_{0};
  int width_{0}, height_{0};
};

}  // namespace display_buffer
}  // namespace esphome
