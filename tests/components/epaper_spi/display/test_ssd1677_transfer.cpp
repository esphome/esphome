#include <gtest/gtest.h>

#include <initializer_list>
#include <vector>

#include "../common.h"
#include "esphome/components/epaper_spi/epaper_spi_ssd1677.h"

namespace esphome::epaper_spi::testing {

class TestableSSD1677 : public EPaperSSD1677 {
 public:
  TestableSSD1677(uint16_t width, uint16_t height) : EPaperSSD1677("test", width, height, nullptr, 0) {}

  void install(spi::SPIDelegate *delegate, uint8_t full_update_every) {
    this->delegate_ = delegate;
    this->set_dc_pin(&this->dc);
    this->set_reset_pin(&this->reset_pin);
    ASSERT_TRUE(this->init_buffer_(this->buffer_length_));
    this->set_full_update_every(full_update_every);
    this->init_comparison_frame_();
  }

  bool has_comparison_frame() const { return this->sent_.is_valid(); }

  void set_frame(std::initializer_list<uint8_t> bytes) {
    size_t i = 0;
    for (const uint8_t byte : bytes)
      this->buffer_[i++] = byte;
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

  /// One call into the transfer; false while there is more to send.
  bool step() { return this->transfer_data(); }

  /// Both planes of one push; returns how many calls it took.
  int run_push() {
    int calls = 1;
    while (!this->transfer_data())
      calls++;
    return calls;
  }

  bool reset_in(EPaperState state) {
    this->state_ = state;
    return this->reset();
  }

  RecordingPin dc;
  RecordingPin reset_pin;
};

using Bytes = std::vector<uint8_t>;

/// A full push ignores the old-image plane, and on the first push after boot the comparison frame
/// holds nothing real yet, so the new frame goes to both planes.
TEST(EPaperSSD1677, FullPushSendsTheNewFrameToBothPlanes) {
  TestableSSD1677 display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 5);

  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  display.set_update_count(0);
  display.run_push();

  EXPECT_EQ(bus.data[0x26], (Bytes{0x0F, 0xF0, 0x3C, 0xC3}));
  EXPECT_EQ(bus.data[0x24], (Bytes{0x0F, 0xF0, 0x3C, 0xC3}));
}

/// Regression test.
///
/// A partial refresh drives every pixel from the pair (0x26 = the image on the panel, 0x24 = the
/// new image), across the whole panel whatever RAM window was written. The controller does not
/// keep its RAM intact between updates, so sending only the changed window of 0x24 - and 0x26 once
/// - leaves the pair wrong outside that window: unchanged pixels get driven on every partial and
/// wash out. Both planes must go out whole, 0x26 holding the frame actually on the panel.
TEST(EPaperSSD1677, PartialPushComparesAgainstTheFrameOnThePanel) {
  TestableSSD1677 display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 5);

  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  display.set_update_count(0);
  display.run_push();
  bus.clear();

  display.set_frame({0x0F, 0xF0, 0x3C, 0x00});
  display.set_dirty(8, 1, 16, 2);  // only the last byte changed
  display.set_update_count(1);
  display.run_push();

  EXPECT_EQ(bus.data[0x26], (Bytes{0x0F, 0xF0, 0x3C, 0xC3})) << "old plane is not the frame on the panel";
  EXPECT_EQ(bus.data[0x24], (Bytes{0x0F, 0xF0, 0x3C, 0x00})) << "new plane is not the whole new frame";
  // The RAM window, set once per plane, must span the panel too, not the changed rectangle.
  EXPECT_EQ(bus.data[0x44], (Bytes{0, 0, 15, 0, 0, 0, 15, 0})) << "x window is not the whole panel";
  EXPECT_EQ(bus.data[0x45], (Bytes{0, 0, 1, 0, 0, 0, 1, 0})) << "y window is not the whole panel";
}

/// The comparison frame must record the bytes that went to 0x24, not whatever the buffer holds
/// later: LVGL can draw into the buffer while a push is in progress. Here the buffer changes
/// between the two planes of a push; the next push must compare against what was actually sent.
TEST(EPaperSSD1677, ComparisonFrameIsWhatWasSentNotTheBuffer) {
  TestableSSD1677 display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 5);

  display.set_frame({0x11, 0x11, 0x11, 0x11});
  display.set_update_count(0);
  display.run_push();

  display.set_frame({0x22, 0x22, 0x22, 0x22});
  display.set_update_count(1);
  ASSERT_FALSE(display.step()) << "expected the old plane to go out on its own first";
  display.set_frame({0x33, 0x33, 0x33, 0x33});  // drawn mid-push, before the new plane
  while (!display.step()) {
  }
  ASSERT_EQ(bus.data[0x24].size(), 8u);
  EXPECT_EQ(Bytes(bus.data[0x24].begin() + 4, bus.data[0x24].end()), (Bytes{0x33, 0x33, 0x33, 0x33}));
  bus.clear();

  display.set_frame({0x44, 0x44, 0x44, 0x44});
  display.set_update_count(2);
  display.run_push();

  EXPECT_EQ(bus.data[0x26], (Bytes{0x33, 0x33, 0x33, 0x33})) << "old plane is not what was last sent";
}

/// Two full planes can take several loop iterations to send; each resumed call must continue the
/// right plane at the right row.
TEST(EPaperSSD1677, ResumesTheRightPlaneAfterYielding) {
  TestableSSD1677 display(16, 4);
  RecordingDelegate bus(&display.dc, 25);  // two rows exceed the 40 ms block
  display.install(&bus, 5);

  display.set_frame({1, 2, 3, 4, 5, 6, 7, 8});
  display.set_update_count(0);
  display.run_push();
  bus.clear();

  display.set_frame({9, 10, 11, 12, 13, 14, 15, 16});
  display.set_update_count(1);
  const int calls = display.run_push();

  EXPECT_GT(calls, 2) << "the transfer never yielded, so this test proves nothing";
  EXPECT_EQ(bus.data[0x26], (Bytes{1, 2, 3, 4, 5, 6, 7, 8}));
  EXPECT_EQ(bus.data[0x24], (Bytes{9, 10, 11, 12, 13, 14, 15, 16}));
}

/// Nothing a partial needs lives in controller RAM any more, so a partial push skips the reset
/// altogether; a full one still gets the hardware pulse and the software reset.
TEST(EPaperSSD1677, PartialPushSkipsTheResetAndAFullPushKeepsIt) {
  TestableSSD1677 display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 5);

  display.set_update_count(1);
  EXPECT_TRUE(display.reset_in(EPaperState::RESET)) << "partial push waited on a reset";
  EXPECT_TRUE(display.reset_pin.level) << "partial push pulsed the reset pin";
  EXPECT_TRUE(bus.commands.empty()) << "partial push sent a software reset";

  display.set_update_count(0);
  EXPECT_FALSE(display.reset_in(EPaperState::RESET));
  EXPECT_FALSE(display.reset_pin.level) << "full push did not pulse the reset pin";
  EXPECT_TRUE(display.reset_in(EPaperState::RESET_END));
  EXPECT_TRUE(display.reset_pin.level);
  EXPECT_EQ(bus.commands, (Bytes{0x12})) << "full push did not send a software reset";
}

/// With every update a full one nothing is ever compared against 0x26, so no comparison frame is
/// allocated and the transfer is EPaperMono's.
TEST(EPaperSSD1677, NoComparisonFrameWhenEveryUpdateIsFull) {
  TestableSSD1677 display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 1);

  EXPECT_FALSE(display.has_comparison_frame());
  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  display.set_update_count(0);
  display.run_push();
  EXPECT_EQ(bus.data[0x24], (Bytes{0x0F, 0xF0, 0x3C, 0xC3}));
}

}  // namespace esphome::epaper_spi::testing
