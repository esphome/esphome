#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "../common.h"
#include "esphome/components/epaper_spi/epaper_spi_ssd1677_gray4.h"

namespace esphome::epaper_spi::testing {

class TestableSSD1677Gray4 : public EPaperSSD1677Gray4 {
 public:
  TestableSSD1677Gray4(uint16_t width, uint16_t height) : EPaperSSD1677Gray4("test", width, height, nullptr, 0) {}

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

  using EPaperSSD1677Gray4::refresh_screen;

  RecordingPin dc;
};

using Bytes = std::vector<uint8_t>;

namespace {

/// A gray that lands squarely on each of the four levels.
Color color_for_level(uint8_t level) {
  static const uint8_t GRAYS[4] = {0, 64, 128, 255};
  const uint8_t v = GRAYS[level];
  return Color(v, v, v);
}

void draw_row(TestableSSD1677Gray4 &display, int y, const std::vector<uint8_t> &levels) {
  for (size_t x = 0; x != levels.size(); x++)
    display.draw_pixel_at((int) x, y, color_for_level(levels[x]));
}

}  // namespace

/// Each pixel's 2-bit level is split across the RAM planes: the high bit to 0x24, the low bit to
/// 0x26, both inverted because the four-level waveform reads 1 as white.
TEST(EPaperSSD1677Gray4, SplitsEachLevelAcrossBothPlanes) {
  TestableSSD1677Gray4 display(8, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  draw_row(display, 0, {0, 1, 2, 3, 0, 1, 2, 3});
  display.run_push();

  //         levels  0 1 2 3 0 1 2 3
  // high bit        0 0 1 1 0 0 1 1 = 0x33, inverted 0xCC
  // low  bit        0 1 0 1 0 1 0 1 = 0x55, inverted 0xAA
  EXPECT_EQ(bus.data[0x24], (Bytes{0xCC}));
  EXPECT_EQ(bus.data[0x26], (Bytes{0xAA}));
}

/// Two buffer bytes (4 pixels each) make one plane byte (8 pixels), leftmost pixel in the most
/// significant bit. An asymmetric row catches a swapped pair or reversed bit order.
TEST(EPaperSSD1677Gray4, PacksPixelsLeftmostFirstAcrossSourceBytes) {
  TestableSSD1677Gray4 display(16, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  draw_row(display, 0, {3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2});
  display.run_push();

  // high bits: pixel 0 (3) and pixel 15 (2) -> 0x80 0x01, inverted 0x7F 0xFE
  // low  bits: pixel 0 (3) only             -> 0x80 0x00, inverted 0x7F 0xFF
  EXPECT_EQ(bus.data[0x24], (Bytes{0x7F, 0xFE}));
  EXPECT_EQ(bus.data[0x26], (Bytes{0x7F, 0xFF}));
}

/// The high-bit plane goes out first, each plane exactly once per push.
TEST(EPaperSSD1677Gray4, WritesTheHighBitPlaneBeforeTheLowBitPlane) {
  TestableSSD1677Gray4 display(8, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  display.run_push();

  const auto &cmds = bus.commands;
  ASSERT_EQ(std::count(cmds.begin(), cmds.end(), 0x24), 1);
  ASSERT_EQ(std::count(cmds.begin(), cmds.end(), 0x26), 1);
  EXPECT_LT(std::find(cmds.begin(), cmds.end(), 0x24) - cmds.begin(),
            std::find(cmds.begin(), cmds.end(), 0x26) - cmds.begin());
}

/// A push that yields partway through must resume the right plane at the right row.
TEST(EPaperSSD1677Gray4, ResumesBothPlanesAfterYielding) {
  TestableSSD1677Gray4 display(8, 4);
  RecordingDelegate bus(&display.dc, 6);  // two rows exceed MAX_TRANSFER_TIME
  display.install(&bus);

  draw_row(display, 0, {0, 0, 0, 0, 0, 0, 0, 0});
  draw_row(display, 1, {1, 1, 1, 1, 1, 1, 1, 1});
  draw_row(display, 2, {2, 2, 2, 2, 2, 2, 2, 2});
  draw_row(display, 3, {3, 3, 3, 3, 3, 3, 3, 3});
  const int calls = display.run_push();

  EXPECT_GT(calls, 2) << "the transfer never yielded, so this test proves nothing";
  // rows at levels 0..3: high bits 0 0 1 1, low bits 0 1 0 1, each inverted across the row
  EXPECT_EQ(bus.data[0x24], (Bytes{0xFF, 0xFF, 0x00, 0x00}));
  EXPECT_EQ(bus.data[0x26], (Bytes{0xFF, 0x00, 0xFF, 0x00}));
}

/// There is no partial four-level update: whatever the caller asks for, the refresh is the panel's
/// four-level sequence.
TEST(EPaperSSD1677Gray4, RefreshIsAlwaysTheFourLevelSequence) {
  TestableSSD1677Gray4 display(8, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  display.refresh_screen(true);

  EXPECT_EQ(bus.commands, (Bytes{0x1A, 0x22, 0x20}));
  EXPECT_EQ(bus.data[0x1A], (Bytes{0x67, 0x00}));
  EXPECT_EQ(bus.data[0x22], (Bytes{0xD7}));
}

}  // namespace esphome::epaper_spi::testing
