#include <gtest/gtest.h>

#include <algorithm>
#include <initializer_list>
#include <vector>

#include "../common.h"
#include "esphome/components/epaper_spi/epaper_spi_ssd1681.h"

namespace esphome::epaper_spi::testing {

class TestableSSD1681 : public EPaperSSD1681 {
 public:
  TestableSSD1681(uint16_t width, uint16_t height) : EPaperSSD1681("test", width, height, nullptr, 0) {}

  void install(spi::SPIDelegate *delegate, uint8_t full_update_every) {
    this->delegate_ = delegate;
    this->set_dc_pin(&this->dc);
    this->set_reset_pin(&this->reset_pin);
    ASSERT_TRUE(this->init_buffer_(this->buffer_length_));
    this->set_full_update_every(full_update_every);
  }

  EPaperState state() const { return this->state_; }
  void step() { this->process_state_(); }
  uint8_t update_count() const { return this->update_count_; }

  void set_frame(std::initializer_list<uint8_t> bytes) {
    size_t i = 0;
    for (const uint8_t byte : bytes)
      this->buffer_[i++] = byte;
  }

  void set_dirty(uint16_t x_low, uint16_t y_low, uint16_t x_high, uint16_t y_high) {
    this->x_low_ = x_low;
    this->y_low_ = y_low;
    this->x_high_ = x_high;
    this->y_high_ = y_high;
  }

  /// Run the UPDATE state with the frame already drawn, and report whether a push follows.
  bool run_update_state() {
    this->set_auto_clear(false);
    this->state_ = EPaperState::UPDATE;
    this->process_state_();
    return this->state_ == EPaperState::RESET;
  }

  /// Run an update from the point where the frame has been drawn through to idle; false if skipped.
  bool run_update() {
    const bool pushed = this->run_update_state();
    while (this->state_ != EPaperState::IDLE)
      this->process_state_();
    return pushed;
  }

  RecordingPin dc;
  RecordingPin reset_pin;
};

using Bytes = std::vector<uint8_t>;

/// A full refresh ignores 0x26, but both RAM banks must end up holding the whole frame so that
/// later partial refreshes compare against a known image, whatever area was drawn.
TEST(EPaperSSD1681, FullRefreshSendsTheWholeFrameToBothBanks) {
  TestableSSD1681 display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 5);

  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  display.set_dirty(8, 1, 16, 2);
  display.run_update();

  EXPECT_EQ(bus.data[0x26], (Bytes{0x0F, 0xF0, 0x3C, 0xC3}));
  EXPECT_EQ(bus.data[0x24], (Bytes{0x0F, 0xF0, 0x3C, 0xC3}));
  EXPECT_EQ(bus.data[0x21], (Bytes{0x40, 0x00}));
  EXPECT_EQ(bus.data[0x22], (Bytes{0xF7}));
}

/// A partial refresh writes the whole new frame to 0x24 and leaves 0x26 alone: the controller's RAM
/// ping-pong carries the last 0x24 over to 0x26, so 0x26 already holds the image on the panel.
TEST(EPaperSSD1681, PartialRefreshWritesTheWholeNewFrameToTheNewImageBankOnly) {
  TestableSSD1681 display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 5);

  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  display.run_update();
  bus.clear();

  display.set_frame({0x0F, 0xF0, 0x3C, 0x00});
  display.set_dirty(8, 1, 16, 2);  // only the last byte changed
  display.run_update();

  EXPECT_EQ(bus.data[0x24], (Bytes{0x0F, 0xF0, 0x3C, 0x00})) << "the whole frame was not sent to 0x24";
  EXPECT_EQ(bus.data.count(0x26), 0u) << "0x26 was written";
  EXPECT_EQ(bus.data[0x44], (Bytes{0, 1})) << "the x window was not the whole width";
  EXPECT_EQ(bus.data[0x45], (Bytes{0, 0, 1, 0})) << "the y window was not the whole height";
  EXPECT_EQ(bus.data[0x21], (Bytes{0x00, 0x00})) << "0x26 is not compared as it is";
  EXPECT_EQ(bus.data[0x22], (Bytes{0xFF}));
}

/// Regression test for successive partial refreshes alternating between two images.
///
/// The RAM ping-pong the panel's OTP may enable swaps the bank that 0x24 reaches after each refresh,
/// so writing only the changed window left the next bank holding the image from two frames ago.
/// Writing the whole new frame to 0x24 every time overwrites that stale bank, with no copy kept on
/// the microcontroller.
TEST(EPaperSSD1681, SuccessivePartialRefreshesEachSendTheWholeNewFrame) {
  TestableSSD1681 display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 5);

  display.set_frame({0xFF, 0xFF, 0xFF, 0xFF});
  display.run_update();

  const std::vector<Bytes> frames{
      {0x7F, 0xFF, 0xFF, 0xFF}, {0x7F, 0xFE, 0xFF, 0xFF}, {0x7F, 0xFE, 0x7F, 0xFF}, {0x7F, 0xFE, 0x7F, 0xFE}};
  for (const Bytes &frame : frames) {
    bus.clear();
    display.set_frame({frame[0], frame[1], frame[2], frame[3]});
    display.set_dirty(0, 0, 16, 2);
    display.run_update();
    EXPECT_EQ(bus.data[0x24], frame) << "the whole frame was not sent to 0x24";
    EXPECT_EQ(bus.data.count(0x26), 0u) << "0x26 was written";
    EXPECT_EQ(bus.data[0x22], (Bytes{0xFF}));
  }
}

/// The whole new frame goes to 0x24 in chunks on a partial, so the main loop keeps running, and it
/// arrives whole against the panel's retained 0x26.
TEST(EPaperSSD1681, PartialSendsTheWholeFrameToTheNewImageBankInChunks) {
  TestableSSD1681 display(16, 2);
  RecordingDelegate bus(&display.dc, MAX_TRANSFER_TIME + 1);
  display.install(&bus, 5);

  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  display.run_update();
  bus.clear();

  display.set_frame({0x0F, 0xF0, 0x3C, 0x00});
  display.set_dirty(0, 0, 16, 2);
  display.run_update_state();
  int transfer_steps = 0;
  while (display.state() != EPaperState::IDLE) {
    if (display.state() == EPaperState::TRANSFER_DATA)
      transfer_steps++;
    display.step();
  }

  EXPECT_GT(transfer_steps, 1) << "the frame was not sent in chunks";
  EXPECT_EQ(bus.data[0x24], (Bytes{0x0F, 0xF0, 0x3C, 0x00})) << "the whole frame did not arrive at 0x24";
  EXPECT_EQ(bus.data.count(0x26), 0u) << "0x26 was written";
}

/// The software reset 0x12 puts the display option register back to its default, which drops the
/// OTP's settings for the partial waveform and makes a partial refresh as slow as a full one. So
/// every update starts with the hardware reset only, and ends in deep sleep mode 1, which keeps the
/// RAM and is left by that reset.
TEST(EPaperSSD1681, UpdatesResetInHardwareOnlyAndSleepInBetween) {
  TestableSSD1681 display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 5);

  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  display.run_update();
  EXPECT_EQ(std::count(bus.commands.begin(), bus.commands.end(), 0x12), 0) << "full refresh sent a software reset";
  EXPECT_EQ(bus.data[0x10], (Bytes{0x01})) << "panel was not put into deep sleep mode 1";
  bus.clear();

  display.set_frame({0x0F, 0xF0, 0x3C, 0x00});
  display.set_dirty(8, 1, 16, 2);
  display.run_update();
  EXPECT_EQ(std::count(bus.commands.begin(), bus.commands.end(), 0x12), 0) << "partial refresh sent a software reset";
  EXPECT_EQ(bus.data[0x10], (Bytes{0x01})) << "panel was not put into deep sleep mode 1";
}

/// With every update a full one the controller loses its RAM in deep sleep mode 2, as before.
TEST(EPaperSSD1681, EveryUpdateFullSleepsWithoutKeepingRAM) {
  TestableSSD1681 display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 1);

  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  display.run_update();
  bus.clear();
  display.set_frame({0x0F, 0xF0, 0x3C, 0x00});
  display.set_dirty(8, 1, 16, 2);
  display.run_update();

  EXPECT_EQ(bus.data[0x26], (Bytes{0x0F, 0xF0, 0x3C, 0x00}));
  EXPECT_EQ(bus.data[0x24], (Bytes{0x0F, 0xF0, 0x3C, 0x00}));
  EXPECT_EQ(bus.data[0x21], (Bytes{0x40, 0x00}));
  EXPECT_EQ(bus.data[0x10], (Bytes{0x03}));
}

/// An update whose frame is the one already on the panel sends nothing and does not count towards
/// the next full refresh. The first update after boot always pushes: nothing is known to be on
/// the panel yet.
TEST(EPaperSSD1681, UnchangedFrameSkipsTheRefresh) {
  TestableSSD1681 display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 5);

  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  display.set_dirty(0, 0, 16, 2);
  EXPECT_TRUE(display.run_update()) << "first update was skipped";
  EXPECT_EQ(display.update_count(), 1);
  bus.clear();

  display.set_dirty(0, 0, 16, 2);  // redrawn, but with the same content
  EXPECT_FALSE(display.run_update()) << "unchanged frame was pushed";
  EXPECT_TRUE(bus.commands.empty());
  EXPECT_EQ(display.update_count(), 1) << "skipped update counted towards the next full refresh";

  display.set_frame({0x0F, 0xF0, 0x3C, 0x00});
  display.set_dirty(0, 0, 16, 2);
  EXPECT_TRUE(display.run_update()) << "changed frame was skipped";
  EXPECT_EQ(display.update_count(), 2);
}

/// The first update sends the whole frame whatever was drawn, so the skip works without auto clear.
TEST(EPaperSSD1681, SkipWorksWhenOnlyPartOfThePanelIsDrawn) {
  TestableSSD1681 display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 5);

  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  display.set_dirty(8, 1, 16, 2);
  EXPECT_TRUE(display.run_update());
  bus.clear();

  display.set_dirty(8, 1, 16, 2);
  EXPECT_FALSE(display.run_update()) << "unchanged frame was pushed";
  EXPECT_TRUE(bus.commands.empty());
}

/// A requested full update pushes even if the frame is unchanged.
TEST(EPaperSSD1681, RequestedFullUpdateIsNotSkipped) {
  TestableSSD1681 display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 5);

  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  display.set_dirty(0, 0, 16, 2);
  display.run_update();
  bus.clear();

  display.request_full_update();
  EXPECT_TRUE(display.run_update());
  EXPECT_EQ(bus.data[0x22], (Bytes{0xF7}));
  EXPECT_EQ(display.update_count(), 1);
}

/// A pixel drawn while a refresh is in flight is sent by the next update: the whole frame goes to
/// 0x24 each time, so nothing drawn is lost.
TEST(EPaperSSD1681, PixelDrawnDuringARefreshIsSentNext) {
  TestableSSD1681 display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 5);

  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  display.set_dirty(0, 0, 16, 2);
  display.run_update();

  display.set_frame({0x0F, 0xF0, 0x3C, 0x00});
  display.set_dirty(8, 1, 16, 2);
  display.run_update_state();
  while (display.state() != EPaperState::POWER_ON)
    display.step();
  display.draw_pixel_at(4, 0, Color::BLACK);  // drawn during the refresh
  while (display.state() != EPaperState::IDLE)
    display.step();
  bus.clear();

  display.set_dirty(0, 0, 16, 2);
  EXPECT_TRUE(display.run_update()) << "pixel drawn during the refresh was never sent";
  EXPECT_EQ(bus.data[0x24], (Bytes{0x07, 0xF0, 0x3C, 0x00}));
}

}  // namespace esphome::epaper_spi::testing
