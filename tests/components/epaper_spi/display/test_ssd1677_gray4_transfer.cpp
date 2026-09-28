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

  /// As configured with monochrome_partial_updates: full_update_every > 1.
  void install_with_partials(spi::SPIDelegate *delegate) {
    this->install(delegate);
    this->set_full_update_every(5);
    this->init_comparison_frame_();
    ASSERT_TRUE(this->sent_.is_valid());
  }

  /// What the base class would decide; 0 means the next push is a full one.
  void set_update_count(uint8_t count) { this->update_count_ = count; }

  /// Pretend only this rectangle changed.
  void set_dirty(uint16_t x_low, uint16_t y_low, uint16_t x_high, uint16_t y_high) {
    this->x_low_ = x_low;
    this->y_low_ = y_low;
    this->x_high_ = x_high;
    this->y_high_ = y_high;
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

/// Without partial updates enabled (the default) every refresh is the four-level sequence, even if
/// the update count says otherwise.
TEST(EPaperSSD1677Gray4, WithoutPartialUpdatesEveryRefreshIsFourLevel) {
  TestableSSD1677Gray4 display(8, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  display.set_update_count(1);
  display.refresh_screen(true);

  EXPECT_EQ(bus.commands, (Bytes{0x1A, 0x22, 0x20}));
  EXPECT_EQ(bus.data[0x1A], (Bytes{0x67, 0x00}));
  EXPECT_EQ(bus.data[0x22], (Bytes{0xD7}));
}

// --- With monochrome partial updates ------------------------------------------------------------

/// A full update is still four-level. It also records, as the frame the next partial update
/// compares against, what the panel shows in black-and-white terms: the high bit of each level.
TEST(EPaperSSD1677Gray4, FullPushRecordsTheHighBitsForTheNextPartial) {
  TestableSSD1677Gray4 display(8, 1);
  RecordingDelegate bus(&display.dc);
  display.install_with_partials(&bus);

  draw_row(display, 0, {0, 1, 2, 3, 0, 1, 2, 3});
  display.set_update_count(0);
  display.run_push();
  EXPECT_EQ(bus.data[0x24], (Bytes{0xCC})) << "full update is no longer the four-level split";
  EXPECT_EQ(bus.data[0x26], (Bytes{0xAA}));
  bus.clear();

  // Nothing changed: old and new planes must match, or the partial drives every pixel.
  display.set_update_count(1);
  display.run_push();
  EXPECT_EQ(bus.data[0x26], (Bytes{0x33})) << "comparison frame is not the high bits";
  EXPECT_EQ(bus.data[0x24], (Bytes{0x33}));
}

/// A partial update sends the comparison frame to 0x26 and the new frame's high bits to 0x24,
/// not inverted (it runs the black-and-white waveform), over the whole panel.
TEST(EPaperSSD1677Gray4, PartialPushSendsTheHighBitsInBlackAndWhite) {
  TestableSSD1677Gray4 display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install_with_partials(&bus);

  draw_row(display, 0, {0, 1, 2, 3, 0, 1, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3});
  draw_row(display, 1, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
  display.set_update_count(0);
  display.run_push();
  bus.clear();

  draw_row(display, 1, {3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
  display.set_dirty(0, 1, 8, 2);  // only the start of the second row changed
  display.set_update_count(1);
  display.run_push();

  EXPECT_EQ(bus.data[0x26], (Bytes{0x33, 0xFF, 0x00, 0x00})) << "old plane is not the frame on the panel";
  EXPECT_EQ(bus.data[0x24], (Bytes{0x33, 0xFF, 0xF0, 0x00})) << "new plane is not the whole frame's high bits";
}

/// A full update resets the controller, which does not keep RAM, so even when only part of the
/// frame changed it must send the whole panel.
TEST(EPaperSSD1677Gray4, FullPushWithPartialsEnabledCoversTheWholePanel) {
  TestableSSD1677Gray4 display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install_with_partials(&bus);

  display.set_dirty(8, 1, 16, 2);
  display.set_update_count(0);
  display.run_push();

  EXPECT_EQ(bus.data[0x24].size(), 4u) << "full update did not send the whole new plane";
  EXPECT_EQ(bus.data[0x26].size(), 4u) << "full update did not send the whole old plane";
}

/// The refresh matches what was sent: black-and-white for a partial update, four-level for a full.
TEST(EPaperSSD1677Gray4, PartialRefreshIsBlackAndWhiteAndFullIsFourLevel) {
  TestableSSD1677Gray4 display(8, 1);
  RecordingDelegate bus(&display.dc);
  display.install_with_partials(&bus);

  display.set_update_count(1);
  display.refresh_screen(true);
  EXPECT_EQ(bus.commands, (Bytes{0x3C, 0x22, 0x20}));
  EXPECT_EQ(bus.data[0x22], (Bytes{0xFF})) << "partial update did not use the black-and-white waveform";
  // The model's border setting is right for the four-level waveform only; under this one it
  // would drive the border black on every partial.
  EXPECT_EQ(bus.data[0x3C], (Bytes{0x01})) << "partial update did not switch the border to LUT1";
  bus.clear();

  display.set_update_count(0);
  display.refresh_screen(false);
  EXPECT_EQ(bus.data[0x22], (Bytes{0xD7})) << "full update did not use the four-level waveform";
  EXPECT_EQ(bus.data.count(0x3C), 0u) << "full update overrode the model's border setting";
}

}  // namespace esphome::epaper_spi::testing
