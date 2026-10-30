#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "../common.h"
#include "esphome/components/epaper_spi/epaper_spi_ssd2677.h"

namespace esphome::epaper_spi::testing {

using Bytes = std::vector<uint8_t>;

static constexpr uint8_t CMD_DTM1 = 0x10;
static constexpr uint8_t CMD_FORCE_TEMPERATURE = 0xE6;

template<typename Driver> class Testable : public Driver {
 public:
  Testable(uint16_t width, uint16_t height, const uint8_t *init_sequence = nullptr, size_t init_sequence_length = 0)
      : Driver("test", width, height, init_sequence, init_sequence_length) {}

  void install(spi::SPIDelegate *delegate, uint8_t full_update_every = 2) {
    this->delegate_ = delegate;
    this->set_dc_pin(&this->dc);
    this->set_reset_pin(&this->reset_pin);
    ASSERT_TRUE(this->init_buffer_(this->buffer_length_));
    this->set_full_update_every(full_update_every);
    this->init_comparison_frame_();
  }

  void set_byte(size_t index, uint8_t value) { this->buffer_[index] = value; }
  size_t buffer_size() const { return this->buffer_length_; }

  /// One push through the base class's state machine, which decides when the previous frame is whole.
  void run_push() {
    this->sent_count_ = 0;
    this->state_ = EPaperState::TRANSFER_DATA;
    while (this->state_ == EPaperState::TRANSFER_DATA)
      this->process_state_();
  }

  /// Both calls count how often the transfer gave the main loop a turn.
  int run_transfer_counting_calls() {
    int calls = 1;
    while (!this->transfer_data())
      calls++;
    return calls;
  }
  void set_partial(bool partial) { this->partial_ = partial; }

  void run_initialise() {
    while (!this->initialise(this->partial_)) {
    }
  }

  /// Give up during the wait after the init sequence, as a busy timeout does, then reset for the next update.
  void abandon_during_initialise_then_reset() {
    ASSERT_FALSE(this->initialise(false));
    this->abandon_update_();
    ASSERT_TRUE(this->reset());
  }

  using Driver::full_waveform;
  using Driver::whole_degrees_;

  RecordingPin dc;
  RecordingPin reset_pin;
};

using Mono = Testable<EPaperSSD2677>;
using Gray = Testable<EPaperSSD2677Gray4>;

/// The reset before an update loses what the init sequence set, whatever happened to the update before.
TEST(EPaperSSD2677, SendsTheInitSequenceAgainAfterAnAbandonedUpdate) {
  static constexpr uint8_t INIT_SEQUENCE[] = {0x50, 1, 0x77};
  Mono display(8, 1, INIT_SEQUENCE, sizeof(INIT_SEQUENCE));
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  display.abandon_during_initialise_then_reset();
  bus.clear();

  display.run_initialise();
  EXPECT_EQ(bus.data[0x50], (Bytes{0x77}));
  EXPECT_EQ(bus.data[CMD_FORCE_TEMPERATURE], (Bytes{0xEE}));
}

/// A slow bus makes the transfer give the main loop a turn; it continues where it stopped.
TEST(EPaperSSD2677, ResumesATransferThatYielded) {
  Mono display(8, 4);
  RecordingDelegate bus(&display.dc, MAX_TRANSFER_TIME + 1);
  display.install(&bus);

  for (size_t y = 0; y != 4; y++)
    display.set_byte(y, y == 0 ? 0x80 : 0x00);

  EXPECT_GT(display.run_transfer_counting_calls(), 1);
  EXPECT_EQ(bus.data[CMD_DTM1], (Bytes{0xC0, 0, 0, 0, 0, 0, 0, 0}));
  EXPECT_EQ(std::count(bus.commands.begin(), bus.commands.end(), CMD_DTM1), 1);
}

/// With full_update_every at 1 there is no previous frame to keep, and the four-level stream is still sent.
TEST(EPaperSSD2677Gray4, FullRefreshWithoutPartialUpdates) {
  Gray display(8, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 1);

  display.set_byte(0, 0x1B);
  display.set_byte(1, 0xE4);
  display.run_push();

  EXPECT_EQ(bus.data[CMD_DTM1], (Bytes{0x1B, 0xE4}));
}

/// A full refresh sends each 1 bit pixel as a gray level: 0 black, 3 white.
TEST(EPaperSSD2677, FullRefreshSendsEachPixelAsAGrayLevel) {
  Mono display(8, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  display.set_byte(0, 0x81);
  display.run_push();

  EXPECT_EQ(bus.data[CMD_DTM1], (Bytes{0xC0, 0x03}));
}

/// A partial refresh pairs each pixel as (old << 1 | new), the old value from the frame before.
TEST(EPaperSSD2677, PartialRefreshPairsOldAndNewPixels) {
  Mono display(8, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  display.set_byte(0, 0xF0);
  display.run_push();
  bus.clear();

  display.set_partial(true);
  display.set_byte(0, 0xCC);
  display.run_push();

  // old 1111 0000, new 1100 1100: pairs 11 11 10 10, 01 01 00 00
  EXPECT_EQ(bus.data[CMD_DTM1], (Bytes{0xFA, 0x50}));
}

/// Without a whole previous frame there is nothing to compare with, so the push stays a full one.
TEST(EPaperSSD2677, PartialWithoutPreviousFrameSendsAFullRefresh) {
  Mono display(8, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  display.set_partial(true);
  display.set_byte(0, 0x80);
  display.run_push();

  EXPECT_EQ(bus.data[CMD_DTM1], (Bytes{0xC0, 0x00}));
}

/// The four-level buffer already holds the stream of a full refresh.
TEST(EPaperSSD2677Gray4, FullRefreshSendsTheBufferAsItIs) {
  Gray display(8, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  display.set_byte(0, 0x1B);
  display.set_byte(1, 0xE4);
  display.run_push();

  EXPECT_EQ(bus.data[CMD_DTM1], (Bytes{0x1B, 0xE4}));
}

/// A partial refresh reduces each level to its high bit, against the high bits sent last time.
TEST(EPaperSSD2677Gray4, PartialRefreshComparesHighBits) {
  Gray display(8, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  // levels 0 1 2 3 0 1 2 3: high bits 0011 0011
  display.set_byte(0, 0x1B);
  display.set_byte(1, 0x1B);
  display.run_push();
  bus.clear();

  // levels 3 3 3 3 0 0 0 0: high bits 1111 0000
  display.set_partial(true);
  display.set_byte(0, 0xFF);
  display.set_byte(1, 0x00);
  display.run_push();

  // old 0011 0011, new 1111 0000: pairs 01 01 11 11, 00 00 10 10
  EXPECT_EQ(bus.data[CMD_DTM1], (Bytes{0x5F, 0x0A}));
}

TEST(EPaperSSD2677, FullWaveformFollowsTemperature) {
  Mono display(8, 1);
  EXPECT_EQ(display.full_waveform(-5), 0xE8);
  EXPECT_EQ(display.full_waveform(10), 0xE8);
  EXPECT_EQ(display.full_waveform(11), 0xEB);
  EXPECT_EQ(display.full_waveform(20), 0xEB);
  EXPECT_EQ(display.full_waveform(21), 0xEE);
  EXPECT_EQ(display.full_waveform(30), 0xEE);
  EXPECT_EQ(display.full_waveform(31), 0xF1);
}

TEST(EPaperSSD2677Gray4, FullWaveformFollowsTemperature) {
  Gray display(8, 1);
  EXPECT_EQ(display.full_waveform(-5), 0xF1);
  EXPECT_EQ(display.full_waveform(15), 0xF1);
  EXPECT_EQ(display.full_waveform(16), 0xFA);
}

/// With no temperature reading the 25 C waveforms are used.
TEST(EPaperSSD2677, ForcesTheWaveformForTheDefaultTemperature) {
  Mono display(8, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  display.run_initialise();
  EXPECT_EQ(bus.data[CMD_FORCE_TEMPERATURE], (Bytes{0xEE}));
}

TEST(EPaperSSD2677Gray4, ForcesTheWaveformForTheDefaultTemperature) {
  Gray display(8, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  display.run_initialise();
  EXPECT_EQ(bus.data[CMD_FORCE_TEMPERATURE], (Bytes{0xFA}));
}

/// A partial update has its own waveform, also for the four-level model.
TEST(EPaperSSD2677Gray4, PartialUsesThePartialWaveform) {
  Gray display(8, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  display.run_push();
  bus.clear();
  display.set_partial(true);
  display.run_initialise();
  EXPECT_EQ(bus.data[CMD_FORCE_TEMPERATURE], (Bytes{0x19}));
}

TEST(EPaperSSD2677, ReadingBecomesWholeDegreesWithDefaultForNoReading) {
  EXPECT_EQ(Mono::whole_degrees_(NAN), 25);
  EXPECT_EQ(Mono::whole_degrees_(7.6f), 8);
  EXPECT_EQ(Mono::whole_degrees_(-3.4f), -3);
}

}  // namespace esphome::epaper_spi::testing
