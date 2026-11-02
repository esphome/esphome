#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include "../common.h"
#include "esphome/components/epaper_spi/epaper_goodisplay_3c.h"

namespace esphome::epaper_spi::testing {
namespace {

class TestableGoodDisplay3C : public EPaperGoodDisplay3C {
 public:
  TestableGoodDisplay3C(uint16_t width, uint16_t height) : EPaperGoodDisplay3C("test", width, height, nullptr, 0) {}

  void install(spi::SPIDelegate *delegate) {
    this->delegate_ = delegate;
    this->set_dc_pin(&this->dc);
    ASSERT_TRUE(this->init_buffer_(this->buffer_length_));
  }

  void draw(int x, int y, Color color) { this->draw_pixel_at(x, y, color); }

  /// Both planes of one push; returns how many calls it took.
  int run_push() {
    int calls = 1;
    while (!this->transfer_data())
      calls++;
    return calls;
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

  using EPaperGoodDisplay3C::REFRESH_START_MS;

  RecordingPin dc;
};

/// Also notes when each command byte was sent.
class TimedCommandDelegate : public RecordingDelegate {
 public:
  using RecordingDelegate::RecordingDelegate;

  void write_array(const uint8_t *ptr, size_t length) override {
    if (!this->dc_->level) {
      for (size_t i = 0; i != length; i++)
        this->sent.emplace_back(ptr[i], millis());
    }
    RecordingDelegate::write_array(ptr, length);
  }

  /// When the n-th (from 0) copy of a command was sent.
  uint32_t time_of(uint8_t command, int n = 0) const {
    for (const auto &[byte, time] : this->sent) {
      if (byte == command && n-- == 0)
        return time;
    }
    ADD_FAILURE() << "command " << int(command) << " not sent";
    return 0;
  }

  std::vector<std::pair<uint8_t, uint32_t>> sent;
};

using Bytes = std::vector<uint8_t>;

const Color RED(255, 0, 0);

/// One row of 16 pixels: white, black, red, then white. The red plane goes first as 1=red, then the black/white
/// plane as 1=white with red pixels sent as 0.
TEST(EPaperGoodDisplay3CTransfer, PixelsReachThePanel) {
  TestableGoodDisplay3C display(16, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  display.fill(Color::WHITE);
  display.draw(1, 0, Color::BLACK);
  display.draw(2, 0, RED);
  EXPECT_EQ(display.run_push(), 1);

  EXPECT_EQ(bus.commands, (Bytes{0x44, 0x45, 0x4E, 0x4F, 0x26, 0x24}));
  EXPECT_EQ(bus.data[0x44], (Bytes{0x00, 0x01}));
  EXPECT_EQ(bus.data[0x45], (Bytes{0x00, 0x00, 0x00, 0x00}));
  EXPECT_EQ(bus.data[0x24], (Bytes{0x9F, 0xFF}));
  EXPECT_EQ(bus.data[0x26], (Bytes{0x20, 0x00}));
}

/// A push that yields to the main loop after every chunk sends the same commands and bytes as one that runs to
/// completion in one call, and a second push starts again from the window setup.
TEST(EPaperGoodDisplay3CTransfer, ResumedPushMatches) {
  // 128x20 pixels is 320 bytes per plane, so each plane takes three chunks of up to MAX_TRANSFER_SIZE.
  constexpr int width = 128;
  constexpr int height = 20;
  const Color colors[] = {Color::WHITE, Color::BLACK, RED};

  TestableGoodDisplay3C reference(width, height);
  RecordingDelegate reference_bus(&reference.dc);
  reference.install(&reference_bus);
  TestableGoodDisplay3C display(width, height);
  // Every data write takes longer than MAX_TRANSFER_TIME, so the push yields after each chunk.
  RecordingDelegate bus(&display.dc, MAX_TRANSFER_TIME + 1);
  display.install(&bus);
  for (int y = 0; y != height; y++) {
    for (int x = 0; x != width; x++) {
      reference.draw(x, y, colors[(x * 7 + y * 3) % 3]);
      display.draw(x, y, colors[(x * 7 + y * 3) % 3]);
    }
  }

  EXPECT_EQ(reference.run_push(), 1);
  EXPECT_GT(display.run_push(), 2);

  EXPECT_EQ(bus.commands, reference_bus.commands);
  EXPECT_EQ(bus.data[0x24].size(), 320u);
  EXPECT_EQ(bus.data[0x24], reference_bus.data[0x24]);
  EXPECT_EQ(bus.data[0x26], reference_bus.data[0x26]);

  bus.clear();
  display.run_push();
  EXPECT_EQ(bus.commands, reference_bus.commands);
}

/// A whole update sends no power-on (0x22 0xF8) before the refresh (0x22 0xF7): with it, this panel does not
/// refresh. Power off (0x83) and deep sleep follow.
TEST(EPaperGoodDisplay3CTransfer, UpdateHasNoSeparatePowerOn) {
  TestableGoodDisplay3C display(16, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);
  display.fill(RED);

  display.run_update();

  EXPECT_EQ(bus.commands, (Bytes{0x44, 0x45, 0x4E, 0x4F, 0x26, 0x24, 0x4E, 0x4F, 0x22, 0x20, 0x22, 0x20, 0x10}));
  EXPECT_EQ(bus.data[0x26], (Bytes{0xFF, 0xFF}));
  EXPECT_EQ(bus.data[0x24], (Bytes{0x00, 0x00}));
  EXPECT_EQ(bus.data[0x22], (Bytes{0xF7, 0x83}));
  EXPECT_EQ(bus.data[0x10], (Bytes{0x01}));
}

/// Busy only goes high a little after master activation (0x20), so power off is not sent until REFRESH_START_MS
/// after the refresh starts.
TEST(EPaperGoodDisplay3CTransfer, WaitsForBusyAfterTheRefreshStarts) {
  TestableGoodDisplay3C display(16, 1);
  TimedCommandDelegate bus(&display.dc);
  display.install(&bus);
  display.fill(RED);

  display.run_update();

  // The first 0x20 starts the refresh; the second 0x22 selects power off.
  EXPECT_GE(bus.time_of(0x22, 1) - bus.time_of(0x20, 0), TestableGoodDisplay3C::REFRESH_START_MS);
}

}  // namespace
}  // namespace esphome::epaper_spi::testing
