#include <gtest/gtest.h>

#include <algorithm>

#include "../common.h"
#include "esphome/components/epaper_spi/epaper_spi_ssd1683.h"
#include "esphome/components/epaper_spi/epaper_spi_t133a01.h"
#include "esphome/components/epaper_spi/epaper_waveshare_3p97_g.h"

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
  void transition(EPaperState state) { this->set_state_(state); }
  void skip_next_idle_wait() { this->skip_next_idle_wait_(); }
  bool waiting_for_idle() const { return this->waiting_for_idle_; }

  RecordingPin dc;
  SwitchableBusyPin busy;
};

TEST(EPaperIdleWaitPolicy, SkipIsConsumedByOneTransition) {
  TestableSSD1683 display;
  display.skip_next_idle_wait();
  display.transition(EPaperState::INITIALISE);
  EXPECT_FALSE(display.waiting_for_idle());
  display.transition(EPaperState::TRANSFER_DATA);
  EXPECT_TRUE(display.waiting_for_idle());
  display.transition(EPaperState::IDLE);
}

TEST(EPaperIdleWaitPolicy, NonWaitingTransitionsConsumeThePolicy) {
  TestableSSD1683 display;
  for (const auto state : {EPaperState::RESET, EPaperState::IDLE}) {
    display.skip_next_idle_wait();
    display.transition(state);
    EXPECT_FALSE(display.waiting_for_idle());
    display.transition(EPaperState::INITIALISE);
    EXPECT_TRUE(display.waiting_for_idle());
  }
  display.transition(EPaperState::IDLE);
}

TEST(EPaperIdleWaitPolicy, NextPolicyDoesNotChangeTheCurrentWait) {
  TestableSSD1683 display;
  display.transition(EPaperState::INITIALISE);
  ASSERT_TRUE(display.waiting_for_idle());
  display.skip_next_idle_wait();
  EXPECT_TRUE(display.waiting_for_idle());
  display.transition(EPaperState::TRANSFER_DATA);
  EXPECT_FALSE(display.waiting_for_idle());
  display.transition(EPaperState::POWER_ON);
  EXPECT_TRUE(display.waiting_for_idle());
  display.transition(EPaperState::IDLE);
}

class WaveshareAccess : public EPaperBase {
 public:
  static constexpr auto STATE = &WaveshareAccess::state_;
  static constexpr auto WAITING = &WaveshareAccess::waiting_for_idle_;
  static constexpr auto DELAY = &WaveshareAccess::delay_until_;
  static constexpr auto WAIT_START = &WaveshareAccess::waiting_for_idle_start_;
  static constexpr auto DELEGATE = &WaveshareAccess::delegate_;
  static constexpr auto TRANSITION = &WaveshareAccess::set_state_;
  static constexpr auto ABANDON = &WaveshareAccess::abandon_update_;
};

constexpr uint8_t WAVESHARE_INIT_SEQUENCE[] = {0xA5, 1, 0x00};

class WaveshareBusyFlow : public ::testing::Test {
 protected:
  void SetUp() override {
    this->panel_.*WaveshareAccess::DELEGATE = &this->bus_;
    this->panel_.set_dc_pin(&this->dc_);
    this->panel_.set_busy_pin(&this->busy_);
  }

  void TearDown() override { this->transition_(EPaperState::IDLE); }

  void transition_(EPaperState state) { (this->panel_.*WaveshareAccess::TRANSITION)(state, 0); }
  EPaperState state_() const { return this->panel_.*WaveshareAccess::STATE; }
  bool waiting_() const { return this->panel_.*WaveshareAccess::WAITING; }
  void step_() {
    const auto now = millis();
    this->panel_.*WaveshareAccess::DELAY = now;
    this->panel_.*WaveshareAccess::WAIT_START = now;
    this->panel_.loop();
  }

  EPaperWaveshare3P97InG panel_{"test", 800, 480, WAVESHARE_INIT_SEQUENCE, sizeof(WAVESHARE_INIT_SEQUENCE)};
  RecordingPin dc_, reset_;
  SwitchableBusyPin busy_;
  RecordingDelegate bus_{&this->dc_};
};

TEST_F(WaveshareBusyFlow, InitialisationStartsBeforeBusyAndWaitsAfterEachCommand) {
  this->busy_.busy = true;
  this->transition_(EPaperState::RESET);
  this->step_();
  ASSERT_EQ(this->state_(), EPaperState::INITIALISE);
  ASSERT_FALSE(this->waiting_());
  this->step_();
  ASSERT_EQ(this->bus_.commands, std::vector<uint8_t>({0xA5}));
  EXPECT_TRUE(this->waiting_());

  this->step_();
  EXPECT_EQ(this->bus_.commands.size(), 1);
  this->busy_.busy = false;
  this->step_();
  ASSERT_EQ(this->bus_.commands, std::vector<uint8_t>({0xA5, 0x04}));
  EXPECT_TRUE(this->waiting_());
  this->busy_.busy = true;
  this->step_();
  EXPECT_EQ(this->state_(), EPaperState::INITIALISE);
  this->busy_.busy = false;
  this->step_();
  EXPECT_EQ(this->state_(), EPaperState::TRANSFER_DATA);
}

TEST_F(WaveshareBusyFlow, HardwareResetCompletionSkipsTheInitialBusyWait) {
  this->panel_.set_reset_pin(&this->reset_);
  this->busy_.busy = true;
  this->transition_(EPaperState::RESET);
  this->step_();
  ASSERT_EQ(this->state_(), EPaperState::RESET_END);
  this->step_();
  ASSERT_EQ(this->state_(), EPaperState::RESET_END);
  this->step_();
  ASSERT_EQ(this->state_(), EPaperState::INITIALISE);
  EXPECT_FALSE(this->waiting_());
  this->step_();
  EXPECT_EQ(this->bus_.commands, std::vector<uint8_t>({0xA5}));
}

TEST_F(WaveshareBusyFlow, RefreshWaitsButPowerOffAndSleepAreConsecutive) {
  this->transition_(EPaperState::REFRESH_SCREEN);
  this->step_();
  ASSERT_EQ(this->bus_.commands, std::vector<uint8_t>({0x12}));
  this->busy_.busy = true;
  this->step_();
  EXPECT_EQ(this->state_(), EPaperState::POWER_OFF);
  EXPECT_EQ(this->bus_.commands.size(), 1);
  this->busy_.busy = false;
  this->step_();
  ASSERT_EQ(this->state_(), EPaperState::DEEP_SLEEP);
  ASSERT_EQ(this->bus_.commands, std::vector<uint8_t>({0x12, 0x02}));
  EXPECT_FALSE(this->waiting_());
  this->busy_.busy = true;
  this->step_();
  EXPECT_EQ(this->state_(), EPaperState::IDLE);
  EXPECT_EQ(this->bus_.commands, std::vector<uint8_t>({0x12, 0x02, 0x07}));
  this->transition_(EPaperState::REFRESH_SCREEN);
  EXPECT_TRUE(this->waiting_());
}

TEST_F(WaveshareBusyFlow, AbandoningAnUpdateConsumesThePowerOffPolicy) {
  (this->panel_.*WaveshareAccess::ABANDON)();
  ASSERT_EQ(this->state_(), EPaperState::IDLE);
  EXPECT_EQ(this->bus_.commands, std::vector<uint8_t>({0x02}));
  this->transition_(EPaperState::INITIALISE);
  EXPECT_TRUE(this->waiting_());
}

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
  TestableT133A01() : EPaperT133A01("test", 8, 2, nullptr, 0) {}

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
