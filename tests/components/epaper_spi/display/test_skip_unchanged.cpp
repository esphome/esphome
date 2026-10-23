#include <gtest/gtest.h>

#include <initializer_list>
#include <vector>

#include "../common.h"
#include "esphome/components/epaper_spi/epaper_spi_ssd1683.h"

namespace esphome::epaper_spi::testing {

/// Drives a driver through whole updates so the skip in the UPDATE state can be checked.
template<typename Driver> class TestableDriver : public Driver {
 public:
  TestableDriver(uint16_t width, uint16_t height) : Driver("test", width, height, nullptr, 0) {}

  void install(spi::SPIDelegate *delegate, uint8_t full_update_every) {
    this->delegate_ = delegate;
    this->set_dc_pin(&this->dc);
    this->set_full_update_every(full_update_every);
    ASSERT_TRUE(this->init_buffer_(this->buffer_length_));
  }

  void set_frame(std::initializer_list<uint8_t> bytes) {
    size_t i = 0;
    for (const uint8_t byte : bytes)
      this->buffer_[i++] = byte;
    this->x_low_ = 0;
    this->y_low_ = 0;
    this->x_high_ = this->width_;
    this->y_high_ = this->height_;
  }

  /// A whole update, with auto clear and the writer as a YAML lambda runs if asked; false if skipped.
  bool run_update(bool auto_clear = false) {
    this->set_auto_clear(auto_clear);
    this->state_ = EPaperState::UPDATE;
    this->process_state_();
    const bool pushed = this->state_ == EPaperState::RESET;
    while (this->state_ != EPaperState::IDLE)
      this->process_state_();
    return pushed;
  }

  /// A whole update with a pixel drawn after the frame was hashed, while it is being sent.
  void run_update_drawn_during_push(int x, int y) {
    this->state_ = EPaperState::UPDATE;
    while (this->state_ != EPaperState::TRANSFER_DATA)
      this->process_state_();
    this->draw_pixel_at(x, y, Color::BLACK);
    while (this->state_ != EPaperState::IDLE)
      this->process_state_();
  }

  /// An update in which nothing is drawn; false if it went straight back to idle.
  bool run_update_drawing_nothing() {
    this->state_ = EPaperState::UPDATE;
    this->process_state_();
    const bool pushed = this->state_ == EPaperState::RESET;
    while (this->state_ != EPaperState::IDLE)
      this->process_state_();
    return pushed;
  }

  /// An update given up on with the busy pin stuck, after its data was sent.
  void run_update_abandoned() {
    this->state_ = EPaperState::UPDATE;
    while (this->state_ != EPaperState::POWER_OFF)
      this->process_state_();
    this->abandon_update_();
  }

  uint8_t update_count() const { return this->update_count_; }
  bool shown_hash_valid() const { return this->shown_hash_valid_; }

  RecordingPin dc;
};

using Bytes = std::vector<uint8_t>;

TEST(EPaperSkipUnchanged, SSD1683SkipsAnUnchangedFrame) {
  TestableDriver<EPaperSSD1683> display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 5);

  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  EXPECT_TRUE(display.run_update());
  bus.clear();

  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  EXPECT_FALSE(display.run_update());
  EXPECT_TRUE(bus.commands.empty());
  EXPECT_EQ(display.update_count(), 1);

  display.set_frame({0x0F, 0xF0, 0x3C, 0x00});
  EXPECT_TRUE(display.run_update());
  EXPECT_EQ(bus.data[0x24], (Bytes{0x0F, 0xF0, 0x3C, 0x00}));
}

/// A frame drawn into while it was being sent is not trusted, so the frame it replaced is pushed again.
TEST(EPaperSkipUnchanged, FrameChangedDuringThePushIsNotTrusted) {
  TestableDriver<EPaperSSD1683> display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 5);

  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  EXPECT_TRUE(display.run_update());
  display.set_frame({0x0F, 0xF0, 0x3C, 0x00});
  display.run_update_drawn_during_push(15, 1);
  EXPECT_FALSE(display.shown_hash_valid());

  display.set_frame({0x0F, 0xF0, 0x3C, 0x00});
  EXPECT_TRUE(display.run_update()) << "frame that never reached the panel was skipped";
}

/// A draw during the push is sent by the next update even when that update draws nothing itself.
TEST(EPaperSkipUnchanged, DrawDuringThePushIsSentByAnUpdateThatDrawsNothing) {
  TestableDriver<EPaperSSD1683> display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 5);

  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  EXPECT_TRUE(display.run_update());
  display.set_frame({0x0F, 0xF0, 0x3C, 0x00});
  display.run_update_drawn_during_push(15, 1);
  bus.clear();

  EXPECT_TRUE(display.run_update_drawing_nothing()) << "draw made during the push was dropped";
  EXPECT_EQ(bus.data[0x24].size(), 4u) << "whole frame not sent";
}

/// An abandoned update leaves no hash to match, since its frame may never have been shown.
TEST(EPaperSkipUnchanged, AbandonedUpdateLeavesNoHash) {
  TestableDriver<EPaperSSD1683> display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 5);

  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  EXPECT_TRUE(display.run_update());
  EXPECT_TRUE(display.shown_hash_valid());

  display.set_frame({0x0F, 0xF0, 0x3C, 0x00});
  display.run_update_abandoned();
  EXPECT_FALSE(display.shown_hash_valid());

  display.set_frame({0x0F, 0xF0, 0x3C, 0x00});
  EXPECT_TRUE(display.run_update());
}

/// With every update a full one, nothing is ever skipped: a full refresh always redraws.
TEST(EPaperSkipUnchanged, NothingSkippedWithoutPartialUpdates) {
  TestableDriver<EPaperSSD1683> display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 1);

  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  EXPECT_TRUE(display.run_update());
  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  EXPECT_TRUE(display.run_update());
}

/// The real update path: auto clear then a writer lambda, as a YAML lambda runs. The clear makes
/// the drawn area the whole panel every time, so only the frame contents can tell updates apart.
TEST(EPaperSkipUnchanged, UpdatesThroughTheWriterAreSkippedWhenTheyDrawTheSameFrame) {
  TestableDriver<EPaperSSD1683> display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 5);
  int value = 1;
  display.set_writer([&](Display &it) { it.draw_pixel_at(value, 0, Color::BLACK); });

  EXPECT_TRUE(display.run_update(true));
  EXPECT_FALSE(display.run_update(true)) << "same frame was pushed";
  value = 2;
  EXPECT_TRUE(display.run_update(true)) << "changed frame was skipped";
  EXPECT_FALSE(display.run_update(true));
}

}  // namespace esphome::epaper_spi::testing
