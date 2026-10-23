#include <gtest/gtest.h>

#include <vector>

#include "../common.h"
#include "esphome/components/epaper_spi/epaper_spi_ssd1677_bwr.h"

namespace esphome::epaper_spi::testing {
namespace {

class TestableSSD1677BWR : public EPaperSSD1677BWR {
 public:
  TestableSSD1677BWR(uint16_t width, uint16_t height, uint16_t offset_height)
      : EPaperSSD1677BWR("test", width, height, nullptr, 0, offset_height) {}

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

  RecordingPin dc;
};

using Bytes = std::vector<uint8_t>;

const Color RED(255, 0, 0);

/// The window is the panel's rows at the end of the gates, written from the last gate down, with X in pixels.
/// One row of 16 pixels: white, black, red, then white. The red plane goes first as 1=red, then the black/white
/// plane as 1=white with red pixels sent as 0.
TEST(EPaperSSD1677BWRTransfer, PixelsReachThePanel) {
  TestableSSD1677BWR display(16, 2, 3);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  display.fill(Color::WHITE);
  display.draw(1, 0, Color::BLACK);
  display.draw(2, 0, RED);
  EXPECT_EQ(display.run_push(), 1);

  EXPECT_EQ(bus.commands, (Bytes{0x44, 0x45, 0x4E, 0x4F, 0x26, 0x24}));
  EXPECT_EQ(bus.data[0x44], (Bytes{0x00, 0x00, 0x0F, 0x00}));
  EXPECT_EQ(bus.data[0x45], (Bytes{0x04, 0x00, 0x03, 0x00}));
  EXPECT_EQ(bus.data[0x4E], (Bytes{0x00, 0x00}));
  EXPECT_EQ(bus.data[0x4F], (Bytes{0x04, 0x00}));
  EXPECT_EQ(bus.data[0x26], (Bytes{0x20, 0x00, 0x00, 0x00}));
  EXPECT_EQ(bus.data[0x24], (Bytes{0x9F, 0xFF, 0xFF, 0xFF}));
}

/// Addresses past 255 use both bytes: the 880x528 panel on the last 528 of 680 gates.
TEST(EPaperSSD1677BWRTransfer, WindowOfTheFullSizePanel) {
  TestableSSD1677BWR display(880, 528, 152);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  display.run_push();

  EXPECT_EQ(bus.data[0x44], (Bytes{0x00, 0x00, 0x6F, 0x03}));
  EXPECT_EQ(bus.data[0x45], (Bytes{0xA7, 0x02, 0x98, 0x00}));
  EXPECT_EQ(bus.data[0x4F], (Bytes{0xA7, 0x02}));
  EXPECT_EQ(bus.data[0x26].size(), 110u * 528u);
  EXPECT_EQ(bus.data[0x24].size(), 110u * 528u);
}

/// A whole update starts with a software reset, and sends no separate power-on before the refresh (0x22 0xF7).
/// Power off (0x83) and deep sleep follow.
TEST(EPaperSSD1677BWRTransfer, UpdateSequence) {
  TestableSSD1677BWR display(16, 1, 0);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);
  display.fill(RED);

  display.run_update();

  EXPECT_EQ(bus.commands, (Bytes{0x12, 0x44, 0x45, 0x4E, 0x4F, 0x26, 0x24, 0x22, 0x20, 0x22, 0x20, 0x10}));
  EXPECT_EQ(bus.data[0x26], (Bytes{0xFF, 0xFF}));
  EXPECT_EQ(bus.data[0x24], (Bytes{0x00, 0x00}));
  EXPECT_EQ(bus.data[0x22], (Bytes{0xF7, 0x83}));
  EXPECT_EQ(bus.data[0x10], (Bytes{0x01}));
}

}  // namespace
}  // namespace esphome::epaper_spi::testing
