#include "image.h"

#include "esphome/core/hal.h"

#include "esphome/core/log.h"

namespace esphome {
namespace image {

static const char *const TAG = "image";

void Image::draw(int x, int y, display::Display *display, Color color_on, Color color_off) {
  switch (type_) {
    case IMAGE_TYPE_BINARY: {
      bool c_on = color_on.is_on();
      bool c_off = color_off.is_on();
      if (this->is_vertical()) {
        if (display->get_pixel_group_mode() == display::PIXEL_GROUP_VERTICAL) {
          const uint32_t height_8 = ((this->height_ + 7u) / 8u);
          uint8_t offset = (uint32_t) y % 8;
          uint8_t reverse_offset = 8 - offset;
          int y_offset = y < 0 ? -1 : 0;
          for (int img_x = 0; img_x < this->width_; img_x++) {
            for (int img_y = 0; img_y < height_8; img_y++) {
              uint8_t data1 = progmem_read_byte(this->data_start_ + img_x * height_8 + img_y);
              uint8_t data2 = 0xff;
              if (this->transparent_) {
                data2 = data1;
                data1 = (c_on) ? 0xff : 0x00;
              } else {
                if (c_off) {
                  if (c_on) {
                    data1 = 0xff;
                  } else {
                    data1 = ~data1;
                  }
                } else if (!c_on) {
                  data1 = 0x00;
                }
              }
              if (img_y == height_8 - 1 && (this->height_ % 8) != 0) {
                data2 &= 0xff << (8 - (this->height_ % 8));
              }
              if (y % 8 == 0) {
                display->draw_vertical_pixel_group(x + img_x, y / 8 + img_y, data1, data2, true);
              } else {
                display->draw_vertical_pixel_group(x + img_x, y / 8 + img_y + y_offset, data1 >> offset,
                                                   data2 >> offset, true);
                display->draw_vertical_pixel_group(x + img_x, y / 8 + img_y + y_offset + 1, data1 << reverse_offset,
                                                   data2 << reverse_offset, true);
              }
            }
          }
          break;
        } else if (display->get_pixel_group_mode() == display::PIXEL_GROUP_HORIZONTAL) {
          ESP_LOGI(TAG, "To speed up rendering set the image type to BINARY");
        }
      } else {
        if (display->get_pixel_group_mode() == display::PIXEL_GROUP_HORIZONTAL) {
          const uint32_t width_8 = ((this->width_ + 7u) / 8u);
          uint8_t offset = (uint32_t) x % 8;
          uint8_t reverse_offset = 8 - offset;
          int x_offset = x < 0 ? -1 : 0;
          for (int img_x = 0; img_x < width_8; img_x++) {
            for (int img_y = 0; img_y < this->height_; img_y++) {
              uint8_t data1 = progmem_read_byte(this->data_start_ + img_x + img_y * width_8);
              uint8_t data2 = 0xff;
              if (this->transparent_) {
                data2 = data1;
                data1 = (c_on) ? 0xff : 0x00;
              } else {
                if (c_off) {
                  if (c_on) {
                    data1 = 0xff;
                  } else {
                    data1 = ~data1;
                  }
                } else if (!c_on) {
                  data1 = 0x00;
                }
              }
              if (img_x == width_8 - 1 && (this->width_ % 8) != 0) {
                data2 &= 0xff << (8 - (this->width_ % 8));
              }
              if (y % 8 == 0) {
                display->draw_horizontal_pixel_group(x / 8 + img_x, y + img_y, data1, data2, true);
              } else {
                display->draw_horizontal_pixel_group(x / 8 + img_x + x_offset, y + img_y, data1 >> offset,
                                                     data2 >> offset, true);
                display->draw_horizontal_pixel_group(x / 8 + img_x + x_offset + 1, y + img_y, data1 << reverse_offset,
                                                     data2 << reverse_offset, true);
              }
            }
          }
          break;
        } else if (display->get_pixel_group_mode() == display::PIXEL_GROUP_VERTICAL) {
          ESP_LOGI(TAG, "To speed up rendering set the image type to BINARY_VERTICAL");
        }
      }
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
bool Image::get_binary_pixel_(int x, int y) const {
  if (this->is_vertical()) {
    const uint32_t height_8 = ((this->height_ + 7u) / 8u) * 8u;
    const uint32_t pos = x * height_8 + y;
    return progmem_read_byte(this->data_start_ + (pos / 8u)) & (0x80 >> (pos % 8u));
  } else {
    const uint32_t width_8 = ((this->width_ + 7u) / 8u) * 8u;
    const uint32_t pos = x + y * width_8;
    return progmem_read_byte(this->data_start_ + (pos / 8u)) & (0x80 >> (pos % 8u));
  }
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
  const uint32_t pos = (x + y * this->width_) * 2;
  uint16_t rgb565 =
      progmem_read_byte(this->data_start_ + pos + 0) << 8 | progmem_read_byte(this->data_start_ + pos + 1);
  auto r = (rgb565 & 0xF800) >> 11;
  auto g = (rgb565 & 0x07E0) >> 5;
  auto b = rgb565 & 0x001F;
  Color color = Color((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2));
  if (rgb565 == 0x0020 && transparent_) {
    // darkest green has been defined as transparent color for transparent RGB565 images.
    color.w = 0;
  } else {
    color.w = 0xFF;
  }
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
