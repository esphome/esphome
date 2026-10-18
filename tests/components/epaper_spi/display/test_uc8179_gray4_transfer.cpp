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

  /// A whole update with the frame already drawn, run through loop() as the main loop would.
  void run_update() {
    this->set_auto_clear(false);
    this->state_ = EPaperState::UPDATE;
    const uint32_t deadline = millis() + 2000;
    while (this->state_ != EPaperState::IDLE) {
      ASSERT_LT(millis(), deadline) << "update did not finish";
      this->loop();
    }
  }

  using EPaperUC8179Gray4::refresh_screen;
  using EPaperUC8179Gray4::transfer_data;

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

void draw_row(TestableUC8179Gray4 &display, int y, const std::vector<uint8_t> &levels) {
  for (size_t x = 0; x != levels.size(); x++)
    display.draw_pixel_at((int) x, y, color_for_level(levels[x]));
}

}  // namespace

/// The high bit of each level goes to DTM1 (0x10), the low bit to DTM2 (0x13), neither inverted.
TEST(EPaperUC8179Gray4, SplitsEachLevelAcrossBothPlanes) {
  TestableUC8179Gray4 display(8, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  draw_row(display, 0, {0, 1, 2, 3, 0, 1, 2, 3});
  display.run_push();

  //         levels  0 1 2 3 0 1 2 3
  // high bit        0 0 1 1 0 0 1 1 = 0x33
  // low  bit        0 1 0 1 0 1 0 1 = 0x55
  EXPECT_EQ(bus.data[0x10], (Bytes{0x33}));
  EXPECT_EQ(bus.data[0x13], (Bytes{0x55}));
}

/// Leftmost pixel in the most significant bit, two buffer bytes per plane byte.
TEST(EPaperUC8179Gray4, PacksPixelsLeftmostFirstAcrossSourceBytes) {
  TestableUC8179Gray4 display(16, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  draw_row(display, 0, {3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2});
  display.run_push();

  EXPECT_EQ(bus.data[0x10], (Bytes{0x80, 0x01}));
  EXPECT_EQ(bus.data[0x13], (Bytes{0x80, 0x00}));
}

/// Fill sets every pixel to one level; white is both bits set on both planes.
TEST(EPaperUC8179Gray4, FillWhiteSetsBothPlanes) {
  TestableUC8179Gray4 display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  display.fill(Color::WHITE);
  display.run_push();

  EXPECT_EQ(bus.data[0x10], (Bytes{0xFF, 0xFF, 0xFF, 0xFF}));
  EXPECT_EQ(bus.data[0x13], (Bytes{0xFF, 0xFF, 0xFF, 0xFF}));
}

/// The refresh registers go first, then DTM1, then DTM2, each once.
TEST(EPaperUC8179Gray4, SendsRefreshRegistersThenHighThenLowPlane) {
  TestableUC8179Gray4 display(8, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  display.run_push();

  EXPECT_EQ(bus.commands, (Bytes{0x50, 0xE5, 0x92, 0x10, 0x13}));
  EXPECT_EQ(bus.data[0x50], (Bytes{0xA9, 0x07}));
  EXPECT_EQ(bus.data[0xE5], (Bytes{0x5A}));
}

/// A push that yields partway through resumes the right plane at the right byte.
TEST(EPaperUC8179Gray4, ResumesBothPlanesAfterYielding) {
  // 300 bytes per plane: three chunks each
  TestableUC8179Gray4 display(8, 300);
  RecordingDelegate bus(&display.dc, 11);  // every chunk exceeds MAX_TRANSFER_TIME
  display.install(&bus);

  for (int y = 0; y != 300; y++) {
    const uint8_t level = y % 4;
    draw_row(display, y, std::vector<uint8_t>(8, level));
  }
  const int calls = display.run_push();

  EXPECT_GT(calls, 2) << "the transfer never yielded, so this test proves nothing";
  EXPECT_EQ(std::count(bus.commands.begin(), bus.commands.end(), 0x10), 1);
  EXPECT_EQ(std::count(bus.commands.begin(), bus.commands.end(), 0x13), 1);
  EXPECT_EQ(std::count(bus.commands.begin(), bus.commands.end(), 0x50), 1);
  ASSERT_EQ(bus.data[0x10].size(), 300u);
  ASSERT_EQ(bus.data[0x13].size(), 300u);
  for (size_t y = 0; y != 300; y++) {
    const uint8_t level = y % 4;
    EXPECT_EQ(bus.data[0x10][y], (level & 2) ? 0xFF : 0x00) << "row " << y;
    EXPECT_EQ(bus.data[0x13][y], (level & 1) ? 0xFF : 0x00) << "row " << y;
  }
}

TEST(EPaperUC8179Gray4, RefreshSendsDisplayRefresh) {
  TestableUC8179Gray4 display(8, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  display.refresh_screen(false);

  EXPECT_EQ(bus.commands, (Bytes{0x12}));
}

/// A whole update: power on, refresh registers, both planes, refresh, power off, deep sleep.
TEST(EPaperUC8179Gray4, FullUpdateSequence) {
  TestableUC8179Gray4 display(8, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  display.run_update();

  EXPECT_EQ(bus.commands, (Bytes{0x04, 0x50, 0xE5, 0x92, 0x10, 0x13, 0x12, 0x02, 0x07}));
  EXPECT_EQ(bus.data[0x07], (Bytes{0xA5}));
}

}  // namespace esphome::epaper_spi::testing
