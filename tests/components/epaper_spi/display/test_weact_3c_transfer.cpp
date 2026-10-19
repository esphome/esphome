#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "../common.h"
#include "esphome/components/epaper_spi/epaper_waveshare_b.h"
#include "esphome/components/epaper_spi/epaper_weact_3c.h"

namespace esphome::epaper_spi::testing {
namespace {

template<typename Driver> class Testable : public Driver {
 public:
  Testable(uint16_t width, uint16_t height) : Driver("test", width, height, nullptr, 0) {}

  void install(spi::SPIDelegate *delegate) {
    this->delegate_ = delegate;
    this->set_dc_pin(&this->dc);
    ASSERT_TRUE(this->init_buffer_(this->buffer_length_));
  }

  void draw(int x, int y, Color color) { this->draw_pixel_at(x, y, color); }

  void run_push() {
    while (!this->transfer_data()) {
    }
  }

  RecordingPin dc;
};

using Bytes = std::vector<uint8_t>;

const Color RED(255, 0, 0);

/// The red plane (0x26) must go out before the black/white plane (0x24).
void expect_red_plane_first(const RecordingDelegate &bus) {
  const auto red = std::find(bus.commands.begin(), bus.commands.end(), 0x26);
  const auto bw = std::find(bus.commands.begin(), bus.commands.end(), 0x24);
  ASSERT_NE(red, bus.commands.end());
  ASSERT_NE(bw, bus.commands.end());
  EXPECT_LT(red, bw);
}

/// One row of 16 pixels: white, black, red, then white. The panel gets the black/white plane as
/// 1=white, 0=black or red, and the red plane as 1=red.
TEST(EPaperWeAct3CTransfer, PixelsReachThePanelUnchanged) {
  Testable<EPaperWeAct3C> display(16, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  display.fill(Color::WHITE);
  display.draw(1, 0, Color::BLACK);
  display.draw(2, 0, RED);
  display.run_push();

  expect_red_plane_first(bus);
  EXPECT_EQ(bus.data[0x24], (Bytes{0x9F, 0xFF}));
  EXPECT_EQ(bus.data[0x26], (Bytes{0x20, 0x00}));
}

TEST(EPaperWeAct3CTransfer, FillsReachThePanelUnchanged) {
  Testable<EPaperWeAct3C> display(16, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  display.fill(Color::WHITE);
  display.run_push();
  EXPECT_EQ(bus.data[0x24], (Bytes{0xFF, 0xFF}));
  EXPECT_EQ(bus.data[0x26], (Bytes{0x00, 0x00}));

  bus.clear();
  display.fill(Color::BLACK);
  display.run_push();
  EXPECT_EQ(bus.data[0x24], (Bytes{0x00, 0x00}));
  EXPECT_EQ(bus.data[0x26], (Bytes{0x00, 0x00}));

  bus.clear();
  display.fill(RED);
  display.run_push();
  EXPECT_EQ(bus.data[0x24], (Bytes{0x00, 0x00}));
  EXPECT_EQ(bus.data[0x26], (Bytes{0xFF, 0xFF}));
}

/// Waveshare (B) panels take the red plane as 0=red; the black/white plane is the same as WeAct's.
TEST(EPaperWeAct3CTransfer, WaveshareBInvertsOnlyTheRedPlane) {
  Testable<EpaperWaveshareB> display(16, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  display.fill(Color::WHITE);
  display.draw(1, 0, Color::BLACK);
  display.draw(2, 0, RED);
  display.run_push();

  expect_red_plane_first(bus);
  EXPECT_EQ(bus.data[0x24], (Bytes{0x9F, 0xFF}));
  EXPECT_EQ(bus.data[0x26], (Bytes{0xDF, 0xFF}));

  bus.clear();
  display.fill(RED);
  display.run_push();
  EXPECT_EQ(bus.data[0x24], (Bytes{0x00, 0x00}));
  EXPECT_EQ(bus.data[0x26], (Bytes{0x00, 0x00}));
}

}  // namespace
}  // namespace esphome::epaper_spi::testing
