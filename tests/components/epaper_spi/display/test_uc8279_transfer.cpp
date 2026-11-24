#include <gtest/gtest.h>

#include <algorithm>
#include <initializer_list>
#include <vector>

#include "../common.h"
#include "esphome/components/epaper_spi/epaper_spi_uc8279.h"

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

/// A 16x2 panel bonded to gates 1 and 2 of three, so gate 0 is outside the panel.
class TestableUC8279 : public EPaperUC8279 {
 public:
  TestableUC8279() : EPaperUC8279("test", 16, 2, nullptr, 0, 1) {}

  void install(spi::SPIDelegate *delegate, uint8_t full_update_every) {
    this->delegate_ = delegate;
    this->set_dc_pin(&this->dc);
    this->set_reset_pin(&this->reset_pin);
    ASSERT_TRUE(this->init_buffer_(this->buffer_length_));
    this->set_full_update_every(full_update_every);
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

  using EPaperUC8279::on_safe_shutdown;

  RecordingPin dc;
  CountingPin reset_pin;
};

using Bytes = std::vector<uint8_t>;

static size_t count(const Bytes &commands, uint8_t command) {
  return std::count(commands.begin(), commands.end(), command);
}

/// A full refresh writes the new image to DTM2 and white to DTM1, for every gate the controller scans:
/// white on the gate outside the panel, then the panel's rows.
TEST(EPaperUC8279, FullRefreshWritesTheImageAndAWhitePreviousImage) {
  TestableUC8279 display;
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 5);

  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  display.run_update();

  EXPECT_TRUE(display.reset_pin.level) << "reset pin left low";
  EXPECT_EQ(bus.data[0x13], (Bytes{0xFF, 0xFF, 0x0F, 0xF0, 0x3C, 0xC3}));
  EXPECT_EQ(bus.data[0x10].size(), 12u) << "DTM1 is not the white previous image plus the image written after";
  EXPECT_EQ(Bytes(bus.data[0x10].begin(), bus.data[0x10].begin() + 6), (Bytes(6, 0xFF)));
  EXPECT_EQ(count(bus.commands, 0x91), 0u) << "a full refresh entered partial mode";
  EXPECT_EQ(bus.data[0x00], (Bytes{0x17, 0x4D})) << "the OTP LUT was not selected before the refresh";
}

/// While partial updates are in use, DTM1 is rewritten with the image after every refresh, so the next
/// partial one compares against the image on the panel. The panel then waits in standby, since its own
/// deep sleep loses both planes.
TEST(EPaperUC8279, PreviousImageIsRewrittenAfterEveryRefresh) {
  TestableUC8279 display;
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 5);

  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  display.run_update();

  EXPECT_EQ(Bytes(bus.data[0x10].begin() + 6, bus.data[0x10].end()), (Bytes{0xFF, 0xFF, 0x0F, 0xF0, 0x3C, 0xC3}));
  EXPECT_EQ(bus.commands.back(), 0x02) << "the panel was not powered off after DTM1 was written";
  EXPECT_EQ(count(bus.commands, 0x07), 0u) << "the panel was put into deep sleep";
}

/// A partial refresh writes only DTM2, without a reset or the configuration, inside a partial window
/// covering the panel's gates.
TEST(EPaperUC8279, PartialRefreshWritesOnlyTheNewImage) {
  TestableUC8279 display;
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 5);

  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  display.run_update();
  bus.clear();
  const int pulses = display.reset_pin.pulses;

  display.set_frame({0x0F, 0xF0, 0x3C, 0x00});
  display.run_update();

  EXPECT_EQ(display.reset_pin.pulses, pulses) << "the partial refresh reset the panel";
  EXPECT_EQ(bus.data[0x13], (Bytes{0xFF, 0xFF, 0x0F, 0xF0, 0x3C, 0x00}));
  EXPECT_EQ(Bytes(bus.data[0x10].begin(), bus.data[0x10].end()), (Bytes{0xFF, 0xFF, 0x0F, 0xF0, 0x3C, 0x00}))
      << "DTM1 was written before the refresh, or not rewritten after it";
  EXPECT_EQ(bus.data[0x90], (Bytes{0x00, 0x00, 0x00, 0x0F, 0x00, 0x01, 0x00, 0x02, 0x01}))
      << "the partial window does not cover the panel's gates";
  const auto refresh = std::find(bus.commands.begin(), bus.commands.end(), 0x12);
  const auto dtm1 = std::find(bus.commands.begin(), bus.commands.end(), 0x10);
  EXPECT_LT(refresh, dtm1) << "DTM1 was written before the refresh";
}

/// Without partial updates the panel goes into its deep sleep after every update, and the next update
/// resets it and writes both planes again.
TEST(EPaperUC8279, EveryUpdateFullSleepsAndResetsEachTime) {
  TestableUC8279 display;
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 1);

  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  display.run_update();
  EXPECT_EQ(bus.data[0x07], (Bytes{0xA5}));
  EXPECT_EQ(bus.data[0x10], (Bytes(6, 0xFF))) << "DTM1 was rewritten without partial updates";
  bus.clear();

  display.set_frame({0x0F, 0xF0, 0x3C, 0x00});
  display.run_update();
  EXPECT_EQ(bus.data[0x13], (Bytes{0xFF, 0xFF, 0x0F, 0xF0, 0x3C, 0x00}));
  EXPECT_EQ(bus.data[0x10], (Bytes(6, 0xFF)));
  EXPECT_EQ(count(bus.commands, 0x91), 0u);
}

/// The rewrite of DTM1 is spread over loop iterations like the image transfer, and the state machine
/// waits for it: the panel is powered off and the update finishes only once all of it is written.
TEST(EPaperUC8279, PreviousImageRewriteIsTimeSliced) {
  TestableUC8279 display;
  RecordingDelegate bus(&display.dc, MAX_TRANSFER_TIME + 1);  // every row overruns the time slice
  display.install(&bus, 5);

  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  display.run_update();

  EXPECT_EQ(Bytes(bus.data[0x10].begin() + 6, bus.data[0x10].end()), (Bytes{0xFF, 0xFF, 0x0F, 0xF0, 0x3C, 0xC3}));
  EXPECT_EQ(bus.commands.back(), 0x02);
}

/// In a full update the white DTM1 plane starts on the pass after DTM2 ends, so one pass never writes both planes.
TEST(EPaperUC8279, FullTransferWritesOnePlanePerPass) {
  TestableUC8279 display;
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 1);
  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});

  EXPECT_FALSE(display.transfer());
  EXPECT_EQ(count(bus.commands, 0x13), 1u);
  EXPECT_EQ(count(bus.commands, 0x10), 0u) << "DTM1 started in the same pass as DTM2";

  EXPECT_TRUE(display.transfer());
  EXPECT_EQ(count(bus.commands, 0x13), 1u);
  EXPECT_EQ(bus.data[0x10], (Bytes(6, 0xFF)));
}

/// The full refresh that comes round after partial ones resets the controller, configures it again and writes a white
/// DTM1 over the image it held, as the first full refresh does.
TEST(EPaperUC8279, FullRefreshAfterPartialOnesStartsAfresh) {
  TestableUC8279 display;
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 2);

  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  display.run_update();
  display.set_frame({0x0F, 0xF0, 0x3C, 0x00});
  display.run_update();
  bus.clear();
  const int pulses = display.reset_pin.pulses;

  display.set_frame({0x0F, 0xF0, 0x00, 0x00});
  display.run_update();
  EXPECT_EQ(display.reset_pin.pulses, pulses + 1) << "the full refresh did not reset the controller";
  EXPECT_EQ(count(bus.commands, 0x91), 0u) << "the full refresh entered partial mode";
  ASSERT_EQ(bus.data[0x10].size(), 12u);
  EXPECT_EQ(Bytes(bus.data[0x10].begin(), bus.data[0x10].begin() + 6), (Bytes(6, 0xFF)));
  EXPECT_EQ(Bytes(bus.data[0x10].begin() + 6, bus.data[0x10].end()), (Bytes{0xFF, 0xFF, 0x0F, 0xF0, 0x00, 0x00}));
}

/// A requested full update, between partial ones, is a full refresh from scratch too.
TEST(EPaperUC8279, RequestedFullUpdateStartsAfresh) {
  TestableUC8279 display;
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 5);

  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  display.run_update();
  bus.clear();
  const int pulses = display.reset_pin.pulses;

  display.request_full_update();
  display.set_frame({0x0F, 0xF0, 0x3C, 0x00});
  display.run_update();
  EXPECT_EQ(display.reset_pin.pulses, pulses + 1);
  EXPECT_EQ(count(bus.commands, 0x91), 0u);
  EXPECT_EQ(Bytes(bus.data[0x10].begin(), bus.data[0x10].begin() + 6), (Bytes(6, 0xFF)));
}

/// Before a restart or the controller's deep sleep the panel goes into its own deep sleep, even while partial updates
/// would otherwise leave it in standby.
TEST(EPaperUC8279, ShutdownPutsThePanelToSleep) {
  TestableUC8279 display;
  RecordingDelegate bus(&display.dc);
  display.install(&bus, 5);

  display.set_frame({0x0F, 0xF0, 0x3C, 0xC3});
  display.run_update();
  bus.clear();
  display.on_safe_shutdown();
  EXPECT_EQ(bus.data[0x07], (Bytes{0xA5}));
}

}  // namespace esphome::epaper_spi::testing
