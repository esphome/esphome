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
    ASSERT_EQ(this->init_sent_frame_(this->buffer_length_), full_update_every > 1);
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

  /// A whole update with the frame already drawn; false if it was skipped.
  bool run_update() {
    this->set_auto_clear(false);
    this->state_ = EPaperState::UPDATE;
    this->process_state_();
    const bool pushed = this->state_ == EPaperState::RESET;
    while (this->state_ != EPaperState::IDLE)
      this->process_state_();
    return pushed;
  }

  /// A whole update with auto clear and the writer, as the YAML lambda path runs; false if skipped.
  bool run_update_through_writer() {
    this->set_auto_clear(true);
    this->state_ = EPaperState::UPDATE;
    this->process_state_();
    const bool pushed = this->state_ == EPaperState::RESET;
    while (this->state_ != EPaperState::IDLE)
      this->process_state_();
    return pushed;
  }

  uint8_t update_count() const { return this->update_count_; }

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

/// With every update a full one there is no comparison frame and nothing is ever skipped.
TEST(EPaperSkipUnchanged, NothingSkippedWithoutPartialUpdates) {
  TestableDriver<EPaperSSD1683> display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 1);

  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  EXPECT_TRUE(display.run_update());
  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  EXPECT_TRUE(display.run_update());
}

}  // namespace esphome::epaper_spi::testing

namespace esphome::epaper_spi::testing {

/// The real update path: auto clear then a writer lambda, as a YAML lambda runs. The clear makes
/// the drawn area the whole panel every time, so only the frame contents can tell updates apart.
TEST(EPaperSkipUnchanged, UpdatesThroughTheWriterAreSkippedWhenTheyDrawTheSameFrame) {
  TestableDriver<EPaperSSD1683> display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 5);
  int value = 1;
  display.set_writer([&](Display &it) { it.draw_pixel_at(value, 0, Color::BLACK); });

  EXPECT_TRUE(display.run_update_through_writer());
  EXPECT_FALSE(display.run_update_through_writer()) << "same frame was pushed";
  value = 2;
  EXPECT_TRUE(display.run_update_through_writer()) << "changed frame was skipped";
  EXPECT_FALSE(display.run_update_through_writer());
}

}  // namespace esphome::epaper_spi::testing
