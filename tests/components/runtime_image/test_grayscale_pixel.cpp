#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>

#include "esphome/components/runtime_image/runtime_image.h"

namespace esphome::runtime_image::testing {

class GrayscaleImage : public RuntimeImage {
 public:
  GrayscaleImage() : RuntimeImage(BMP, image::IMAGE_TYPE_GRAYSCALE, image::TRANSPARENCY_OPAQUE) {}
  uint8_t pixel(int x) const { return this->buffer_[x]; }
};

// The integer weights stay within one step of the floating point formula they replace, and keep white at 255
TEST(GrayscalePixel, MatchesTheFloatingPointWeights) {
  GrayscaleImage img;
  ASSERT_GT(img.resize(256, 1), 0);
  for (int r = 0; r < 256; r += 5) {
    for (int g = 0; g < 256; g += 5) {
      for (int b = 0; b < 256; b += 5) {
        img.draw_pixel(0, 0, Color(r, g, b));
        const int expected = static_cast<int>(0.2125 * r + 0.7154 * g + 0.0721 * b);
        ASSERT_LE(std::abs(img.pixel(0) - expected), 1) << "r=" << r << " g=" << g << " b=" << b;
      }
    }
  }
  img.draw_pixel(1, 0, Color(255, 255, 255));
  EXPECT_EQ(img.pixel(1), 255);
  img.draw_pixel(2, 0, Color(0, 0, 0));
  EXPECT_EQ(img.pixel(2), 0);
}

}  // namespace esphome::runtime_image::testing
