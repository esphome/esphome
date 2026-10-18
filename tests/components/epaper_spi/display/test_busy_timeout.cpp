#include <gtest/gtest.h>

#include <algorithm>

#include "../common.h"
#include "esphome/components/epaper_spi/epaper_spi_ssd1683.h"
#include "esphome/components/epaper_spi/epaper_spi_t133a01.h"

namespace esphome::epaper_spi::testing {
// Other test files define fixtures with the same names; keep these to this file
namespace {

template<typename T> void loop_until_idle(T &display) {
  const uint32_t deadline = millis() + 1000;
  while (display.state() != EPaperState::IDLE && millis() < deadline)
    display.loop();
}

class TestableSSD1683 : public EPaperSSD1683 {
 public:
  TestableSSD1683() : EPaperSSD1683("test", 16, 2, nullptr, 0) {}

  void install(spi::SPIDelegate *delegate) {
    this->delegate_ = delegate;
    this->set_dc_pin(&this->dc);
    this->set_busy_pin(&this->busy);
    this->set_full_update_every(5);
    this->set_auto_clear(false);
    ASSERT_TRUE(this->init_buffer_(this->buffer_length_));
    this->busy_timeout_seconds_ = 0;  // abandon as soon as the stuck pin is seen, so the test does not wait
  }

  void draw_whole_panel() {
    this->x_low_ = 0;
    this->y_low_ = 0;
    this->x_high_ = this->width_;
    this->y_high_ = this->height_;
  }

  EPaperState state() const { return this->state_; }
  uint8_t update_count() const { return this->update_count_; }
  void set_update_count(uint8_t count) { this->update_count_ = count; }

  RecordingPin dc;
  SwitchableBusyPin busy;
};

/// If the busy pin never clears, the update is abandoned instead of waiting for ever: the display goes back to
/// idle with a warning. Once the pin works again the next update is accepted and is a full one, even though nothing
/// was drawn since, and completing it clears the warning.
TEST(EPaperBusyTimeout, StuckBusyPinAbandonsTheUpdateAndRecovers) {
  TestableSSD1683 display;
  RecordingDelegate bus(&display.dc);
  display.install(&bus);
  display.set_update_count(3);

  // Stick the pin after the data has been sent, when the dirty area has already been emptied
  display.draw_whole_panel();
  display.update();
  const uint32_t deadline = millis() + 1000;
  while (display.state() <= EPaperState::TRANSFER_DATA && millis() < deadline)
    display.loop();
  ASSERT_EQ(display.state(), EPaperState::POWER_ON);
  display.busy.busy = true;
  loop_until_idle(display);

  EXPECT_EQ(display.state(), EPaperState::IDLE) << "the update was not abandoned";
  EXPECT_TRUE(display.status_has_warning());

  display.busy.busy = false;
  display.update();
  EXPECT_NE(display.state(), EPaperState::IDLE) << "a later update was refused";
  loop_until_idle(display);

  EXPECT_EQ(display.state(), EPaperState::IDLE) << "the later update did not complete";
  EXPECT_EQ(display.update_count(), 1) << "the later update was not a full one";
  EXPECT_FALSE(display.status_has_warning());
}

class TestableT133A01 : public EPaperT133A01 {
 public:
  TestableT133A01() : EPaperT133A01("test", 8, 2, nullptr, 0, true) {}

  void install(spi::SPIDelegate *delegate) {
    this->delegate_ = delegate;
    this->set_dc_pin(&this->dc);
    this->set_cs_pins(&this->cs, &this->cs1);
    this->set_busy_pin(&this->busy);
    ASSERT_TRUE(this->init_buffer_(this->buffer_length_));
    this->busy_timeout_seconds_ = 0;  // abandon as soon as the stuck pin is seen, so the test does not wait
  }

  void start_transfer() { this->set_state_(EPaperState::TRANSFER_DATA); }
  EPaperState state() const { return this->state_; }

  using EPaperT133A01::transfer_sub_;
  using EPaperT133A01::TRF_PARTIAL_WAIT_M;
  using EPaperT133A01::waiting_for_idle_;

  RecordingPin dc, cs, cs1;
  SwitchableBusyPin busy;
};

/// The T133A01 waits for the busy pin with a data stream open. Abandoning the update there must close the SPI
/// transaction and release both chip selects, or the next command would block.
TEST(EPaperBusyTimeout, AbandonedT133A01TransferReleasesTheBus) {
  TestableT133A01 display;
  TimedSPIDelegate delegate(MAX_TRANSFER_TIME + 5);
  display.install(&delegate);

  display.start_transfer();
  display.loop();
  ASSERT_EQ(display.state(), EPaperState::TRANSFER_DATA);
  ASSERT_FALSE(display.cs.level) << "the transfer should have yielded with its data stream open";

  display.busy.busy = true;
  loop_until_idle(display);

  ASSERT_EQ(display.state(), EPaperState::IDLE) << "the update was not abandoned";
  EXPECT_EQ(delegate.begin_count, delegate.end_count) << "SPI transaction left open";
  EXPECT_TRUE(display.cs.level) << "CS left asserted";
  EXPECT_TRUE(display.cs1.level) << "CS1 left asserted";
}

/// A partial update waits for the busy pin between its two halves. That wait must use the same timeout as every other
/// wait, so a stuck pin abandons the update instead of leaving the display busy for ever and refusing every later
/// update.
TEST(EPaperBusyTimeout, StuckBusyPinDuringPartialUpdateAbandonsIt) {
  TestableT133A01 display;
  TimedSPIDelegate delegate(0);
  display.install(&delegate);

  // The first half of a partial update has been sent and the transfer is waiting for the panel. The wait on
  // entering TRANSFER_DATA is long over, so the stuck pin is first seen by the partial path's own wait.
  display.start_transfer();
  display.waiting_for_idle_ = false;
  display.transfer_sub_ = TestableT133A01::TRF_PARTIAL_WAIT_M;
  display.busy.busy = true;

  loop_until_idle(display);

  EXPECT_EQ(display.state(), EPaperState::IDLE) << "a stuck busy pin during a partial update was never given up on";
}

/// Abandoning an update powers the panel off, so it is not left with its high-voltage drivers enabled.
TEST(EPaperBusyTimeout, AbandonedUpdatePowersThePanelOff) {
  TestableT133A01 display;
  RecordingDelegate bus(&display.dc, MAX_TRANSFER_TIME + 5);  // burn time so the transfer yields mid-stream
  display.install(&bus);

  display.start_transfer();
  display.loop();
  ASSERT_EQ(display.state(), EPaperState::TRANSFER_DATA);

  display.busy.busy = true;
  loop_until_idle(display);
  ASSERT_EQ(display.state(), EPaperState::IDLE) << "the update was not abandoned";

  EXPECT_NE(std::find(bus.commands.begin(), bus.commands.end(), 0x02), bus.commands.end())
      << "the panel was not powered off (POF) when the update was abandoned";
}

}  // namespace
}  // namespace esphome::epaper_spi::testing
