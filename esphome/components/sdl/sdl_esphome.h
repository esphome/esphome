#pragma once

#ifdef USE_HOST
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/display/display.h"
#include "esphome/components/snapshot/snapshot.h"
#define SDL_MAIN_HANDLED
#include "SDL.h"
#include <map>

namespace esphome::sdl {

constexpr static const char *const TAG = "sdl";

class Sdl final : public display::Display, public snapshot::Snapshot {
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
  void set_window_options(uint32_t window_options) { this->window_options_ = window_options; }
  void set_position(int32_t pos_x, int32_t pos_y) {
    this->pos_x_ = pos_x;
    this->pos_y_ = pos_y;
  }
  void set_headless(bool headless) { this->headless_ = headless; }
  void set_snapshot_key(int32_t keycode) { this->snapshot_key_ = keycode; }

  int get_width() override;
  int get_height() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void dump_config() override { LOG_DISPLAY("", "SDL", this); }
  template<typename F> void add_key_listener(int32_t keycode, F &&callback) {
    if (!this->key_callbacks_.count(keycode)) {
      this->key_callbacks_[keycode] = CallbackManager<void(bool)>();
    }
    this->key_callbacks_[keycode].add(std::forward<F>(callback));
  }

  int mouse_x{};
  int mouse_y{};
  bool mouse_down{};

 protected:
  int get_width_internal() override { return this->width_; }
  int get_height_internal() override { return this->height_; }
  void redraw_(SDL_Rect &rect);
  bool setup_renderer_();
  /// Release the window, surface, renderer and textures, and forget them.
  void destroy_renderer_();
  /// Log an SDL failure during setup, release anything already created, and return false.
  bool setup_failed_(const char *what);
  int snapshot_width() override { return this->width_; }
  int snapshot_height() override { return this->height_; }
  bool capture_bgr(uint8_t *dest, size_t row_stride) override;
  void handle_event_(const SDL_Event &event);
  /// The display owning the given window, or nullptr if it is not one of ours.
  static Sdl *instance_for_window_(uint32_t window_id);
  SDL_Renderer *renderer_{};
  SDL_Window *window_{};
  SDL_Texture *texture_{};
  // Offscreen render target used when headless. SDL_CreateSoftwareRenderer only borrows the
  // surface, and the renderer goes back to using it as its output whenever the capture target is
  // released, so it has to stay alive as long as the renderer does.
  SDL_Surface *surface_{};
  // Capture target, created on first snapshot.
  SDL_Texture *shot_target_{};
  std::map<int32_t, CallbackManager<void(bool)>> key_callbacks_{};
  int width_{};
  int height_{};
  uint32_t window_options_{0};
  int32_t pos_x_{SDL_WINDOWPOS_UNDEFINED};
  int32_t pos_y_{SDL_WINDOWPOS_UNDEFINED};
  int32_t snapshot_key_{0};
  uint16_t x_low_{0};
  uint16_t y_low_{0};
  uint16_t x_high_{0};
  uint16_t y_high_{0};
  bool headless_{false};
};

}  // namespace esphome::sdl

#endif
