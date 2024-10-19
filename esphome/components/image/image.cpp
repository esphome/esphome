#include "image.h"

#include "esphome/core/hal.h"

namespace esphome {
namespace image {

void Image::draw(int x, int y, display::Display *display, Color color_on, Color color_off) {
  switch (type_) {
    case IMAGE_TYPE_BINARY: {
      for (int img_x = 0; img_x < width_; img_x++) {
        for (int img_y = 0; img_y < height_; img_y++) {
          if (this->get_binary_pixel_(img_x, img_y)) {
            display->draw_pixel_at(x + img_x, y + img_y, color_on);
          } else if (!this->transparent_) {
            display->draw_pixel_at(x + img_x, y + img_y, color_off);
          }
        }
      }
      break;
    }
    case IMAGE_TYPE_GRAYSCALE:
      for (int img_x = 0; img_x < width_; img_x++) {
        for (int img_y = 0; img_y < height_; img_y++) {
          auto color = this->get_grayscale_pixel_(img_x, img_y);
          if (color.w >= 0x80) {
            display->draw_pixel_at(x + img_x, y + img_y, color);
          }
        }
      }
      break;
    case IMAGE_TYPE_RGB565:
      for (int img_x = 0; img_x < width_; img_x++) {
        for (int img_y = 0; img_y < height_; img_y++) {
          auto color = this->get_rgb565_pixel_(img_x, img_y);
          if (color.w >= 0x80) {
            display->draw_pixel_at(x + img_x, y + img_y, color);
          }
        }
      }
      break;
    case IMAGE_TYPE_RGB24:
      for (int img_x = 0; img_x < width_; img_x++) {
        for (int img_y = 0; img_y < height_; img_y++) {
          auto color = this->get_rgb24_pixel_(img_x, img_y);
          if (color.w >= 0x80) {
            display->draw_pixel_at(x + img_x, y + img_y, color);
          }
        }
      }
      break;
    case IMAGE_TYPE_RGBA:
      for (int img_x = 0; img_x < width_; img_x++) {
        for (int img_y = 0; img_y < height_; img_y++) {
          auto color = this->get_rgba_pixel_(img_x, img_y);
          if (color.w >= 0x80) {
            display->draw_pixel_at(x + img_x, y + img_y, color);
          }
        }
      }
      break;
  }
}
Color Image::get_pixel(int x, int y, Color color_on, Color color_off) const {
  if (x < 0 || x >= this->width_ || y < 0 || y >= this->height_)
    return color_off;
  switch (this->type_) {
    case IMAGE_TYPE_BINARY:
      return this->get_binary_pixel_(x, y) ? color_on : color_off;
    case IMAGE_TYPE_GRAYSCALE:
      return this->get_grayscale_pixel_(x, y);
    case IMAGE_TYPE_RGB565:
      return this->get_rgb565_pixel_(x, y);
    case IMAGE_TYPE_RGB24:
      return this->get_rgb24_pixel_(x, y);
    case IMAGE_TYPE_RGBA:
      return this->get_rgba_pixel_(x, y);
    default:
      return color_off;
  }
}
#ifdef USE_LVGL
lv_img_dsc_t *Image::get_lv_img_dsc() {
  // lazily construct lvgl image_dsc.
  if (this->dsc_.data != this->data_start_) {
    this->dsc_.data = this->data_start_;
    this->dsc_.header.always_zero = 0;
    this->dsc_.header.reserved = 0;
    this->dsc_.header.w = this->width_;
    this->dsc_.header.h = this->height_;
    this->dsc_.data_size = this->get_width_stride() * this->get_height();
    switch (this->get_type()) {
      case IMAGE_TYPE_BINARY:
        this->dsc_.header.cf = LV_IMG_CF_ALPHA_1BIT;
        break;

      case IMAGE_TYPE_GRAYSCALE:
        this->dsc_.header.cf = LV_IMG_CF_ALPHA_8BIT;
        break;

      case IMAGE_TYPE_RGB24:
        this->dsc_.header.cf = LV_IMG_CF_RGB888;
        break;

      case IMAGE_TYPE_RGB565:
#if LV_COLOR_DEPTH == 16
        this->dsc_.header.cf = this->has_transparency() ? LV_IMG_CF_TRUE_COLOR_ALPHA : LV_IMG_CF_TRUE_COLOR;
#else
        this->dsc_.header.cf = LV_IMG_CF_RGB565;
#endif
        break;

      case IMAGE_TYPE_RGBA:
#if LV_COLOR_DEPTH == 32
        this->dsc_.header.cf = LV_IMG_CF_TRUE_COLOR;
#else
        this->dsc_.header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
#endif
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
Color Image::get_rgba_pixel_(int x, int y) const {
  const uint32_t pos = (x + y * this->width_) * 4;
  return Color(progmem_read_byte(this->data_start_ + pos + 0), progmem_read_byte(this->data_start_ + pos + 1),
               progmem_read_byte(this->data_start_ + pos + 2), progmem_read_byte(this->data_start_ + pos + 3));
}
Color Image::get_rgb24_pixel_(int x, int y) const {
  const uint32_t pos = (x + y * this->width_) * 3;
  Color color = Color(progmem_read_byte(this->data_start_ + pos + 0), progmem_read_byte(this->data_start_ + pos + 1),
                      progmem_read_byte(this->data_start_ + pos + 2));
  if (color.b == 1 && color.r == 0 && color.g == 0 && transparent_) {
    // (0, 0, 1) has been defined as transparent color for non-alpha images.
    // putting blue == 1 as a first condition for performance reasons (least likely value to short-cut the if)
    color.w = 0;
  } else {
    color.w = 0xFF;
  }
  return color;
}
Color Image::get_rgb565_pixel_(int x, int y) const {
  const uint8_t *pos = this->data_start_;
  if (this->transparent_) {
    pos += (x + y * this->width_) * 3;
  } else {
    pos += (x + y * this->width_) * 2;
  }
  uint16_t rgb565 = encode_uint16(progmem_read_byte(pos), progmem_read_byte(pos + 1));
  auto r = (rgb565 & 0xF800) >> 11;
  auto g = (rgb565 & 0x07E0) >> 5;
  auto b = rgb565 & 0x001F;
  auto a = this->transparent_ ? progmem_read_byte(pos + 2) : 0xFF;
  Color color = Color((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2), a);
  return color;
}

Color Image::get_grayscale_pixel_(int x, int y) const {
  const uint32_t pos = (x + y * this->width_);
  const uint8_t gray = progmem_read_byte(this->data_start_ + pos);
  uint8_t alpha = (gray == 1 && transparent_) ? 0 : 0xFF;
  return Color(gray, gray, gray, alpha);
}
int Image::get_width() const { return this->width_; }
int Image::get_height() const { return this->height_; }
ImageType Image::get_type() const { return this->type_; }
Image::Image(const uint8_t *data_start, int width, int height, ImageType type)
    : width_(width), height_(height), type_(type), data_start_(data_start) {}

}  // namespace image
}  // namespace esphome
