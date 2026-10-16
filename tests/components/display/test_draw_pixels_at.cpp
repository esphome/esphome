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
  rows.draw_pixels_at(10, 20, W, H, source.data(), bitness, big_endian, X_OFFSET, Y_OFFSET, X_PAD);
  columns.draw_pixels_at(10, 20, W, H, source.data(), bitness, big_endian, X_OFFSET, Y_OFFSET, X_PAD);
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
  display.draw_pixels_at(3, 4, 1, 1, source.data(), bitness, big_endian, 0, 0, 0);
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
  rows.draw_pixels_at(0, 0, W, H, source.data(), COLOR_BITNESS_332, true, X_OFFSET, Y_OFFSET, X_PAD);
  columns.draw_pixels_at(0, 0, W, H, source.data(), COLOR_BITNESS_332, true, X_OFFSET, Y_OFFSET, X_PAD);
  ASSERT_GT(rows.count(), 1u);
  ASSERT_GT(columns.count(), 1u);
  EXPECT_EQ(std::get<0>(rows.pixels()[1]), 1);     // the row walk steps x first
  EXPECT_EQ(std::get<1>(columns.pixels()[1]), 1);  // the column walk steps y first
}

// Remove before 2027.5.0, with the draw_pixels_at() overloads that take a color order.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

// An out of tree driver that still overrides only the form with a color order.
class LegacyDisplay : public RecordingDisplay {
 public:
  void draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, ColorOrder order,
                      ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) override {
    this->calls_.push_back({x_start, y_start, w, h, x_offset, y_offset, x_pad});
    this->order_ = order;
    this->ptr_ = ptr;
    this->bitness_ = bitness;
    this->big_endian_ = big_endian;
  }

  std::vector<std::vector<int>> calls_;
  ColorOrder order_{COLOR_ORDER_GRB};
  const uint8_t *ptr_{};
  ColorBitness bitness_{COLOR_BITNESS_888};
  bool big_endian_{};
};

// Callers use the form without a color order; a legacy override still receives every block, as RGB.
TEST(DrawPixelsAt, LegacyOverrideStillReceivesBlocks) {
  const auto source = make_source(2);
  LegacyDisplay legacy;
  Display &display = legacy;
  display.draw_pixels_at(10, 20, W, H, source.data(), COLOR_BITNESS_565, true, X_OFFSET, Y_OFFSET, X_PAD);
  display.draw_pixels_at(1, 2, 3, 4, source.data(), COLOR_BITNESS_565, true);
  ASSERT_EQ(legacy.calls_.size(), 2u);
  EXPECT_EQ(legacy.calls_[0], (std::vector<int>{10, 20, W, H, X_OFFSET, Y_OFFSET, X_PAD}));
  EXPECT_EQ(legacy.calls_[1], (std::vector<int>{1, 2, 3, 4, 0, 0, 0}));
  EXPECT_EQ(legacy.order_, COLOR_ORDER_RGB);
  EXPECT_EQ(legacy.ptr_, source.data());
  EXPECT_EQ(legacy.bitness_, COLOR_BITNESS_565);
  EXPECT_TRUE(legacy.big_endian_);
  EXPECT_EQ(legacy.count(), 0u);
}

// The deprecated form still honours a color order other than RGB when it draws per pixel.
static void expect_ordered(ColorOrder order, Color expected) {
  const std::vector<uint8_t> source{0x12, 0x34, 0x56};  // b, g, r in memory: 0x563412
  RecordingDisplay display;
  display.draw_pixels_at(3, 4, 1, 1, source.data(), order, COLOR_BITNESS_888, false);
  ASSERT_EQ(display.count(), 1u);
  EXPECT_EQ(display.pixels()[0], std::make_tuple(3, 4, expected.raw_32));
  EXPECT_EQ(expected.raw_32, ColorUtil::to_color(0x563412, order, COLOR_BITNESS_888).raw_32);
}

TEST(DrawPixelsAt, DeprecatedOrderRgb) { expect_ordered(COLOR_ORDER_RGB, Color(0x56, 0x34, 0x12)); }
TEST(DrawPixelsAt, DeprecatedOrderBgr) { expect_ordered(COLOR_ORDER_BGR, Color(0x12, 0x34, 0x56)); }
TEST(DrawPixelsAt, DeprecatedOrderGrb) { expect_ordered(COLOR_ORDER_GRB, Color(0x34, 0x56, 0x12)); }

// The deprecated packing functions still honour the order; with RGB they match the new ones.
TEST(DrawPixelsAt, DeprecatedPackingMatchesForRgb) {
  const Color color(0xC0, 0x60, 0x30);
  EXPECT_EQ(ColorUtil::color_to_565(color, COLOR_ORDER_RGB), ColorUtil::color_to_565(color));
  EXPECT_EQ(ColorUtil::color_to_332(color, COLOR_ORDER_RGB), ColorUtil::color_to_332(color));
  EXPECT_EQ(ColorUtil::color_to_565(color, COLOR_ORDER_BGR), ColorUtil::color_to_565(Color(0x30, 0x60, 0xC0)));
}

#pragma GCC diagnostic pop

}  // namespace esphome::display::testing
