#pragma once
#include "esphome/components/display/display_buffer.h"

namespace esphome {
namespace online_image {

enum ImageFormat {
  JPEG,
  PNG,
};

class OnlineImage {
 public:
  OnlineImage(const char* url, uint16_t width, uint16_t height): url(url), width(width), height(height) {};

  void draw(int x, int y, display::DisplayBuffer* display);

 protected:
  const char* url;
  const uint16_t width;
  const uint16_t height;

};

} // namespace online_image
} // namespace esphome