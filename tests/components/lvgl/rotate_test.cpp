#include <gtest/gtest.h>
#include <cstdint>
#include <vector>
#include "esphome/components/lvgl/lvgl_rotate.h"

namespace esphome::lvgl::testing {

namespace {

constexpr uint16_t PADDING = 0xAAAA;

struct RotateCase {
  lv_coord_t width;
  lv_coord_t height;
  lv_coord_t rounding;
};

std::vector<uint16_t> make_source(lv_coord_t width, lv_coord_t height) {
  std::vector<uint16_t> src(width * height);
  for (size_t i = 0; i != src.size(); i++)
    src[i] = static_cast<uint16_t>(i * 7919 + 1);
  return src;
}

lv_coord_t round_up(lv_coord_t value, lv_coord_t rounding) { return (value + rounding - 1) / rounding * rounding; }

// The straightforward pixel by pixel rotation that the tiled versions replace.
void reference_rotate_90(const uint16_t *src, uint16_t *dst, lv_coord_t width, lv_coord_t height, lv_coord_t stride) {
  for (lv_coord_t x = height; x-- != 0;) {
    for (lv_coord_t y = 0; y != width; y++)
      dst[y * stride + x] = *src++;
  }
}

void reference_rotate_270(const uint16_t *src, uint16_t *dst, lv_coord_t width, lv_coord_t height, lv_coord_t stride) {
  for (lv_coord_t x = 0; x != height; x++) {
    for (lv_coord_t y = width; y-- != 0;)
      dst[y * stride + x] = *src++;
  }
}

// Sizes below, at and above the tile size, partial tiles in both directions, and rounded strides.
const RotateCase CASES[] = {
    {1, 1, 1},   {1, 50, 1},  {50, 1, 1},  {31, 33, 1},  {32, 32, 1},  {64, 64, 1},  {65, 97, 1},    {800, 3, 1},
    {3, 480, 1}, {33, 31, 2}, {77, 45, 4}, {100, 61, 8}, {480, 40, 1}, {40, 480, 1}, {241, 123, 16},
};

}  // namespace

TEST(LvglRotateTest, Rotate90MatchesReference) {
  for (const auto &c : CASES) {
    const lv_coord_t stride = round_up(c.height, c.rounding);
    auto src = make_source(c.width, c.height);
    std::vector<uint16_t> expected(c.width * stride, PADDING);
    std::vector<uint16_t> actual(c.width * stride, PADDING);
    reference_rotate_90(src.data(), expected.data(), c.width, c.height, stride);
    rotate_90_tiled(src.data(), actual.data(), c.width, c.height, stride);
    EXPECT_EQ(expected, actual) << "width " << c.width << " height " << c.height << " stride " << stride;
  }
}

TEST(LvglRotateTest, Rotate270MatchesReference) {
  for (const auto &c : CASES) {
    const lv_coord_t stride = round_up(c.height, c.rounding);
    auto src = make_source(c.width, c.height);
    std::vector<uint16_t> expected(c.width * stride, PADDING);
    std::vector<uint16_t> actual(c.width * stride, PADDING);
    reference_rotate_270(src.data(), expected.data(), c.width, c.height, stride);
    rotate_270_tiled(src.data(), actual.data(), c.width, c.height, stride);
    EXPECT_EQ(expected, actual) << "width " << c.width << " height " << c.height << " stride " << stride;
  }
}

TEST(LvglRotateTest, Rotate90SmallBlock) {
  // 3 x 2 source:  1 2 3
  //                4 5 6
  // rotated by 90 degrees clockwise gives 2 x 3:  4 1 / 5 2 / 6 3
  const uint16_t src[] = {1, 2, 3, 4, 5, 6};
  uint16_t dst[6] = {};
  rotate_90_tiled(src, dst, 3, 2, 2);
  const uint16_t expected[] = {4, 1, 5, 2, 6, 3};
  for (int i = 0; i != 6; i++)
    EXPECT_EQ(expected[i], dst[i]) << "index " << i;
}

TEST(LvglRotateTest, Rotate270SmallBlock) {
  // Same source rotated by 270 degrees clockwise gives 2 x 3:  3 6 / 2 5 / 1 4
  const uint16_t src[] = {1, 2, 3, 4, 5, 6};
  uint16_t dst[6] = {};
  rotate_270_tiled(src, dst, 3, 2, 2);
  const uint16_t expected[] = {3, 6, 2, 5, 1, 4};
  for (int i = 0; i != 6; i++)
    EXPECT_EQ(expected[i], dst[i]) << "index " << i;
}

}  // namespace esphome::lvgl::testing
