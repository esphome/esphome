#ifdef USE_HOST
#include "sdl_esphome.h"
#include "esphome/components/display/display_color_utils.h"

namespace esphome {
namespace sdl {

void Sdl::setup() {
  ESP_LOGD(TAG, "Starting setup");
  SDL_Init(SDL_INIT_VIDEO);
  this->window_ = SDL_CreateWindow(App.get_name().c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                   this->width_, this->height_, 0);
  this->renderer_ = SDL_CreateRenderer(this->window_, -1, SDL_RENDERER_SOFTWARE);
  this->texture_ =
      SDL_CreateTexture(this->renderer_, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STATIC, this->width_, this->height_);
  SDL_SetTextureBlendMode(this->texture_, SDL_BLENDMODE_BLEND);
  ESP_LOGD(TAG, "Setup Complete");
}
void Sdl::update() {
  this->do_update_();
  if ((this->x_high_ < this->x_low_) || (this->y_high_ < this->y_low_))
    return;
  SDL_Rect rect{this->x_low_, this->y_low_, this->x_high_ + 1 - this->x_low_, this->y_high_ + 1 - this->y_low_};
  this->x_low_ = this->width_;
  this->y_low_ = this->height_;
  this->x_high_ = 0;
  this->y_high_ = 0;
  SDL_RenderCopy(this->renderer_, this->texture_, &rect, &rect);
  SDL_RenderPresent(this->renderer_);
}

void Sdl::draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                         display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) {
  SDL_Rect rect{x_start, y_start, w, h};
  if (this->rotation_ != display::DISPLAY_ROTATION_0_DEGREES || bitness != display::COLOR_BITNESS_565 || big_endian) {
    display::Display::draw_pixels_at(x_start, y_start, w, h, ptr, order, bitness, big_endian, x_offset, y_offset,
                                     x_pad);
  } else {
    auto stride = x_offset + w + x_pad;
    auto data = ptr + (stride * y_offset + x_offset) * 2;
    SDL_UpdateTexture(this->texture_, &rect, data, stride * 2);
  }
  SDL_RenderCopy(this->renderer_, this->texture_, &rect, &rect);
  SDL_RenderPresent(this->renderer_);
}

void Sdl::draw_pixel_at(int x, int y, Color color) {
  SDL_Rect rect{x, y, 1, 1};
  auto data = (display::ColorUtil::color_to_565(color, display::COLOR_ORDER_RGB));
  SDL_UpdateTexture(this->texture_, &rect, &data, 2);
  if (x < this->x_low_)
    this->x_low_ = x;
  if (y < this->y_low_)
    this->y_low_ = y;
  if (x > this->x_high_)
    this->x_high_ = x;
  if (y > this->y_high_)
    this->y_high_ = y;
}

void Sdl::loop() {
  SDL_Event e;
  if (SDL_PollEvent(&e)) {
    switch (e.type) {
      case SDL_QUIT:
        exit(0);

      case SDL_MOUSEBUTTONDOWN:
      case SDL_MOUSEBUTTONUP:
        if (e.button.button == 1) {
          this->mouse_x = e.button.x;
          this->mouse_y = e.button.y;
          this->mouse_down = e.button.state != 0;
        }
        break;

      case SDL_MOUSEMOTION:
        if (e.motion.state & 1) {
          this->mouse_x = e.button.x;
          this->mouse_y = e.button.y;
          this->mouse_down = true;
        } else {
          this->mouse_down = false;
        }
        break;

      default:
        ESP_LOGV(TAG, "Event %d", e.type);
        break;
    }
  }
}

}  // namespace sdl
}  // namespace esphome
#endif
