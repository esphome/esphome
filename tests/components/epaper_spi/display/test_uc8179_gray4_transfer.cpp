#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "../common.h"
#include "esphome/components/epaper_spi/epaper_spi_uc8179_gray4.h"

namespace esphome::epaper_spi::testing {

class TestableUC8179Gray4 : public EPaperUC8179Gray4 {
 public:
  TestableUC8179Gray4(uint16_t width, uint16_t height) : EPaperUC8179Gray4("test", width, height, nullptr, 0) {}

  void install(spi::SPIDelegate *delegate) {
    this->delegate_ = delegate;
    this->set_dc_pin(&this->dc);
    ASSERT_TRUE(this->init_buffer_(this->buffer_length_));
  }

  /// Both planes of one push; returns how many calls it took.
  int run_push() {
    int calls = 1;
    while (!this->transfer_data())
      calls++;
    return calls;
  }

  using EPaperUC8179Gray4::transfer_data;

  RecordingPin dc;
};

using Bytes = std::vector<uint8_t>;

namespace {

/// A gray that lands squarely on each of the four levels.
Color color_for_level(uint8_t level) {
  static const uint8_t GRAYS[4] = {0, 85, 170, 255};
  const uint8_t v = GRAYS[level];
  return Color(v, v, v);
}

void draw_row(TestableUC8179Gray4 &display, int y, const std::vector<uint8_t> &levels) {
  for (size_t x = 0; x != levels.size(); x++)
    display.draw_pixel_at((int) x, y, color_for_level(levels[x]));
}

}  // namespace

/// Each pixel's 2-bit level is split across the RAM planes: the low bit to DTM1 (0x10), the high
/// bit to DTM2 (0x13), both inverted because the controller reads 0 as white.
TEST(EPaperUC8179Gray4, SplitsEachLevelAcrossBothPlanes) {
  TestableUC8179Gray4 display(8, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  draw_row(display, 0, {0, 1, 2, 3, 0, 1, 2, 3});
  display.run_push();

  //         levels  0 1 2 3 0 1 2 3
  // low  bit        0 1 0 1 0 1 0 1 = 0x55, inverted 0xAA
  // high bit        0 0 1 1 0 0 1 1 = 0x33, inverted 0xCC
  EXPECT_EQ(bus.data[0x10], (Bytes{0xAA}));
  EXPECT_EQ(bus.data[0x13], (Bytes{0xCC}));
}

/// Two buffer bytes (4 pixels each) make one plane byte (8 pixels), leftmost pixel in the most
/// significant bit.
TEST(EPaperUC8179Gray4, PacksPixelsLeftmostFirstAcrossSourceBytes) {
  TestableUC8179Gray4 display(16, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);
  display.fill(Color::BLACK);

  draw_row(display, 0, {3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2});
  display.run_push();

  // low  bits: pixel 0 (3) only             -> 0x80 0x00, inverted 0x7F 0xFF
  // high bits: pixel 0 (3) and pixel 15 (2) -> 0x80 0x01, inverted 0x7F 0xFE
  EXPECT_EQ(bus.data[0x10], (Bytes{0x7F, 0xFF}));
  EXPECT_EQ(bus.data[0x13], (Bytes{0x7F, 0xFE}));
}

/// The four-level waveform is selected before the data, and the low-bit plane goes out first.
TEST(EPaperUC8179Gray4, SelectsTheWaveformThenWritesBothPlanesOnce) {
  TestableUC8179Gray4 display(8, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  display.run_push();

  EXPECT_EQ(bus.data[0xE0], (Bytes{0x02}));
  EXPECT_EQ(bus.data[0xE5], (Bytes{0x5F}));
  const auto &cmds = bus.commands;
  ASSERT_EQ(std::count(cmds.begin(), cmds.end(), 0x10), 1);
  ASSERT_EQ(std::count(cmds.begin(), cmds.end(), 0x13), 1);
  const auto pos = [&](uint8_t cmd) { return std::find(cmds.begin(), cmds.end(), cmd) - cmds.begin(); };
  EXPECT_LT(pos(0xE5), pos(0x10));
  EXPECT_LT(pos(0x10), pos(0x13));
}

/// A push that yields partway through must resume the right plane at the right row.
TEST(EPaperUC8179Gray4, ResumesBothPlanesAfterYielding) {
  TestableUC8179Gray4 display(8, 4);
  RecordingDelegate bus(&display.dc, 6);  // two rows exceed MAX_TRANSFER_TIME
  display.install(&bus);

  draw_row(display, 0, {0, 0, 0, 0, 0, 0, 0, 0});
  draw_row(display, 1, {1, 1, 1, 1, 1, 1, 1, 1});
  draw_row(display, 2, {2, 2, 2, 2, 2, 2, 2, 2});
  draw_row(display, 3, {3, 3, 3, 3, 3, 3, 3, 3});
  const int calls = display.run_push();

  EXPECT_GT(calls, 2) << "the transfer never yielded, so this test proves nothing";
  const auto &cmds = bus.commands;
  EXPECT_EQ(std::count(cmds.begin(), cmds.end(), 0x10), 1);
  EXPECT_EQ(std::count(cmds.begin(), cmds.end(), 0x13), 1);
  // rows at levels 0..3: low bits 0 1 0 1, high bits 0 0 1 1, each inverted across the row
  EXPECT_EQ(bus.data[0x10], (Bytes{0xFF, 0x00, 0xFF, 0x00}));
  EXPECT_EQ(bus.data[0x13], (Bytes{0xFF, 0xFF, 0x00, 0x00}));
}

}  // namespace esphome::epaper_spi::testing
