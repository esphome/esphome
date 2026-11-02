#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "../common.h"
#include "esphome/components/epaper_spi/epaper_spi_uc8151.h"

namespace esphome::epaper_spi::testing {

/// A reset pin that counts how often it is driven low.
class CountingPin : public RecordingPin {
 public:
  void digital_write(bool value) override {
    if (!value)
      this->pulses++;
    RecordingPin::digital_write(value);
  }
  int pulses{0};
};

static constexpr uint8_t INIT_SEQUENCE[] = {0x00, 1, 0x1F, 0x50, 1, 0x97};

/// A 16x100 panel: the buffer is 200 bytes, more than one transfer chunk.
class TestableUC8151 : public EPaperUC8151 {
 public:
  TestableUC8151() : EPaperUC8151("test", 16, 100, INIT_SEQUENCE, sizeof(INIT_SEQUENCE)) {}

  void install(spi::SPIDelegate *delegate, uint8_t full_update_every) {
    this->delegate_ = delegate;
    this->set_dc_pin(&this->dc);
    this->set_reset_pin(&this->reset_pin);
    ASSERT_TRUE(this->init_buffer_(this->buffer_length_));
    this->set_full_update_every(full_update_every);
  }

  void set_frame(const std::vector<uint8_t> &bytes) {
    for (size_t i = 0; i != bytes.size(); i++)
      this->buffer_[i] = bytes[i];
    this->x_low_ = 0;
    this->y_low_ = 0;
    this->x_high_ = this->width_;
    this->y_high_ = this->height_;
  }

  /// A whole update with the frame already drawn, run through loop() as the main loop would.
  void run_update() {
    this->set_auto_clear(false);
    this->state_ = EPaperState::UPDATE;
    const uint32_t deadline = millis() + 2000;
    while (this->state_ != EPaperState::IDLE) {
      ASSERT_LT(millis(), deadline) << "update did not finish";
      this->loop();
    }
  }

  bool transfer() { return this->transfer_data(); }
  bool refresh_done(bool partial) {
    this->partial_ = partial;
    return this->after_refresh();
  }
  void set_window(uint16_t x_start, uint16_t x_end, uint16_t y_start, uint16_t y_end) {
    this->set_partial_window_(x_start, x_end, y_start, y_end);
  }
  void begin_partial() { this->partial_ = true; }

  RecordingPin dc;
  CountingPin reset_pin;
};

using Bytes = std::vector<uint8_t>;

static size_t count(const Bytes &commands, uint8_t command) {
  return std::count(commands.begin(), commands.end(), command);
}

static size_t position(const Bytes &commands, uint8_t command) {
  return std::find(commands.begin(), commands.end(), command) - commands.begin();
}

static Bytes make_frame(uint8_t seed) {
  Bytes frame(200);
  for (size_t i = 0; i != frame.size(); i++)
    frame[i] = static_cast<uint8_t>(seed + i * 7);
  return frame;
}

static const Bytes FRAME_1 = make_frame(1);
static const Bytes FRAME_2 = make_frame(2);
static const Bytes FRAME_3 = make_frame(3);

/// Without partial updates the update is the init sequence, power on, the raw buffer in DTM2, a refresh and
/// the panel's deep sleep; DTM1 is never written.
TEST(EPaperUC8151, EveryUpdateFullUsesTheOtpWaveformAndSleeps) {
  TestableUC8151 display;
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 1);

  display.set_frame(FRAME_1);
  display.run_update();

  EXPECT_GT(display.reset_pin.pulses, 0);
  EXPECT_TRUE(display.reset_pin.level) << "reset pin left low";
  EXPECT_EQ(bus.data[0x00], (Bytes{0x1F}));
  EXPECT_EQ(bus.data[0x50], (Bytes{0x97}));
  EXPECT_EQ(bus.data[0x13], FRAME_1);
  EXPECT_EQ(count(bus.commands, 0x10), 0u) << "DTM1 was written without partial updates";
  EXPECT_EQ(count(bus.commands, 0x91), 0u);
  EXPECT_EQ(count(bus.commands, 0x90), 0u);
  EXPECT_EQ(count(bus.commands, 0x92), 0u);
  EXPECT_EQ(count(bus.commands, 0x01), 0u) << "the partial settings were sent";
  EXPECT_EQ(count(bus.commands, 0x20), 0u);
  EXPECT_EQ(count(bus.commands, 0x04), 1u);
  EXPECT_LT(position(bus.commands, 0x04), position(bus.commands, 0x13)) << "data sent before power on";
  EXPECT_LT(position(bus.commands, 0x13), position(bus.commands, 0x12)) << "refresh before the data";
  EXPECT_EQ(count(bus.commands, 0x02), 1u);
  EXPECT_EQ(bus.data[0x07], (Bytes{0xA5}));
}

/// The first update of a run with partial updates is a full one: the image goes to DTM2, then to DTM1 once the
/// refresh has run, and the panel stays in standby.
TEST(EPaperUC8151, FirstFullUpdateWritesDtm1AfterTheRefresh) {
  TestableUC8151 display;
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 5);

  display.set_frame(FRAME_1);
  display.run_update();

  EXPECT_GT(display.reset_pin.pulses, 0);
  EXPECT_EQ(bus.data[0x13], FRAME_1);
  EXPECT_EQ(bus.data[0x10], FRAME_1);
  EXPECT_LT(position(bus.commands, 0x12), position(bus.commands, 0x10)) << "DTM1 written before the refresh";
  EXPECT_EQ(count(bus.commands, 0x92), 0u) << "a full update left partial mode";
  EXPECT_EQ(count(bus.commands, 0x91), 0u);
  EXPECT_EQ(count(bus.commands, 0x07), 0u) << "the panel was put into deep sleep";
  EXPECT_EQ(bus.commands.back(), 0x02) << "the panel was not powered off after DTM1 was written";
}

/// A partial update keeps the controller's state: no reset and no model init sequence. It loads the partial
/// settings and waveform, covers the whole screen with the window, writes the buffer to DTM2, refreshes, leaves
/// partial mode and then rewrites DTM1.
TEST(EPaperUC8151, PartialUpdateLoadsTheWaveformAndTheWindow) {
  TestableUC8151 display;
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 5);

  display.set_frame(FRAME_1);
  display.run_update();
  bus.clear();
  const int pulses = display.reset_pin.pulses;

  display.set_frame(FRAME_2);
  display.run_update();

  EXPECT_EQ(display.reset_pin.pulses, pulses) << "the partial update reset the panel";
  EXPECT_EQ(bus.data[0x01], (Bytes{0x03, 0x00, 0x2B, 0x2B, 0x03}));
  EXPECT_EQ(bus.data[0x06], (Bytes{0x17, 0x17, 0x17}));
  EXPECT_EQ(bus.data[0x00], (Bytes{0xBF, 0x0D})) << "the model init sequence was sent again";
  EXPECT_EQ(bus.data[0x30], (Bytes{0x3A}));
  EXPECT_EQ(bus.data[0x82], (Bytes{0x08}));
  EXPECT_EQ(bus.data[0x50], (Bytes{0x17}));
  ASSERT_EQ(bus.data[0x20].size(), 44u);
  EXPECT_EQ(bus.data[0x20][0], 0x00);
  for (const uint8_t command : {0x21, 0x22, 0x23, 0x24})
    ASSERT_EQ(bus.data[command].size(), 42u) << "LUT " << static_cast<int>(command);
  EXPECT_EQ(bus.data[0x21][0], 0x00);
  EXPECT_EQ(bus.data[0x22][0], 0x80);
  EXPECT_EQ(bus.data[0x23][0], 0x40);
  EXPECT_EQ(bus.data[0x24][0], 0x00);

  EXPECT_EQ(bus.data[0x90], (Bytes{0x00, 0x0F, 0x00, 0x00, 0x00, 0x63, 0x01}));
  EXPECT_EQ(bus.data[0x13], FRAME_2) << "DTM2 does not hold the unmodified buffer";
  EXPECT_EQ(bus.data[0x10], FRAME_2) << "DTM1 was not rewritten after the refresh";

  EXPECT_LT(position(bus.commands, 0x04), position(bus.commands, 0x91));
  EXPECT_LT(position(bus.commands, 0x91), position(bus.commands, 0x90));
  EXPECT_LT(position(bus.commands, 0x90), position(bus.commands, 0x13));
  EXPECT_LT(position(bus.commands, 0x13), position(bus.commands, 0x12));
  EXPECT_LT(position(bus.commands, 0x12), position(bus.commands, 0x92));
  EXPECT_LT(position(bus.commands, 0x92), position(bus.commands, 0x10));
  EXPECT_EQ(count(bus.commands, 0x91), 1u);
  EXPECT_EQ(count(bus.commands, 0x92), 1u);
  EXPECT_EQ(count(bus.commands, 0x07), 0u) << "the panel was put into deep sleep";
  EXPECT_EQ(bus.commands.back(), 0x02);
}

/// The full update that comes round after partial ones resets the panel and sends the model init sequence again.
TEST(EPaperUC8151, FullUpdateAfterPartialOnesStartsAfresh) {
  TestableUC8151 display;
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 2);

  display.set_frame(FRAME_1);
  display.run_update();
  display.set_frame(FRAME_2);
  display.run_update();
  bus.clear();
  const int pulses = display.reset_pin.pulses;

  display.set_frame(FRAME_3);
  display.run_update();

  EXPECT_EQ(display.reset_pin.pulses, pulses + 1);
  EXPECT_EQ(bus.data[0x00], (Bytes{0x1F}));
  EXPECT_EQ(count(bus.commands, 0x91), 0u);
  EXPECT_EQ(count(bus.commands, 0x92), 0u);
  EXPECT_EQ(bus.data[0x13], FRAME_3);
  EXPECT_EQ(bus.data[0x10], FRAME_3);
}

/// The UC8151 window has one byte per horizontal bound, and x is rounded out to whole bytes.
TEST(EPaperUC8151, PartialWindowRoundsXToWholeBytes) {
  TestableUC8151 display;
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 5);

  display.set_window(3, 9, 260, 300);
  EXPECT_EQ(bus.commands, (Bytes{0x91, 0x90}));
  EXPECT_EQ(bus.data[0x90], (Bytes{0x00, 0x0F, 0x01, 0x04, 0x01, 0x2C, 0x01}));

  bus.clear();
  display.set_window(8, 16, 0, 295);
  EXPECT_EQ(bus.data[0x90], (Bytes{0x08, 0x17, 0x00, 0x00, 0x01, 0x27, 0x01}));
}

/// The image transfer is spread over loop iterations, and the data written is still the whole buffer.
TEST(EPaperUC8151, TransferIsTimeSliced) {
  TestableUC8151 display;
  RecordingDelegate bus(&display.dc, MAX_TRANSFER_TIME + 1);  // every row overruns the time slice
  display.install(&bus, 1);
  display.set_frame(FRAME_1);

  int passes = 0;
  while (!display.transfer()) {
    ASSERT_LT(++passes, 10) << "transfer never finished";
  }
  EXPECT_GT(passes, 0) << "the transfer did not yield";
  EXPECT_EQ(count(bus.commands, 0x13), 1u) << "the command was repeated after a yield";
  EXPECT_EQ(bus.data[0x13], FRAME_1);
}

/// A partial transfer sets the window once, on the first pass only.
TEST(EPaperUC8151, PartialTransferSetsTheWindowOnce) {
  TestableUC8151 display;
  RecordingDelegate bus(&display.dc, MAX_TRANSFER_TIME + 1);
  display.install(&bus, 5);
  display.set_frame(FRAME_1);
  display.begin_partial();

  int passes = 0;
  while (!display.transfer()) {
    ASSERT_LT(++passes, 10) << "transfer never finished";
  }
  EXPECT_GT(passes, 0);
  EXPECT_EQ(count(bus.commands, 0x91), 1u);
  EXPECT_EQ(count(bus.commands, 0x90), 1u);
  EXPECT_EQ(bus.data[0x13], FRAME_1);
}

/// The rewrite of DTM1 is spread over loop iterations too; leaving partial mode happens once, before the first slice.
TEST(EPaperUC8151, PreviousImageRewriteIsTimeSliced) {
  TestableUC8151 display;
  RecordingDelegate bus(&display.dc, MAX_TRANSFER_TIME + 1);
  display.install(&bus, 5);
  display.set_frame(FRAME_1);

  int passes = 0;
  while (!display.refresh_done(true)) {
    ASSERT_LT(++passes, 10) << "rewrite never finished";
  }
  EXPECT_GT(passes, 0);
  EXPECT_EQ(count(bus.commands, 0x92), 1u);
  EXPECT_EQ(count(bus.commands, 0x10), 1u);
  EXPECT_EQ(bus.data[0x10], FRAME_1);
}

/// Without partial updates nothing is written after the refresh.
TEST(EPaperUC8151, NothingIsWrittenAfterTheRefreshWithoutPartialUpdates) {
  TestableUC8151 display;
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 1);
  display.set_frame(FRAME_1);

  EXPECT_TRUE(display.refresh_done(false));
  EXPECT_TRUE(bus.commands.empty());
}

/// None of the commands of the other UltraChip drivers are used.
TEST(EPaperUC8151, NoDualSpiOrTemperatureCommandsAreSent) {
  TestableUC8151 display;
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 5);

  display.set_frame(FRAME_1);
  display.run_update();
  display.set_frame(FRAME_2);
  display.run_update();

  for (const uint8_t command : {0xE0, 0xE5, 0x15})
    EXPECT_EQ(count(bus.commands, command), 0u) << "command " << static_cast<int>(command);
}

}  // namespace esphome::epaper_spi::testing
