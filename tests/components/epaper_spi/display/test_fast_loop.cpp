#include <gtest/gtest.h>

#include <vector>

#include "../common.h"
#include "esphome/components/epaper_spi/epaper_spi_ssd1683.h"

namespace esphome::epaper_spi::testing {

class TestableSSD1683 : public EPaperSSD1683 {
 public:
  TestableSSD1683() : EPaperSSD1683("test", 16, 2, nullptr, 0) {}

  void install(spi::SPIDelegate *delegate) {
    this->delegate_ = delegate;
    this->set_dc_pin(&this->dc);
    ASSERT_TRUE(this->init_buffer_(this->buffer_length_));
  }

  /// Runs one update, recording whether the loop was asked to run continuously in each state.
  std::vector<std::pair<EPaperState, bool>> run_update() {
    std::vector<std::pair<EPaperState, bool>> seen;
    this->set_auto_clear(false);
    this->x_low_ = 0;
    this->y_low_ = 0;
    this->x_high_ = this->width_;
    this->y_high_ = this->height_;
    this->update();
    seen.emplace_back(this->state_, HighFrequencyLoopRequester::is_high_frequency());
    while (this->state_ != EPaperState::IDLE) {
      this->process_state_();
      seen.emplace_back(this->state_, HighFrequencyLoopRequester::is_high_frequency());
    }
    return seen;
  }

  RecordingPin dc;
};

/// The loop runs continuously for the short steps of an update, not while the panel's waveform runs, and
/// goes back to its normal pace once the update is done.
TEST(EPaperFastLoop, LoopRunsContinuouslyExceptDuringTheWaveform) {
  TestableSSD1683 display;
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  ASSERT_FALSE(HighFrequencyLoopRequester::is_high_frequency());
  const auto seen = display.run_update();
  for (const auto &[state, high_frequency] : seen) {
    const bool expected = state != EPaperState::IDLE && state != EPaperState::POWER_OFF;
    EXPECT_EQ(high_frequency, expected) << "in state " << static_cast<int>(state);
  }
  EXPECT_FALSE(HighFrequencyLoopRequester::is_high_frequency()) << "the loop was left running continuously";
}

}  // namespace esphome::epaper_spi::testing
