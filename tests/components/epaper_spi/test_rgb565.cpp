#include <array>
#include <cstring>
#include <tuple>

#include <gtest/gtest.h>

#include "esphome/components/epaper_spi/epaper_waveshare_3p97_g.h"

namespace esphome::epaper_spi::testing {

class FrameAccess : public EPaperBase {
 public:
  static constexpr auto BUFFER = &FrameAccess::buffer_;
  static constexpr auto BITS_PER_PIXEL = &FrameAccess::bits_per_pixel_;
  static constexpr auto ROW_WIDTH = &FrameAccess::row_width_;
  static constexpr auto BUFFER_LENGTH = &FrameAccess::buffer_length_;
  static constexpr auto IS_MONO = &FrameAccess::buffer_is_mono_;
};

TEST(Waveshare3P97Rgb565, FrameFormat) {
  EPaperWaveshare3P97InG panel("test", 800, 480, nullptr, 0);
  EXPECT_EQ(panel.*FrameAccess::BITS_PER_PIXEL, 2);
  EXPECT_EQ(panel.*FrameAccess::ROW_WIDTH, 200);
  EXPECT_EQ(panel.*FrameAccess::BUFFER_LENGTH, 96000);
  EXPECT_FALSE((panel.*FrameAccess::IS_MONO)());
}

class Waveshare3P97Rgb565Drawing : public ::testing::TestWithParam<std::tuple<DisplayRotation, bool, bool>> {};

TEST_P(Waveshare3P97Rgb565Drawing, MatchesScalarPixels) {
  const auto [rotation, big_endian, clipped] = this->GetParam();
  EPaperWaveshare3P97InG panel("block", 800, 480, nullptr, 0);
  EPaperWaveshare3P97InG reference("scalar", 800, 480, nullptr, 0);
  auto &buffer = panel.*FrameAccess::BUFFER;
  auto &reference_buffer = reference.*FrameAccess::BUFFER;
  ASSERT_TRUE(buffer.init(96000));
  ASSERT_TRUE(reference_buffer.init(96000));
  size_t contiguous_length;
  const auto *bytes = buffer.get_span(0, contiguous_length);
  ASSERT_EQ(contiguous_length, 96000);
  size_t reference_length;
  const auto *reference_bytes = reference_buffer.get_span(0, reference_length);
  ASSERT_EQ(reference_length, 96000);
  panel.set_rotation(rotation);
  reference.set_rotation(rotation);

  constexpr int width = 13;
  constexpr int height = 5;
  constexpr int x_offset = 2;
  constexpr int y_offset = 1;
  constexpr int x_pad = 3;
  constexpr int stride = x_offset + width + x_pad;
  const std::array<Color, 4> colors{Color::BLACK, Color::WHITE, Color(255, 255, 0), Color(255, 0, 0)};
  const std::array<uint16_t, 4> rgb565{0x0000, 0xFFFF, 0xFFE0, 0xF800};
  std::array<uint8_t, stride *(y_offset + height) * 2> source{};
  for (int row = 0; row < height; row++) {
    for (int column = 0; column < width; column++) {
      const auto value = rgb565[(row + column) % rgb565.size()];
      const size_t index = ((y_offset + row) * stride + x_offset + column) * 2;
      source[index] = big_endian ? value >> 8 : value & 0xFF;
      source[index + 1] = big_endian ? value & 0xFF : value >> 8;
    }
  }

  const std::array<std::array<int, 2>, 3> origins{{{3, 2}, {795, 477}, {-3, -2}}};
  for (const auto &origin : origins) {
    SCOPED_TRACE(::testing::Message() << "origin=" << origin[0] << "," << origin[1]);
    panel.fill(Color::WHITE);
    reference.fill(Color::WHITE);
    if (clipped) {
      panel.start_clipping(5, 3, 799, 479);
      reference.start_clipping(5, 3, 799, 479);
    }
    panel.draw_pixels_at(origin[0], origin[1], width, height, source.data(), COLOR_BITNESS_565, big_endian, x_offset,
                         y_offset, x_pad);
    auto &scalar = static_cast<display::Display &>(reference);
    for (int row = 0; row < height; row++) {
      for (int column = 0; column < width; column++) {
        scalar.draw_pixel_at(origin[0] + column, origin[1] + row, colors[(row + column) % colors.size()]);
      }
    }
    EXPECT_EQ(std::memcmp(bytes, reference_bytes, 96000), 0);
    if (clipped) {
      panel.end_clipping();
      reference.end_clipping();
    }
  }
}

INSTANTIATE_TEST_SUITE_P(FormatVariants, Waveshare3P97Rgb565Drawing,
                         ::testing::Combine(::testing::Values(DISPLAY_ROTATION_0_DEGREES, DISPLAY_ROTATION_180_DEGREES),
                                            ::testing::Bool(), ::testing::Bool()));

}  // namespace esphome::epaper_spi::testing