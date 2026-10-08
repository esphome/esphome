#include <gtest/gtest.h>

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
    this->init_sent_frame_(this->buffer_length_);
  }

  bool has_comparison_frame() const { return this->sent_.is_valid(); }
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

/// Regression test for speckled white areas after a partial refresh.
///
/// With the new frame in both banks and 0x26 inverted, the controller treats every pixel as
/// changed and drives it with the short partial waveform, which does not fully restore white on
/// this panel. Only the changed window of 0x24 is written before a partial refresh, with 0x26
/// compared as it is; the controller copies 0x24 into 0x26 after each refresh by itself.
TEST(EPaperSSD1681, PartialRefreshWritesOnlyTheChangedWindowOfTheNewImageBank) {
  TestableSSD1681 display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 5);

  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  display.run_update();
  bus.clear();

  display.set_frame({0x0F, 0xF0, 0x3C, 0x00});
  display.set_dirty(8, 1, 16, 2);  // only the last byte changed
  display.run_update();

  EXPECT_EQ(bus.data[0x24], (Bytes{0x00}));
  EXPECT_EQ(bus.data.count(0x26), 0u) << "0x26 was written";
  EXPECT_EQ(bus.data[0x44], (Bytes{1, 1}));
  EXPECT_EQ(bus.data[0x45], (Bytes{1, 0, 1, 0}));
  EXPECT_EQ(bus.data[0x21], (Bytes{0x00, 0x00})) << "0x26 is not compared as it is";
  EXPECT_EQ(bus.data[0x22], (Bytes{0xFF}));
}

/// Controller RAM does not survive the hardware reset, and deep sleep can only be left by one, so
/// while partial refreshes are in use the panel is never put to sleep and only a full refresh
/// resets it.
TEST(EPaperSSD1681, PanelStaysAwakeAndUnresetBetweenPartialRefreshes) {
  TestableSSD1681 display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 5);

  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  display.run_update();
  EXPECT_EQ(bus.commands.front(), 0x12) << "full refresh did not send a software reset";
  EXPECT_EQ(bus.commands.back(), 0x20) << "panel was put to sleep after the full refresh";
  bus.clear();

  display.set_frame({0x0F, 0xF0, 0x3C, 0x00});
  display.set_dirty(8, 1, 16, 2);
  display.run_update();
  EXPECT_EQ(bus.commands.front(), 0x44) << "partial refresh sent a software reset";
  EXPECT_EQ(bus.commands.back(), 0x20) << "panel was put to sleep after the partial refresh";
  EXPECT_TRUE(display.reset_pin.level) << "partial refresh pulsed the reset pin";
}

/// With every update a full one the controller is reset and put to sleep every time, as before.
TEST(EPaperSSD1681, EveryUpdateFullResetsAndSleepsEveryTime) {
  TestableSSD1681 display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 1);

  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  display.run_update();
  bus.clear();
  display.set_frame({0x0F, 0xF0, 0x3C, 0x00});
  display.set_dirty(8, 1, 16, 2);
  display.run_update();

  EXPECT_EQ(bus.commands.front(), 0x12);
  EXPECT_EQ(bus.data[0x26], (Bytes{0x0F, 0xF0, 0x3C, 0x00}));
  EXPECT_EQ(bus.data[0x24], (Bytes{0x0F, 0xF0, 0x3C, 0x00}));
  EXPECT_EQ(bus.data[0x21], (Bytes{0x40, 0x00}));
  EXPECT_EQ(bus.commands.back(), 0x10);
}

/// An update whose frame is the one already on the panel sends nothing and does not count towards
/// the next full refresh. The first update after boot always pushes: nothing is known to be on
/// the panel yet.
TEST(EPaperSSD1681, UnchangedFrameSkipsTheRefresh) {
  TestableSSD1681 display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 5);
  ASSERT_TRUE(display.has_comparison_frame());

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

/// A partial refresh sends only the bytes that differ from the frame on the panel, whatever area
/// was drawn: with auto clear on, the drawn area is always the whole panel.
TEST(EPaperSSD1681, PartialRefreshSendsOnlyTheBytesThatChanged) {
  TestableSSD1681 display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 5);

  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  display.set_dirty(0, 0, 16, 2);
  display.run_update();
  bus.clear();

  display.set_frame({0x0F, 0xF0, 0x3C, 0x00});
  display.set_dirty(0, 0, 16, 2);  // whole panel redrawn, only the last byte differs
  display.run_update();

  EXPECT_EQ(bus.data[0x24], (Bytes{0x00}));
  EXPECT_EQ(bus.data[0x44], (Bytes{1, 1}));
  EXPECT_EQ(bus.data[0x45], (Bytes{1, 0, 1, 0}));
}

/// The comparison frame holds what was sent, so a change that was drawn but not pushed yet
/// (because it was drawn during a push) is still sent by the next update.
TEST(EPaperSSD1681, ComparisonFrameIsWhatWasSent) {
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
  EXPECT_EQ(bus.data[0x24], (Bytes{0x07}));
}

}  // namespace esphome::epaper_spi::testing
