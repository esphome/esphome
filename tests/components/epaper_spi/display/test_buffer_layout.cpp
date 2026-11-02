#include <gtest/gtest.h>

#include "esphome/components/epaper_spi/epaper_spi_ssd1677_gray4.h"
#include "esphome/components/epaper_spi/epaper_spi_ssd1683.h"
#include "esphome/components/epaper_spi/epaper_spi_t133a01.h"
#include "esphome/components/epaper_spi/epaper_waveshare_bwr.h"
#include "esphome/components/epaper_spi/epaper_weact_3c.h"

namespace esphome::epaper_spi::testing {
namespace {

template<typename Driver> class Layout : public Driver {
 public:
  Layout(uint16_t width, uint16_t height) : Driver("test", width, height, nullptr, 0) {}
  uint16_t row_width() const { return this->row_width_; }
  size_t buffer_length() const { return this->buffer_length_; }
  bool is_mono() const { return this->buffer_is_mono_(); }
};

/// Each driver's bits per pixel and plane count give the row width and buffer size its drawing code expects, and only
/// a single 1 bit per pixel plane takes the direct drawing paths.
TEST(EPaperBufferLayout, SizesFollowBitsPerPixelAndPlanes) {
  Layout<EPaperSSD1683> mono(21, 3);
  EXPECT_EQ(mono.row_width(), 3);
  EXPECT_EQ(mono.buffer_length(), 9u);
  EXPECT_TRUE(mono.is_mono());

  Layout<EPaperSSD1677Gray4> gray(21, 3);
  EXPECT_EQ(gray.row_width(), 6);
  EXPECT_EQ(gray.buffer_length(), 18u);
  EXPECT_FALSE(gray.is_mono());

  Layout<EPaperT133A01> t133(20, 3);
  EXPECT_EQ(t133.row_width(), 10);
  EXPECT_EQ(t133.buffer_length(), 30u);
  EXPECT_FALSE(t133.is_mono());

  Layout<EPaperWeAct3C> weact(21, 3);
  EXPECT_EQ(weact.row_width(), 3);
  EXPECT_EQ(weact.buffer_length(), 18u);
  EXPECT_FALSE(weact.is_mono());

  Layout<EPaperWaveshareBWR> bwr(21, 3);
  EXPECT_EQ(bwr.buffer_length(), 18u);
  EXPECT_FALSE(bwr.is_mono());
}

}  // namespace
}  // namespace esphome::epaper_spi::testing
