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

  /// Run an update from the point where the frame has been drawn through to idle.
  void run_update() {
    this->state_ = EPaperState::RESET;
    while (this->state_ != EPaperState::IDLE)
      this->process_state_();
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

/// The software reset 0x12 turns off the RAM ping-pong the panel's OTP enables, after which the
/// controller no longer keeps 0x26 in step and the next partial refresh compares against a stale
/// image. So every update starts with the hardware reset only, and ends in deep sleep mode 1, which
/// keeps the RAM and is left by that reset.
TEST(EPaperSSD1681, UpdatesResetInHardwareOnlyAndSleepInBetween) {
  TestableSSD1681 display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 5);

  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  display.run_update();
  EXPECT_EQ(bus.commands.front(), 0x44) << "full refresh sent a software reset";
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

}  // namespace esphome::epaper_spi::testing
