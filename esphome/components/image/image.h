#pragma once
#include "esphome/core/color.h"
#include "esphome/components/display/display.h"

#ifdef USE_LVGL
#include "esphome/components/lvgl/lvgl_proxy.h"
#endif  // USE_LVGL

namespace esphome {
namespace image {

enum ImageType {
  IMAGE_TYPE_BINARY = 0,
  IMAGE_TYPE_GRAYSCALE = 1,
  IMAGE_TYPE_RGB24 = 2,
  IMAGE_TYPE_RGB565 = 3,
  IMAGE_TYPE_RGBA = 4,
};

class Image : public display::BaseImage {
 public:
  Image(const uint8_t *data_start, int width, int height, ImageType type);
  Color get_pixel(int x, int y, Color color_on = display::COLOR_ON, Color color_off = display::COLOR_OFF) const;
  int get_width() const override;
  int get_height() const override;
  const uint8_t *get_data_start() const { return this->data_start_; }
  ImageType get_type() const;

  int get_bpp() const {
    switch (this->type_) {
      case IMAGE_TYPE_BINARY:
        return 1;
      case IMAGE_TYPE_GRAYSCALE:
        return 8;
      case IMAGE_TYPE_RGB565:
        return this->transparent_ ? 24 : 16;
      case IMAGE_TYPE_RGB24:
        return 24;
      case IMAGE_TYPE_RGBA:
        return 32;
    }
    return 0;
  }

  /// Return the stride of the image in bytes, that is, the distance in bytes
  /// between two consecutive rows of pixels.
  uint32_t get_width_stride() const { return (this->width_ * this->get_bpp() + 7u) / 8u; }
  void draw(int x, int y, display::Display *display, Color color_on, Color color_off) override;

  void set_transparency(bool transparent) { transparent_ = transparent; }
  bool has_transparency() const { return transparent_; }

#ifdef USE_LVGL
  lv_img_dsc_t *get_lv_img_dsc();
#endif
 protected:
  bool get_binary_pixel_(int x, int y) const;
  Color get_rgb24_pixel_(int x, int y) const;
  Color get_rgba_pixel_(int x, int y) const;
  Color get_rgb565_pixel_(int x, int y) const;
  Color get_grayscale_pixel_(int x, int y) const;

  int width_;
  int height_;
  ImageType type_;
  const uint8_t *data_start_;
  bool transparent_;
#ifdef USE_LVGL
  lv_img_dsc_t dsc_{};
#endif
};

}  // namespace image
}  // namespace esphome
