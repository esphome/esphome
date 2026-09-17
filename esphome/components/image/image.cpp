#include "image.h"

#include <algorithm>

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome::image {

// Walks the clipped image range with rows (ROWS_OUTER) or columns as the outer
// loop. The coordinate selection folds at compile time, and the pixel body must
// be ESPHOME_ALWAYS_INLINE: at -Os GCC otherwise leaves it out of line and calls
// it once per pixel.
template<bool ROWS_OUTER, typename F>
static void for_each_pixel(int x_start, int x_end, int y_start, int y_end, F &&f) {
  const int outer_start = ROWS_OUTER ? y_start : x_start;
  const int outer_end = ROWS_OUTER ? y_end : x_end;
  const int inner_start = ROWS_OUTER ? x_start : y_start;
  const int inner_end = ROWS_OUTER ? x_end : y_end;
  for (int outer = outer_start; outer < outer_end; outer++) {
    for (int inner = inner_start; inner < inner_end; inner++) {
      f(ROWS_OUTER ? inner : outer, ROWS_OUTER ? outer : inner);
    }
  }
}

void Image::draw(int x, int y, display::Display *display, Color color_on, Color color_off) {
  int x_start = 0;
  int y_start = 0;
  int x_end = width_;
  int y_end = height_;

  auto clipping = display->get_clipping();
  if (clipping.is_set()) {
    if (clipping.x > x)
      x_start += clipping.x - x;
    if (clipping.y > y)
      y_start += clipping.y - y;
    if (x_end > clipping.x2() - x)
      x_end = clipping.x2() - x;
    if (y_end > clipping.y2() - y)
      y_end = clipping.y2() - y;
  }

  // Clamp to the display so the bulk path below never writes off screen; the
  // per pixel path bounds checks itself, this just saves it the work.
  if (x < 0)
    x_start = std::max(x_start, -x);
  if (y < 0)
    y_start = std::max(y_start, -y);
  x_end = std::min(x_end, display->get_width() - x);
  y_end = std::min(y_end, display->get_height() - y);
  if (x_end <= x_start || y_end <= y_start)
    return;

  // Opaque RGB565 and RGB pixels are already in the layout draw_pixels_at()
  // takes, so hand the window to the display: drivers with a bulk path blit
  // it, the others walk it per pixel as before.
  if (this->transparency_ == TRANSPARENCY_OPAQUE &&
      (this->type_ == IMAGE_TYPE_RGB565 || this->type_ == IMAGE_TYPE_RGB)) {
    const bool rgb565 = this->type_ == IMAGE_TYPE_RGB565;
    display->draw_pixels_at(x + x_start, y + y_start, x_end - x_start, y_end - y_start, this->data_start_,
                            display::COLOR_ORDER_RGB, rgb565 ? display::COLOR_BITNESS_565 : display::COLOR_BITNESS_888,
                            rgb565 && this->big_endian_, x_start, y_start, this->width_ - x_end);
    return;
  }

  // Pixel data and frame buffers are row-major, so walking rows keeps the
  // image read and the frame buffer write sequential. When the display swaps
  // the axes before writing, walking columns keeps the write sequential
  // instead; a strided write costs more than a strided read.
#ifdef USE_PSRAM
  if (display->pixel_axes_swapped()) {
    this->draw_<false>(x, y, display, color_on, color_off, x_start, x_end, y_start, y_end);
    return;
  }
#endif
  // Without PSRAM the frame buffer is in uncached RAM, where the strided write
  // is free, so one copy of the loops is enough.
  this->draw_<true>(x, y, display, color_on, color_off, x_start, x_end, y_start, y_end);
}

template<bool ROWS_OUTER>
void Image::draw_(int x, int y, display::Display *display, Color color_on, Color color_off, int x_start, int x_end,
                  int y_start, int y_end) {
  // Local copies: draw_pixel_at() is an opaque virtual call, so members read
  // in the loop would otherwise be reloaded every pixel.
  const Transparency transparency = this->transparency_;
  switch (this->type_) {
    case IMAGE_TYPE_BINARY:
      for_each_pixel<ROWS_OUTER>(x_start, x_end, y_start, y_end, [&](int img_x, int img_y) ESPHOME_ALWAYS_INLINE {
        if (this->get_binary_pixel_(img_x, img_y)) {
          display->draw_pixel_at(x + img_x, y + img_y, color_on);
        } else if (transparency == TRANSPARENCY_OPAQUE) {
          display->draw_pixel_at(x + img_x, y + img_y, color_off);
        }
      });
      break;
    case IMAGE_TYPE_GRAYSCALE: {
      const int width = this->width_;
      const uint8_t *const data = this->data_start_;
      for_each_pixel<ROWS_OUTER>(x_start, x_end, y_start, y_end, [&](int img_x, int img_y) ESPHOME_ALWAYS_INLINE {
        const uint8_t gray = progmem_read_byte(data + img_x + img_y * width);
        Color color = Color(gray, gray, gray, 0xFF);
        if (transparency == TRANSPARENCY_CHROMA_KEY) {
          if (gray == 1) {
            return;  // transparent pixel, skip drawing
          }
        } else if (transparency == TRANSPARENCY_ALPHA_CHANNEL) {
          // gray is the alpha: blend from color_off to color_on, drawn opaque
          color = Color(Color::blend_channel(color_off.r, color_on.r, gray),
                        Color::blend_channel(color_off.g, color_on.g, gray),
                        Color::blend_channel(color_off.b, color_on.b, gray), 0xFF);
        }
        display->draw_pixel_at(x + img_x, y + img_y, color);
      });
      break;
    }
    case IMAGE_TYPE_RGB565:
      for_each_pixel<ROWS_OUTER>(x_start, x_end, y_start, y_end, [&](int img_x, int img_y) ESPHOME_ALWAYS_INLINE {
        auto color = this->get_rgb565_pixel_(img_x, img_y);
        if (color.w >= 0x80) {
          display->draw_pixel_at(x + img_x, y + img_y, color);
        }
      });
      break;
    case IMAGE_TYPE_RGB:
      for_each_pixel<ROWS_OUTER>(x_start, x_end, y_start, y_end, [&](int img_x, int img_y) ESPHOME_ALWAYS_INLINE {
        auto color = this->get_rgb_pixel_(img_x, img_y);
        if (color.w >= 0x80) {
          display->draw_pixel_at(x + img_x, y + img_y, color);
        }
      });
      break;
  }
}
Color Image::get_pixel(int x, int y, const Color color_on, const Color color_off) const {
  if (x < 0 || x >= this->width_ || y < 0 || y >= this->height_)
    return color_off;
  switch (this->type_) {
    case IMAGE_TYPE_BINARY:
      if (this->get_binary_pixel_(x, y))
        return color_on;
      return color_off;
    case IMAGE_TYPE_GRAYSCALE:
      return this->get_grayscale_pixel_(x, y);
    case IMAGE_TYPE_RGB565:
      return this->get_rgb565_pixel_(x, y);
    case IMAGE_TYPE_RGB:
      return this->get_rgb_pixel_(x, y);
    default:
      return color_off;
  }
}
#ifdef USE_LVGL
lv_image_dsc_t *Image::get_lv_image_dsc() {
  // lazily construct lvgl image_dsc.
  if (this->dsc_.data != this->data_start_) {
    this->dsc_.data = this->data_start_;
    this->dsc_.header.reserved_2 = 0;
    this->dsc_.header.stride = this->get_width_stride();
    this->dsc_.header.w = this->width_;
    this->dsc_.header.h = this->height_;
    this->dsc_.data_size = this->get_width_stride() * this->get_height();
    switch (this->get_type()) {
      case IMAGE_TYPE_BINARY:
        this->dsc_.header.cf = LV_COLOR_FORMAT_A1;
        break;

      case IMAGE_TYPE_GRAYSCALE:
        this->dsc_.header.cf = LV_COLOR_FORMAT_A8;
        break;

      case IMAGE_TYPE_RGB:
        switch (this->transparency_) {
          case TRANSPARENCY_ALPHA_CHANNEL:
            this->dsc_.header.cf = LV_COLOR_FORMAT_ARGB8888;
            break;
          case TRANSPARENCY_CHROMA_KEY:
          default:
            this->dsc_.header.cf = LV_COLOR_FORMAT_RGB888;
            break;
        }
        break;

      case IMAGE_TYPE_RGB565:
        switch (this->transparency_) {
          case TRANSPARENCY_ALPHA_CHANNEL:
            this->dsc_.header.cf = LV_COLOR_FORMAT_RGB565A8;
            break;
          default:
            this->dsc_.header.cf = LV_COLOR_FORMAT_RGB565;
        }
        break;
    }
  }
  return &this->dsc_;
}
#endif  // USE_LVGL

bool Image::get_binary_pixel_(int x, int y) const {
  const uint32_t width_8 = ((this->width_ + 7u) / 8u) * 8u;
  const uint32_t pos = x + y * width_8;
  return progmem_read_byte(this->data_start_ + (pos / 8u)) & (0x80 >> (pos % 8u));
}
Color Image::get_rgb_pixel_(int x, int y) const {
  const uint32_t pos = (x + y * this->width_) * this->bpp_ / 8;
  Color color = Color(progmem_read_byte(this->data_start_ + pos + 2), progmem_read_byte(this->data_start_ + pos + 1),
                      progmem_read_byte(this->data_start_ + pos + 0), 0xFF);

  switch (this->transparency_) {
    case TRANSPARENCY_CHROMA_KEY:
      if (color.g == 1 && color.r == 0 && color.b == 0) {
        // (0, 1, 0) has been defined as transparent color for non-alpha images.
        color.w = 0;
      }
      break;
    case TRANSPARENCY_ALPHA_CHANNEL:
      color.w = progmem_read_byte(this->data_start_ + (pos + 3));
      break;
    default:
      break;
  }
  return color;
}
Color Image::get_rgb565_pixel_(int x, int y) const {
  const uint8_t *pos = this->data_start_ + (x + y * this->width_) * this->bpp_ / 8;
  const uint16_t rgb565 = this->big_endian_ ? encode_uint16(progmem_read_byte(pos), progmem_read_byte(pos + 1))
                                            : encode_uint16(progmem_read_byte(pos + 1), progmem_read_byte(pos));
  auto r = (rgb565 & 0xF800) >> 11;
  auto g = (rgb565 & 0x07E0) >> 5;
  auto b = rgb565 & 0x001F;
  auto a = 0xFF;
  switch (this->transparency_) {
    case TRANSPARENCY_ALPHA_CHANNEL:
      a = progmem_read_byte(this->data_start_ + this->width_ * this->height_ * 2 + (x + y * this->width_));
      break;
    case TRANSPARENCY_CHROMA_KEY:
      if (rgb565 == 0x0020)
        a = 0;
      break;
    default:
      break;
  }
  return Color((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2), a);
}

Color Image::get_grayscale_pixel_(int x, int y) const {
  const uint32_t pos = (x + y * this->width_);
  const uint8_t gray = progmem_read_byte(this->data_start_ + pos);
  switch (this->transparency_) {
    case TRANSPARENCY_CHROMA_KEY:
      if (gray == 1)
        return Color(0, 0, 0, 0);
      return Color(gray, gray, gray, 0xFF);
    case TRANSPARENCY_ALPHA_CHANNEL:
      return Color(0, 0, 0, gray);
    default:
      return Color(gray, gray, gray, 0xFF);
  }
}
int Image::get_width() const { return this->width_; }
int Image::get_height() const { return this->height_; }
ImageType Image::get_type() const { return this->type_; }
Image::Image(const uint8_t *data_start, int width, int height, ImageType type, Transparency transparency)
    : width_(width), height_(height), type_(type), data_start_(data_start), transparency_(transparency) {
  switch (this->type_) {
    case IMAGE_TYPE_BINARY:
      this->bpp_ = 1;
      break;
    case IMAGE_TYPE_GRAYSCALE:
      this->bpp_ = 8;
      break;
    case IMAGE_TYPE_RGB565:
      this->bpp_ = 16;
      break;
    case IMAGE_TYPE_RGB:
      this->bpp_ = this->transparency_ == TRANSPARENCY_ALPHA_CHANNEL ? 32 : 24;
      break;
  }
}

}  // namespace esphome::image
