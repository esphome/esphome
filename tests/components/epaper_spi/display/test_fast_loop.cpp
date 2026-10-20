#include <gtest/gtest.h>

#include "../common.h"
#include "esphome/components/epaper_spi/epaper_spi_ssd1683.h"
#include "esphome/components/epaper_spi/epaper_spi_t133a01.h"

namespace esphome::epaper_spi::testing {
// Other test files define fixtures with the same names; keep these to this file
namespace {

constexpr uint8_t MALFORMED_INIT_SEQUENCE[] = {0x01};

class TestableSSD1683 : public EPaperSSD1683 {
 public:
  explicit TestableSSD1683(const uint8_t *init_sequence = nullptr, size_t init_sequence_length = 0)
      : EPaperSSD1683("test", 16, 2, init_sequence, init_sequence_length) {}
  // The request count is shared by the whole test binary, so never leave it raised
  ~TestableSSD1683() { this->high_frequency_loop_.stop(); }

  void install(spi::SPIDelegate *delegate) {
    this->delegate_ = delegate;
    this->set_dc_pin(&this->dc);
    this->set_busy_pin(&this->busy);
    this->set_auto_clear(false);
    ASSERT_TRUE(this->init_buffer_(this->buffer_length_));
  }

  void draw_whole_panel() {
    this->x_low_ = 0;
    this->y_low_ = 0;
    this->x_high_ = this->width_;
    this->y_high_ = this->height_;
  }

  void draw_nothing() {
    this->x_low_ = this->width_;
    this->y_low_ = this->height_;
    this->x_high_ = 0;
    this->y_high_ = 0;
  }

  /// Runs the loop for the given time, or until the display is idle.
  void loop_for(uint32_t ms) {
    const uint32_t until = millis() + ms;
    while (this->state_ != EPaperState::IDLE && millis() < until)
      this->loop();
  }

  EPaperState state() const { return this->state_; }

  RecordingPin dc, reset;
  SwitchableBusyPin busy;
};

/// With no long waits the loop runs continuously from the start of an update to its end, then goes back to its
/// normal pace.
TEST(EPaperFastLoop, LoopRunsContinuouslyForShortStepsAndStopsAfterward) {
  TestableSSD1683 display;
  RecordingDelegate bus(&display.dc);
  display.install(&bus);
  ASSERT_FALSE(HighFrequencyLoopRequester::is_high_frequency());

  display.draw_whole_panel();
  display.update();
  bool always_fast = true;
  const uint32_t deadline = millis() + 1000;
  while (display.state() != EPaperState::IDLE && millis() < deadline) {
    always_fast &= HighFrequencyLoopRequester::is_high_frequency();
    display.loop();
  }

  ASSERT_EQ(display.state(), EPaperState::IDLE);
  EXPECT_TRUE(always_fast);
  EXPECT_FALSE(HighFrequencyLoopRequester::is_high_frequency()) << "the loop was left running continuously";
}

/// An update with nothing drawn ends straight away and does not leave the loop running continuously.
TEST(EPaperFastLoop, UpdateWithNothingDrawnStopsTheFastLoop) {
  TestableSSD1683 display;
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  display.draw_nothing();
  display.update();
  display.loop();

  ASSERT_EQ(display.state(), EPaperState::IDLE);
  EXPECT_FALSE(HighFrequencyLoopRequester::is_high_frequency());
}

/// A display marked failed part way through an update is no longer looped, so it must stop the fast loop itself.
TEST(EPaperFastLoop, FailedDisplayStopsTheFastLoop) {
  TestableSSD1683 display(MALFORMED_INIT_SEQUENCE, sizeof(MALFORMED_INIT_SEQUENCE));
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  display.draw_whole_panel();
  display.update();
  const uint32_t deadline = millis() + 1000;
  while (!display.is_failed() && millis() < deadline)
    display.loop();

  ASSERT_TRUE(display.is_failed());
  EXPECT_FALSE(HighFrequencyLoopRequester::is_high_frequency());
}

/// A long busy wait, such as the waveform or a stuck busy pin, is polled at the normal pace. Once the pin clears,
/// the next steps run continuously again.
TEST(EPaperFastLoop, LongBusyWaitUsesTheNormalPace) {
  TestableSSD1683 display;
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  display.busy.busy = true;
  display.draw_whole_panel();
  display.update();
  display.loop_for(FAST_BUSY_POLL_MS + 20);

  ASSERT_NE(display.state(), EPaperState::IDLE);
  EXPECT_FALSE(HighFrequencyLoopRequester::is_high_frequency());

  display.busy.busy = false;
  display.loop();
  EXPECT_TRUE(HighFrequencyLoopRequester::is_high_frequency());
  display.loop_for(1000);
  EXPECT_EQ(display.state(), EPaperState::IDLE);
  EXPECT_FALSE(HighFrequencyLoopRequester::is_high_frequency());
}

/// A delay longer than a loop interval, such as a long reset pulse, is waited out at the normal pace.
TEST(EPaperFastLoop, LongDelayUsesTheNormalPace) {
  TestableSSD1683 display;
  RecordingDelegate bus(&display.dc);
  display.install(&bus);
  display.set_reset_pin(&display.reset);
  display.set_reset_duration(200);

  display.draw_whole_panel();
  display.update();
  display.loop_for(50);

  ASSERT_EQ(display.state(), EPaperState::RESET_END);
  EXPECT_FALSE(HighFrequencyLoopRequester::is_high_frequency());

  display.loop_for(1000);
  EXPECT_EQ(display.state(), EPaperState::IDLE);
  EXPECT_FALSE(HighFrequencyLoopRequester::is_high_frequency());
}

/// A step that takes several loop passes gets the fast loop back once a long busy wait before it has ended.
TEST(EPaperFastLoop, FastLoopResumesWhenALongBusyWaitEnds) {
  TestableSSD1683 display;
  // every row of data overruns a transfer slice, so the transfer takes a loop pass per row
  RecordingDelegate bus(&display.dc, MAX_TRANSFER_TIME + 1);
  display.install(&bus);

  display.draw_whole_panel();
  display.update();
  const uint32_t deadline = millis() + 1000;
  while (display.state() != EPaperState::TRANSFER_DATA && millis() < deadline)
    display.loop();
  ASSERT_EQ(display.state(), EPaperState::TRANSFER_DATA);
  display.busy.busy = true;
  display.loop_for(FAST_BUSY_POLL_MS + 20);
  ASSERT_FALSE(HighFrequencyLoopRequester::is_high_frequency());

  display.busy.busy = false;
  display.loop();
  ASSERT_EQ(display.state(), EPaperState::TRANSFER_DATA) << "the transfer should take more than one pass";
  EXPECT_TRUE(HighFrequencyLoopRequester::is_high_frequency());
}

class TestableT133A01 : public EPaperT133A01 {
 public:
  TestableT133A01() : EPaperT133A01("test", 8, 2, nullptr, 0, true) {}
  ~TestableT133A01() { this->high_frequency_loop_.stop(); }

  void install(spi::SPIDelegate *delegate) {
    this->delegate_ = delegate;
    this->set_dc_pin(&this->dc);
    this->set_primary_cs_pin(&this->cs);
    this->set_secondary_cs_pin(&this->cs1);
    this->set_busy_pin(&this->busy);
    ASSERT_TRUE(this->init_buffer_(this->buffer_length_));
  }

  void start_transfer() { this->set_state_(EPaperState::TRANSFER_DATA); }

  RecordingPin dc, cs, cs1;
  SwitchableBusyPin busy;
};

/// A busy wait that starts in the middle of a step is polled continuously for its first moments, however long
/// the step has already run.
TEST(EPaperFastLoop, WaitInTheMiddleOfAStepIsPolledFast) {
  TestableT133A01 display;
  TimedSPIDelegate delegate(MAX_TRANSFER_TIME + 5);
  display.install(&delegate);

  display.start_transfer();
  const uint32_t until = millis() + FAST_BUSY_POLL_MS + 20;
  while (millis() < until) {
  }
  // the transfer waits for the busy pin after its first command, then sends a row and yields
  display.loop();
  display.busy.busy = true;
  display.loop();

  EXPECT_TRUE(HighFrequencyLoopRequester::is_high_frequency());
}

}  // namespace
}  // namespace esphome::epaper_spi::testing
