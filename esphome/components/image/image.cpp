#include "image.h"

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome::image {

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

  // Pixel data and frame buffers are row-major, so walking rows keeps the
  // image read and the frame buffer write sequential. When the display swaps
  // the axes before writing, walking columns keeps the write sequential
  // instead; a strided write costs more than a strided read.
  const bool rows_outer = !display->pixel_axes_swapped();
  switch (type_) {
    case IMAGE_TYPE_BINARY:
      if (rows_outer) {
        this->draw_<true, IMAGE_TYPE_BINARY>(x, y, display, color_on, color_off, x_start, x_end, y_start, y_end);
      } else {
        this->draw_<false, IMAGE_TYPE_BINARY>(x, y, display, color_on, color_off, x_start, x_end, y_start, y_end);
      }
      break;
    case IMAGE_TYPE_GRAYSCALE:
      if (rows_outer) {
        this->draw_<true, IMAGE_TYPE_GRAYSCALE>(x, y, display, color_on, color_off, x_start, x_end, y_start, y_end);
      } else {
        this->draw_<false, IMAGE_TYPE_GRAYSCALE>(x, y, display, color_on, color_off, x_start, x_end, y_start, y_end);
      }
      break;
    case IMAGE_TYPE_RGB565:
      if (rows_outer) {
        this->draw_<true, IMAGE_TYPE_RGB565>(x, y, display, color_on, color_off, x_start, x_end, y_start, y_end);
      } else {
        this->draw_<false, IMAGE_TYPE_RGB565>(x, y, display, color_on, color_off, x_start, x_end, y_start, y_end);
      }
      break;
    case IMAGE_TYPE_RGB:
      if (rows_outer) {
        this->draw_<true, IMAGE_TYPE_RGB>(x, y, display, color_on, color_off, x_start, x_end, y_start, y_end);
      } else {
        this->draw_<false, IMAGE_TYPE_RGB>(x, y, display, color_on, color_off, x_start, x_end, y_start, y_end);
      }
      break;
  }
}

template<bool ROWS_OUTER, ImageType TYPE>
void Image::draw_(int x, int y, display::Display *display, Color color_on, Color color_off, int x_start, int x_end,
                  int y_start, int y_end) {
  // The selects below fold at compile time, so each instantiation is a plain
  // nested loop with one pixel format inside.
  const int outer_start = ROWS_OUTER ? y_start : x_start;
  const int outer_end = ROWS_OUTER ? y_end : x_end;
  const int inner_start = ROWS_OUTER ? x_start : y_start;
  const int inner_end = ROWS_OUTER ? x_end : y_end;

  for (int outer = outer_start; outer < outer_end; outer++) {
    for (int inner = inner_start; inner < inner_end; inner++) {
      const int img_x = ROWS_OUTER ? inner : outer;
      const int img_y = ROWS_OUTER ? outer : inner;
      if constexpr (TYPE == IMAGE_TYPE_BINARY) {
        if (this->get_binary_pixel_(img_x, img_y)) {
          display->draw_pixel_at(x + img_x, y + img_y, color_on);
        } else if (!this->transparency_) {
          display->draw_pixel_at(x + img_x, y + img_y, color_off);
        }
      } else if constexpr (TYPE == IMAGE_TYPE_GRAYSCALE) {
        const uint32_t pos = (img_x + img_y * this->width_);
        const uint8_t gray = progmem_read_byte(this->data_start_ + pos);
        Color color = Color(gray, gray, gray, 0xFF);
        switch (this->transparency_) {
          case TRANSPARENCY_CHROMA_KEY:
            if (gray == 1) {
              continue;  // skip drawing
            }
            break;
          case TRANSPARENCY_ALPHA_CHANNEL:
            // gray is the alpha: blend from color_off to color_on, drawn opaque
            color = Color(Color::blend_channel(color_off.r, color_on.r, gray),
                          Color::blend_channel(color_off.g, color_on.g, gray),
                          Color::blend_channel(color_off.b, color_on.b, gray), 0xFF);
            break;
          default:
            break;
        }
        display->draw_pixel_at(x + img_x, y + img_y, color);
      } else {
        auto color =
            TYPE == IMAGE_TYPE_RGB565 ? this->get_rgb565_pixel_(img_x, img_y) : this->get_rgb_pixel_(img_x, img_y);
        if (color.w >= 0x80) {
          display->draw_pixel_at(x + img_x, y + img_y, color);
        }
      }
    }
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
  uint16_t rgb565 = encode_uint16(progmem_read_byte(pos + 1), progmem_read_byte(pos));
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
