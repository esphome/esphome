#pragma once
#include "esphome/components/display/display_buffer.h"

namespace esphome {
namespace online_image {

enum ImageFormat {
  JPEG,
  PNG,
};

class OnlineImage: public esphome::display::Image {
 public:
  OnlineImage(const char* url, display::ImageType type): Image(nullptr, 0, 0, type), url(url) {};
  bool get_pixel(int x, int y) const override;
  Color get_color_pixel(int x, int y) const override;
  Color get_grayscale_pixel(int x, int y) const override;

  void draw(int x, int y, display::DisplayBuffer* display);

 protected:
  const char* url;

};

} // namespace online_image
} // namespace esphome