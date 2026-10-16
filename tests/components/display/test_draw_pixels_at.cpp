#include <gtest/gtest.h>

#include <algorithm>
#include <initializer_list>
#include <tuple>
#include <vector>

#include "esphome/components/display/display.h"

namespace esphome::display::testing {

// Records every pixel write; pixel_axes_swapped() picks the walk order.
class RecordingDisplay : public Display {
 public:
  void set_axes_swapped(bool swapped) { this->axes_swapped_ = swapped; }
  bool pixel_axes_swapped() const override { return this->axes_swapped_; }
  void draw_pixel_at(int x, int y, Color color) override { this->pixels_.emplace_back(x, y, color.raw_32); }
  void update() override {}
  DisplayType get_display_type() override { return DisplayType::DISPLAY_TYPE_COLOR; }
  int get_width_internal() override { return 64; }
  int get_height_internal() override { return 64; }

  std::vector<std::tuple<int, int, uint32_t>> sorted_pixels() const {
    auto sorted = this->pixels_;
    std::sort(sorted.begin(), sorted.end());
    return sorted;
  }
  size_t count() const { return this->pixels_.size(); }
  const std::vector<std::tuple<int, int, uint32_t>> &pixels() const { return this->pixels_; }

 protected:
  bool axes_swapped_{false};
  std::vector<std::tuple<int, int, uint32_t>> pixels_;
};

// A 7x5 region with a 2 pixel left offset, a 3 line top offset and 4 pixels of
// right padding, so the source index arithmetic of both walks is exercised.
static constexpr int W = 7, H = 5, X_OFFSET = 2, Y_OFFSET = 3, X_PAD = 4;
static constexpr int LINE_STRIDE = X_OFFSET + W + X_PAD;
static constexpr int LINES = Y_OFFSET + H;

static std::vector<uint8_t> make_source(int bytes_per_pixel) {
  std::vector<uint8_t> source(LINE_STRIDE * LINES * bytes_per_pixel);
  uint8_t value = 1;
  for (auto &byte : source)
    byte = value++;
  return source;
}

static void expect_walks_match(ColorBitness bitness, int bytes_per_pixel, bool big_endian) {
  const auto source = make_source(bytes_per_pixel);
  RecordingDisplay rows;
  RecordingDisplay columns;
  columns.set_axes_swapped(true);
  rows.draw_pixels_at(10, 20, W, H, source.data(), COLOR_ORDER_RGB, bitness, big_endian, X_OFFSET, Y_OFFSET, X_PAD);
  columns.draw_pixels_at(10, 20, W, H, source.data(), COLOR_ORDER_RGB, bitness, big_endian, X_OFFSET, Y_OFFSET, X_PAD);
  EXPECT_EQ(rows.count(), static_cast<size_t>(W * H));
  EXPECT_EQ(columns.count(), static_cast<size_t>(W * H));
  EXPECT_EQ(rows.sorted_pixels(), columns.sorted_pixels());
}

TEST(DrawPixelsAt, ColumnWalkMatchesRowWalk332) { expect_walks_match(COLOR_BITNESS_332, 1, true); }
TEST(DrawPixelsAt, ColumnWalkMatchesRowWalk565BigEndian) { expect_walks_match(COLOR_BITNESS_565, 2, true); }
TEST(DrawPixelsAt, ColumnWalkMatchesRowWalk565LittleEndian) { expect_walks_match(COLOR_BITNESS_565, 2, false); }
TEST(DrawPixelsAt, ColumnWalkMatchesRowWalk888) { expect_walks_match(COLOR_BITNESS_888, 3, false); }

// One known pixel per bitness and byte order, so a decode that swapped bytes or channels
// could not pass the walk comparison above by being wrong the same way twice.
static void expect_golden(ColorBitness bitness, std::initializer_list<uint8_t> bytes, bool big_endian, Color expected) {
  const std::vector<uint8_t> source(bytes);
  RecordingDisplay display;
  display.draw_pixels_at(3, 4, 1, 1, source.data(), COLOR_ORDER_RGB, bitness, big_endian, 0, 0, 0);
  ASSERT_EQ(display.count(), 1u);
  EXPECT_EQ(display.pixels()[0], std::make_tuple(3, 4, expected.raw_32));
}

TEST(DrawPixelsAt, Golden565BigEndian) {
  expect_golden(COLOR_BITNESS_565, {0xE0, 0x07}, true, Color(230, 0, 57));  // 0xE007: r 28, g 0, b 7
}
TEST(DrawPixelsAt, Golden565LittleEndian) {
  expect_golden(COLOR_BITNESS_565, {0xE0, 0x07}, false, Color(0, 255, 0));  // 0x07E0: pure green
}
// 888 is always b,g,r in memory for RGB order, whatever the byte order flag says
TEST(DrawPixelsAt, Golden888IgnoresBigEndian) {
  expect_golden(COLOR_BITNESS_888, {0x10, 0x20, 0x30}, true, Color(0x30, 0x20, 0x10));
}
TEST(DrawPixelsAt, Golden888LittleEndian) {
  expect_golden(COLOR_BITNESS_888, {0x10, 0x20, 0x30}, false, Color(0x30, 0x20, 0x10));
}
TEST(DrawPixelsAt, Golden332) {
  expect_golden(COLOR_BITNESS_332, {0xE3}, true, Color(255, 0, 255));  // r 7 of 7, g 0, b 3 of 3
}

// The column walk really is a different order, otherwise the tests above prove nothing.
TEST(DrawPixelsAt, ColumnWalkChangesOrder) {
  const auto source = make_source(1);
  RecordingDisplay rows;
  RecordingDisplay columns;
  columns.set_axes_swapped(true);
  rows.draw_pixels_at(0, 0, W, H, source.data(), COLOR_ORDER_RGB, COLOR_BITNESS_332, true, X_OFFSET, Y_OFFSET, X_PAD);
  columns.draw_pixels_at(0, 0, W, H, source.data(), COLOR_ORDER_RGB, COLOR_BITNESS_332, true, X_OFFSET, Y_OFFSET,
                         X_PAD);
  ASSERT_GT(rows.count(), 1u);
  ASSERT_GT(columns.count(), 1u);
  EXPECT_EQ(std::get<0>(rows.pixels()[1]), 1);     // the row walk steps x first
  EXPECT_EQ(std::get<1>(columns.pixels()[1]), 1);  // the column walk steps y first
}

}  // namespace esphome::display::testing
