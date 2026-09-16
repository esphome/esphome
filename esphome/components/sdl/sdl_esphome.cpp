#ifdef USE_HOST
#include "sdl_esphome.h"
#include "esphome/components/display/display_color_utils.h"

#include <cstdlib>

namespace esphome::sdl {

namespace {

// Key under which each window keeps a pointer back to its Sdl instance.
constexpr const char *const WINDOW_DATA_KEY = "esphome_sdl";

}  // namespace

int Sdl::get_width() {
  switch (this->rotation_) {
    case display::DISPLAY_ROTATION_90_DEGREES:
    case display::DISPLAY_ROTATION_270_DEGREES:
      return this->get_height_internal();
    case display::DISPLAY_ROTATION_0_DEGREES:
    case display::DISPLAY_ROTATION_180_DEGREES:
    default:
      return this->get_width_internal();
  }
}

int Sdl::get_height() {
  switch (this->rotation_) {
    case display::DISPLAY_ROTATION_0_DEGREES:
    case display::DISPLAY_ROTATION_180_DEGREES:
      return this->get_height_internal();
    case display::DISPLAY_ROTATION_90_DEGREES:
    case display::DISPLAY_ROTATION_270_DEGREES:
    default:
      return this->get_width_internal();
  }
}

void Sdl::destroy_renderer_() {
  // Reverse order of creation: the renderer refers to the window or surface it was made from.
  if (this->shot_target_ != nullptr) {
    SDL_DestroyTexture(this->shot_target_);
    this->shot_target_ = nullptr;
  }
  if (this->texture_ != nullptr) {
    SDL_DestroyTexture(this->texture_);
    this->texture_ = nullptr;
  }
  if (this->renderer_ != nullptr) {
    SDL_DestroyRenderer(this->renderer_);
    this->renderer_ = nullptr;
  }
  if (this->window_ != nullptr) {
    SDL_DestroyWindow(this->window_);
    this->window_ = nullptr;
  }
  if (this->surface_ != nullptr) {
    SDL_FreeSurface(this->surface_);
    this->surface_ = nullptr;
  }
}

bool Sdl::setup_failed_(const char *what) {
  ESP_LOGE(TAG, "%s: %s", what, SDL_GetError());
  // Give back whatever was created before the failure. Without this a half set up display leaves an
  // empty window on screen for the life of the process, still registered as an event target.
  this->destroy_renderer_();
  return false;
}

bool Sdl::setup_renderer_() {
  SDL_SetMainReady();
  if (this->headless_) {
    // SDL_INIT_VIDEO is deliberately not requested: a software renderer bound to a surface needs no
    // video device, so this works on a machine with no display server at all.
    if (SDL_Init(0) != 0)
      return this->setup_failed_("SDL_Init failed");
    this->surface_ = SDL_CreateRGBSurfaceWithFormat(0, this->width_, this->height_, 16, SDL_PIXELFORMAT_RGB565);
    if (this->surface_ == nullptr)
      return this->setup_failed_("Could not create offscreen surface");
    this->renderer_ = SDL_CreateSoftwareRenderer(this->surface_);
  } else {
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
      return this->setup_failed_("SDL_Init failed");
    this->window_ = SDL_CreateWindow(App.get_name().c_str(), this->pos_x_, this->pos_y_, this->width_, this->height_,
                                     this->window_options_);
    if (this->window_ == nullptr)
      return this->setup_failed_("Could not create window");
    // Lets loop() find the display an event belongs to, so one display does not act on another's
    // input when several windows are open.
    SDL_SetWindowData(this->window_, WINDOW_DATA_KEY, this);
    this->renderer_ = SDL_CreateRenderer(this->window_, -1, SDL_RENDERER_SOFTWARE);
  }
  if (this->renderer_ == nullptr)
    return this->setup_failed_("Could not create renderer");
  if (SDL_RenderSetLogicalSize(this->renderer_, this->width_, this->height_) != 0)
    return this->setup_failed_("Could not set renderer logical size");
  this->texture_ =
      SDL_CreateTexture(this->renderer_, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STATIC, this->width_, this->height_);
  if (this->texture_ == nullptr)
    return this->setup_failed_("Could not create texture");
  // The texture has no alpha channel, so blending is pointless. Headless it would also force a
  // different software blit path onto the 16 bit target surface.
  if (SDL_SetTextureBlendMode(this->texture_, this->headless_ ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND) != 0)
    return this->setup_failed_("Could not set texture blend mode");
  return true;
}

void Sdl::setup() {
  if (!this->setup_renderer_()) {
    this->mark_failed();
    return;
  }
  if (this->headless_) {
    // Nothing generates events, so there is nothing for loop() to do.
    this->disable_loop();
  } else if (this->snapshot_key_ != 0) {
    this->add_key_listener(this->snapshot_key_, [this](bool down) {
      if (down && !this->take_snapshot(nullptr)) {
        ESP_LOGW(TAG, "snapshot key did not write a file");
      }
    });
  }
}

void Sdl::update() {
  if (this->texture_ == nullptr)
    return;
  this->do_update_();
  if ((this->x_high_ < this->x_low_) || (this->y_high_ < this->y_low_))
    return;
  SDL_Rect rect{this->x_low_, this->y_low_, this->x_high_ + 1 - this->x_low_, this->y_high_ + 1 - this->y_low_};
  this->x_low_ = this->width_;
  this->y_low_ = this->height_;
  this->x_high_ = 0;
  this->y_high_ = 0;
  this->redraw_(rect);
}

void Sdl::redraw_(SDL_Rect &rect) {
  // Nothing to present when headless - a snapshot blits the whole texture when it needs it, so
  // doing it here as well would just burn CPU. draw_pixels_at() calls this on every partial
  // update, so it is worth skipping.
  if (this->headless_)
    return;
  SDL_RenderCopy(this->renderer_, this->texture_, &rect, &rect);
  SDL_RenderPresent(this->renderer_);
}

void Sdl::draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                         display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) {
  if (this->texture_ == nullptr)
    return;
  SDL_Rect rect{x_start, y_start, w, h};
  if (this->rotation_ != display::DISPLAY_ROTATION_0_DEGREES || bitness != display::COLOR_BITNESS_565 || big_endian) {
    Display::draw_pixels_at(x_start, y_start, w, h, ptr, order, bitness, big_endian, x_offset, y_offset, x_pad);
  } else {
    auto stride = x_offset + w + x_pad;
    auto data = ptr + (stride * y_offset + x_offset) * 2;
    SDL_UpdateTexture(this->texture_, &rect, data, stride * 2);
  }
  this->redraw_(rect);
}

void Sdl::draw_pixel_at(int x, int y, Color color) {
  if (this->texture_ == nullptr || !this->get_clipping().inside(x, y))
    return;

  if (this->rotation_ == display::DISPLAY_ROTATION_180_DEGREES) {
    x = this->width_ - x - 1;
    y = this->height_ - y - 1;
  } else if (this->rotation_ == display::DISPLAY_ROTATION_90_DEGREES) {
    auto tmp = x;
    x = this->width_ - y - 1;
    y = tmp;
  } else if (this->rotation_ == display::DISPLAY_ROTATION_270_DEGREES) {
    auto tmp = y;
    y = this->height_ - x - 1;
    x = tmp;
  }

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

void Sdl::process_key(uint32_t keycode, bool down) {
  auto callback = this->key_callbacks_.find(keycode);
  if (callback != this->key_callbacks_.end())
    callback->second(down);
}

Sdl *Sdl::instance_for_window_(uint32_t window_id) {
  SDL_Window *window = SDL_GetWindowFromID(window_id);
  if (window == nullptr)
    return nullptr;
  return static_cast<Sdl *>(SDL_GetWindowData(window, WINDOW_DATA_KEY));
}

void Sdl::handle_event_(const SDL_Event &event) {
  switch (event.type) {
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
      if (event.button.button == 1) {
        this->mouse_x = event.button.x;
        this->mouse_y = event.button.y;
        this->mouse_down = event.button.state != 0;
      }
      break;

    case SDL_MOUSEMOTION:
      if (event.motion.state & 1) {
        this->mouse_x = event.motion.x;
        this->mouse_y = event.motion.y;
        this->mouse_down = true;
      } else {
        this->mouse_down = false;
      }
      break;

    case SDL_KEYDOWN:
      // Ignore auto-repeat, otherwise holding a key floods the listeners.
      if (event.key.repeat != 0)
        break;
      ESP_LOGD(TAG, "keydown %d", event.key.keysym.sym);
      this->process_key(event.key.keysym.sym, true);
      break;

    case SDL_KEYUP:
      ESP_LOGD(TAG, "keyup %d", event.key.keysym.sym);
      this->process_key(event.key.keysym.sym, false);
      break;

    case SDL_WINDOWEVENT:
      switch (event.window.event) {
        case SDL_WINDOWEVENT_SIZE_CHANGED:
        case SDL_WINDOWEVENT_EXPOSED:
        case SDL_WINDOWEVENT_RESIZED: {
          SDL_Rect rect{0, 0, this->width_, this->height_};
          this->redraw_(rect);
          break;
        }
        default:
          break;
      }
      break;

    default:
      break;
  }
}

void Sdl::loop() {
  SDL_Event e;
  // Take everything that is waiting, not one event per loop. A touch drag produces a burst of
  // motion events, and consuming them one at a time lets the queue grow without bound, so the
  // pointer ends up acting on input from further and further in the past. Draining collapses a
  // burst to the position it ended at, which is the one the user is asking for anyway.
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT)
      exit(0);

    // Events carry the window they happened in, so send each one to the display that owns it.
    uint32_t window_id;
    switch (e.type) {
      case SDL_MOUSEBUTTONDOWN:
      case SDL_MOUSEBUTTONUP:
        window_id = e.button.windowID;
        break;
      case SDL_MOUSEMOTION:
        window_id = e.motion.windowID;
        break;
      case SDL_KEYDOWN:
      case SDL_KEYUP:
        window_id = e.key.windowID;
        break;
      case SDL_WINDOWEVENT:
        window_id = e.window.windowID;
        break;
      default:
        // Anything else, including the touch events SDL reports alongside the mouse events it
        // synthesises from them, is not used here.
        ESP_LOGV(TAG, "Event %d", e.type);
        continue;
    }

    Sdl *target = instance_for_window_(window_id);
    if (target == nullptr) {
      // Nothing to route this to: the window has gone, or it is not one of ours. Say so, otherwise
      // input that stops working leaves no trace at all.
      ESP_LOGV(TAG, "Event %d for unknown window %u", e.type, window_id);
      continue;
    }
    target->handle_event_(e);
  }
}

bool Sdl::capture_bgr(uint8_t *dest, size_t row_stride) {
  if (this->texture_ == nullptr || this->renderer_ == nullptr) {
    ESP_LOGE(TAG, "Snapshot requested but SDL is not set up");
    return false;
  }
  if (this->shot_target_ == nullptr) {
    this->shot_target_ = SDL_CreateTexture(this->renderer_, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_TARGET,
                                           this->width_, this->height_);
    if (this->shot_target_ == nullptr) {
      ESP_LOGE(TAG, "Could not create capture texture: %s", SDL_GetError());
      return false;
    }
    SDL_SetTextureBlendMode(this->shot_target_, SDL_BLENDMODE_NONE);
  }

  // Render into an offscreen target first. SDL_RenderReadPixels works in physical output pixels and
  // ignores the logical size, so reading straight off a resizable window would read more pixels than
  // there is room for.
  // Every step is checked: a failed clear or copy would otherwise be read back as a blank or stale
  // picture, written out, and reported as a snapshot that worked.
  bool ok = false;
  if (SDL_SetRenderTarget(this->renderer_, this->shot_target_) == 0) {
    ok = SDL_SetRenderDrawColor(this->renderer_, 0, 0, 0, SDL_ALPHA_OPAQUE) == 0 &&
         SDL_RenderClear(this->renderer_) == 0 &&
         SDL_RenderCopy(this->renderer_, this->texture_, nullptr, nullptr) == 0 &&
         SDL_RenderReadPixels(this->renderer_, nullptr, SDL_PIXELFORMAT_BGR24, dest, static_cast<int>(row_stride)) == 0;
    if (SDL_SetRenderTarget(this->renderer_, nullptr) != 0) {
      // Stuck rendering into shot_target_ from here on, so there's no point continuing.
      ESP_LOGE(TAG, "Could not restore the render target: %s", SDL_GetError());
      this->mark_failed();
      return false;
    }
  }
  if (!ok) {
    ESP_LOGE(TAG, "Could not capture the screen: %s", SDL_GetError());
  }
  return ok;
}

}  // namespace esphome::sdl
#endif
