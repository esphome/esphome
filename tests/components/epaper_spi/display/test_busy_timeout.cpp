#include <gtest/gtest.h>

#include "../common.h"
#include "esphome/components/epaper_spi/epaper_spi_ssd1683.h"

namespace esphome::epaper_spi::testing {

/// A busy pin stuck at "busy".
class StuckBusyPin : public RecordingPin {
 public:
  bool digital_read() override { return true; }
};

class TestableSSD1683 : public EPaperSSD1683 {
 public:
  TestableSSD1683() : EPaperSSD1683("test", 16, 2, nullptr, 0) {}

  void install(spi::SPIDelegate *delegate) {
    this->delegate_ = delegate;
    this->set_dc_pin(&this->dc);
    this->set_busy_pin(&this->busy);
    this->set_full_update_every(5);
    ASSERT_TRUE(this->init_buffer_(this->buffer_length_));
    this->busy_timeout_ = 50;
  }

  void start_update() {
    this->set_auto_clear(false);
    this->x_low_ = 0;
    this->y_low_ = 0;
    this->x_high_ = this->width_;
    this->y_high_ = this->height_;
    this->update();
  }

  EPaperState state() const { return this->state_; }
  uint8_t update_count() const { return this->update_count_; }
  void set_update_count(uint8_t count) { this->update_count_ = count; }

  RecordingPin dc;
  StuckBusyPin busy;
};

/// If the busy pin never clears, the update is abandoned instead of waiting for ever: the display goes back to
/// idle so later updates are accepted, the component is marked with a warning, and the next update is a full one.
TEST(EPaperBusyTimeout, StuckBusyPinAbandonsTheUpdate) {
  TestableSSD1683 display;
  RecordingDelegate bus(&display.dc);
  display.install(&bus);
  display.set_update_count(3);

  display.start_update();
  const uint32_t deadline = millis() + 1000;
  while (display.state() != EPaperState::IDLE && millis() < deadline)
    display.loop();

  EXPECT_EQ(display.state(), EPaperState::IDLE) << "the update was not abandoned";
  EXPECT_TRUE(display.status_has_warning());
  EXPECT_EQ(display.update_count(), 0) << "the next update is not a full one";

  display.start_update();
  EXPECT_NE(display.state(), EPaperState::IDLE) << "a later update was refused";
}

}  // namespace esphome::epaper_spi::testing
