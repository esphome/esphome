#include <gtest/gtest.h>

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
  Testable(uint16_t width, uint16_t height) : Driver("test", width, height, nullptr, 0) {}

  void install(spi::SPIDelegate *delegate, uint8_t full_update_every = 2) {
    this->delegate_ = delegate;
    this->set_dc_pin(&this->dc);
    this->set_reset_pin(&this->reset_pin);
    ASSERT_TRUE(this->init_buffer_(this->buffer_length_));
    this->set_full_update_every(full_update_every);
    this->init_sent_frame_(this->sent_frame_length());
  }

  void set_byte(size_t index, uint8_t value) { this->buffer_[index] = value; }
  size_t buffer_size() const { return this->buffer_length_; }

  void run_push() {
    while (!this->transfer_data()) {
    }
    this->sent_valid_ = this->sent_.is_valid() && this->sent_count_ == this->sent_.size();
    this->sent_count_ = 0;
  }
  void set_partial(bool partial) { this->partial_ = partial; }

  void run_initialise() {
    while (!this->initialise(this->partial_)) {
    }
  }

  using Driver::full_waveform;
  using Driver::whole_degrees_;

  RecordingPin dc;
  RecordingPin reset_pin;
};

using Mono = Testable<EPaperSSD2677>;
using Gray = Testable<EPaperSSD2677Gray4>;

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
