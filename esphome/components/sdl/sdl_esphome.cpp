#ifdef USE_HOST
#include "sdl_esphome.h"
#include "esphome/components/display/display_color_utils.h"

#include <fcntl.h>
#include <unistd.h>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>

namespace esphome::sdl {

namespace {

// Key under which each window keeps a pointer back to its Sdl instance.
constexpr const char *const WINDOW_DATA_KEY = "esphome_sdl";

// Longest name we will build a path from. NAME_MAX is 255 and we may append a collision suffix.
constexpr size_t MAX_NAME_LENGTH = 200;
// Give up rather than spin forever if every candidate name is taken.
constexpr unsigned MAX_NAME_ATTEMPTS = 1000;

/// Reduce a user supplied name to a single safe path component. Everything outside the allowed set
/// is replaced, so "..", "/" and absolute paths cannot escape the screenshot directory.
/// Returns an empty string if nothing usable is left.
std::string sanitize_name(const char *name) {
  std::string result;
  bool all_dots = true;
  for (const char *p = name; *p != '\0' && result.size() < MAX_NAME_LENGTH; p++) {
    char c = *p;
    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '_' || c == '-'))
      c = '_';
    if (c != '.')
      all_dots = false;
    result.push_back(c);
  }
  if (all_dots)
    return "";
  if (result.size() < 4 || result.compare(result.size() - 4, 4, ".bmp") != 0)
    result += ".bmp";
  return result;
}

/// Insert "-<attempt>" before the file extension, e.g. "shot.bmp" -> "shot-1.bmp".
std::string add_suffix(const std::string &name, unsigned attempt) {
  char suffix[12];
  snprintf(suffix, sizeof(suffix), "-%u", attempt);
  auto dot = name.rfind('.');
  return name.substr(0, dot) + suffix + name.substr(dot);
}

/// Directory screenshots are written to. The environment variable lets a test redirect output
/// without rebuilding, matching how the host platform handles ESPHOME_PREFDIR.
const char *screenshot_dir() {
  const char *dir = getenv("ESPHOME_SCREENSHOT_DIR");  // NOLINT(concurrency-mt-unsafe)
  return dir != nullptr && dir[0] != '\0' ? dir : ESPHOME_SDL_SCREENSHOT_DIR;
}

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

bool Sdl::setup_renderer_() {
  SDL_SetMainReady();
  if (this->headless_) {
    // SDL_INIT_VIDEO is deliberately not requested: a software renderer bound to a surface needs no
    // video device, so this works on a machine with no display server at all.
    if (SDL_Init(0) != 0) {
      ESP_LOGE(TAG, "SDL_Init failed: %s", SDL_GetError());
      return false;
    }
    this->surface_ = SDL_CreateRGBSurfaceWithFormat(0, this->width_, this->height_, 16, SDL_PIXELFORMAT_RGB565);
    if (this->surface_ == nullptr) {
      ESP_LOGE(TAG, "Could not create offscreen surface: %s", SDL_GetError());
      return false;
    }
    this->renderer_ = SDL_CreateSoftwareRenderer(this->surface_);
  } else {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
      ESP_LOGE(TAG, "SDL_Init failed: %s", SDL_GetError());
      return false;
    }
    this->window_ = SDL_CreateWindow(App.get_name().c_str(), this->pos_x_, this->pos_y_, this->width_, this->height_,
                                     this->window_options_);
    if (this->window_ == nullptr) {
      ESP_LOGE(TAG, "Could not create window: %s", SDL_GetError());
      return false;
    }
    // Lets loop() find the display an event belongs to, so one display does not act on another's
    // input when several windows are open.
    SDL_SetWindowData(this->window_, WINDOW_DATA_KEY, this);
    this->renderer_ = SDL_CreateRenderer(this->window_, -1, SDL_RENDERER_SOFTWARE);
  }
  if (this->renderer_ == nullptr) {
    ESP_LOGE(TAG, "Could not create renderer: %s", SDL_GetError());
    return false;
  }
  SDL_RenderSetLogicalSize(this->renderer_, this->width_, this->height_);
  this->texture_ =
      SDL_CreateTexture(this->renderer_, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STATIC, this->width_, this->height_);
  if (this->texture_ == nullptr) {
    ESP_LOGE(TAG, "Could not create texture: %s", SDL_GetError());
    return false;
  }
  // The texture has no alpha channel, so blending is pointless. Headless it would also force a
  // different software blit path onto the 16 bit target surface.
  SDL_SetTextureBlendMode(this->texture_, this->headless_ ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND);
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
  } else if (this->screenshot_key_ != 0) {
    this->add_key_listener(this->screenshot_key_, [this](bool down) {
      if (down)
        this->save_screenshot(nullptr);
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
  // Nothing to present when headless - save_screenshot() blits the whole texture when it needs it,
  // so doing it here as well would just burn CPU (draw_pixel_at() calls this once per pixel).
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
    if (target != nullptr)
      target->handle_event_(e);
  }
}

bool Sdl::write_bmp_(SDL_Surface *surface, const std::string &name, bool exact) {
  const std::string dir = screenshot_dir();
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec) {
    ESP_LOGE(TAG, "Could not create screenshot directory %s: %s", dir.c_str(), ec.message().c_str());
    return false;
  }

  // O_EXCL guarantees we never write over a file that is already there.
  std::string path;
  int fd = -1;
  for (unsigned attempt = 0; attempt < MAX_NAME_ATTEMPTS; attempt++) {
    path = dir + "/" + (attempt == 0 ? name : add_suffix(name, attempt));
    fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0644);
    if (fd >= 0)
      break;
    if (errno != EEXIST) {
      ESP_LOGE(TAG, "Could not create %s: %s", path.c_str(), strerror(errno));
      return false;
    }
    if (exact) {
      // The caller asked for this exact name, so silently writing somewhere else would be worse
      // than failing - a test asserting on the path would pick up a stale file.
      ESP_LOGE(TAG, "Screenshot %s already exists, not overwriting", path.c_str());
      return false;
    }
  }
  if (fd < 0) {
    ESP_LOGE(TAG, "Could not find an unused name for %s in %s", name.c_str(), dir.c_str());
    return false;
  }

  FILE *fp = fdopen(fd, "wb");
  if (fp == nullptr) {
    ESP_LOGE(TAG, "Could not open %s: %s", path.c_str(), strerror(errno));
    ::close(fd);
    ::unlink(path.c_str());
    return false;
  }
  SDL_RWops *rw = SDL_RWFromFP(fp, SDL_TRUE);
  if (rw == nullptr) {
    ESP_LOGE(TAG, "SDL_RWFromFP failed: %s", SDL_GetError());
    fclose(fp);
    ::unlink(path.c_str());
    return false;
  }
  // SDL_SaveBMP_RW closes rw (and with it fp) on every path.
  if (SDL_SaveBMP_RW(surface, rw, 1) != 0) {
    ESP_LOGE(TAG, "Could not write %s: %s", path.c_str(), SDL_GetError());
    // Leave no truncated file behind - it would block a retry under the same name.
    ::unlink(path.c_str());
    return false;
  }
  ESP_LOGI(TAG, "Screenshot written to %s", path.c_str());
  return true;
}

bool Sdl::save_screenshot(const char *filename) {
  if (this->texture_ == nullptr || this->renderer_ == nullptr) {
    ESP_LOGE(TAG, "Screenshot requested but SDL is not set up");
    return false;
  }

  std::string name;
  bool exact = false;
  if (filename != nullptr) {
    name = sanitize_name(filename);
    exact = !name.empty();
  }
  if (name.empty()) {
    struct timespec now {};
    clock_gettime(CLOCK_REALTIME, &now);
    struct tm tm_buf {};
    localtime_r(&now.tv_sec, &tm_buf);
    char stamp[32];
    // ::strftime - display::Display has an unrelated member of the same name
    ::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &tm_buf);
    char buffer[MAX_NAME_LENGTH];
    snprintf(buffer, sizeof(buffer), "%s-%s-%03ld.bmp", this->screenshot_prefix_, stamp, now.tv_nsec / 1000000);
    name = buffer;
  }

  // BGR24 is the BMP channel order, so SDL_SaveBMP_RW can write the surface without converting it.
  SDL_Surface *shot = SDL_CreateRGBSurfaceWithFormat(0, this->width_, this->height_, 24, SDL_PIXELFORMAT_BGR24);
  if (shot == nullptr) {
    ESP_LOGE(TAG, "Could not create capture surface: %s", SDL_GetError());
    return false;
  }
  if (this->shot_target_ == nullptr) {
    this->shot_target_ = SDL_CreateTexture(this->renderer_, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_TARGET,
                                           this->width_, this->height_);
    if (this->shot_target_ == nullptr) {
      ESP_LOGE(TAG, "Could not create capture texture: %s", SDL_GetError());
      SDL_FreeSurface(shot);
      return false;
    }
    SDL_SetTextureBlendMode(this->shot_target_, SDL_BLENDMODE_NONE);
  }

  // Render into an offscreen target first. SDL_RenderReadPixels works in physical output pixels and
  // ignores the logical size, so reading straight off a resizable window would read more pixels than
  // the surface holds.
  bool ok = false;
  if (SDL_SetRenderTarget(this->renderer_, this->shot_target_) == 0) {
    SDL_SetRenderDrawColor(this->renderer_, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(this->renderer_);
    SDL_RenderCopy(this->renderer_, this->texture_, nullptr, nullptr);
    ok = SDL_RenderReadPixels(this->renderer_, nullptr, SDL_PIXELFORMAT_BGR24, shot->pixels, shot->pitch) == 0;
    SDL_SetRenderTarget(this->renderer_, nullptr);
  }
  if (!ok) {
    ESP_LOGE(TAG, "Could not read back the screen: %s", SDL_GetError());
    SDL_FreeSurface(shot);
    return false;
  }

  ok = this->write_bmp_(shot, name, exact);
  SDL_FreeSurface(shot);
  return ok;
}

}  // namespace esphome::sdl
#endif
