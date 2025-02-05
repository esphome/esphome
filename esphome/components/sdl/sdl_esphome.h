#pragma once

#ifdef USE_HOST
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/display/display.h"
#define SDL_MAIN_HANDLED
#include "SDL.h"
#include <map>

namespace esphome {
namespace sdl {

constexpr static const char *const TAG = "sdl";

class Sdl : public display::Display {
 public:
  display::DisplayType get_display_type() override { return display::DISPLAY_TYPE_COLOR; }
  void update() override;
  void loop() override;
  void setup() override;
  void draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                      display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) override;
  void draw_pixel_at(int x, int y, Color color) override;
  void process_key(uint32_t keycode, bool down);
  void set_dimensions(uint16_t width, uint16_t height) {
    this->width_ = width;
    this->height_ = height;
  }
  int get_width() override { return this->width_; }
  int get_height() override { return this->height_; }
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void dump_config() override { LOG_DISPLAY("", "SDL", this); }
  void add_key_listener(int32_t keycode, std::function<void(bool)> &&callback) {
    if (!this->key_callbacks_.count(keycode)) {
      this->key_callbacks_[keycode] = CallbackManager<void(bool)>();
    }
    this->key_callbacks_[keycode].add(std::move(callback));
  }

  int mouse_x{};
  int mouse_y{};
  bool mouse_down{};

 protected:
  int get_width_internal() override { return this->width_; }
  int get_height_internal() override { return this->height_; }
  void redraw_(SDL_Rect &rect);
  int width_{};
  int height_{};
  SDL_Renderer *renderer_{};
  SDL_Window *window_{};
  SDL_Texture *texture_{};
  uint16_t x_low_{0};
  uint16_t y_low_{0};
  uint16_t x_high_{0};
  uint16_t y_high_{0};
  std::map<int32_t, CallbackManager<void(bool)>> key_callbacks_{};
};
}  // namespace sdl
}  // namespace esphome

#endif
