#pragma once
#include "esphome/core/color.h"

namespace esphome {
namespace display {

enum ImageType {
  IMAGE_TYPE_BINARY = 0,
  IMAGE_TYPE_GRAYSCALE = 1,
  IMAGE_TYPE_RGB24 = 2,
  IMAGE_TYPE_RGB565 = 3,
  IMAGE_TYPE_RGBA = 4,
};

inline int image_type_to_bpp(ImageType type) {
  switch (type) {
    case IMAGE_TYPE_BINARY:
      return 1;
    case IMAGE_TYPE_GRAYSCALE:
      return 8;
    case IMAGE_TYPE_RGB565:
      return 16;
    case IMAGE_TYPE_RGB24:
      return 24;
    case IMAGE_TYPE_RGBA:
      return 32;
  }
  return 0;
}

inline int image_type_to_width_stride(int width, ImageType type) { return (width * image_type_to_bpp(type) + 7u) / 8u; }

/// Turn the pixel OFF.
extern const Color COLOR_OFF;
/// Turn the pixel ON.
extern const Color COLOR_ON;

class DisplayBuffer;

class BaseImage {
 public:
  virtual void draw(int x, int y, DisplayBuffer *display, Color color_on, Color color_off) = 0;
  virtual int get_width() const = 0;
  virtual int get_height() const = 0;
};

class Image : public BaseImage {
 public:
  Image(const uint8_t *data_start, int width, int height, ImageType type);
  Color get_pixel(int x, int y, Color color_on = COLOR_ON, Color color_off = COLOR_OFF) const;
  int get_width() const override;
  int get_height() const override;
  ImageType get_type() const;

  void draw(int x, int y, DisplayBuffer *display, Color color_on, Color color_off) override;

  void set_transparency(bool transparent) { transparent_ = transparent; }
  bool has_transparency() const { return transparent_; }

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
};

}  // namespace display
}  // namespace esphome
