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

  /// Both planes of one push; returns how many calls it took.
  int run_push() {
    int calls = 1;
    while (!this->transfer_data())
      calls++;
    return calls;
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

/// A push that yields to the main loop part way through each plane, and resumes over several calls, sends the same
/// commands and bytes as a push that runs to completion in one call.
template<typename Driver> void expect_resumed_push_matches() {
  // 128x20 pixels is 320 bytes per plane, so each plane takes three chunks of up to MAX_TRANSFER_SIZE.
  constexpr int width = 128;
  constexpr int height = 20;
  const Color colors[] = {Color::WHITE, Color::BLACK, RED};

  Testable<Driver> reference(width, height);
  RecordingDelegate reference_bus(&reference.dc);
  reference.install(&reference_bus);
  Testable<Driver> display(width, height);
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

  expect_red_plane_first(bus);
  EXPECT_EQ(bus.commands, reference_bus.commands);
  EXPECT_EQ(bus.data[0x26].size(), 320u);
  EXPECT_EQ(bus.data[0x24], reference_bus.data[0x24]);
  EXPECT_EQ(bus.data[0x26], reference_bus.data[0x26]);
}

TEST(EPaperWeAct3CTransfer, WeActResumedPush) { expect_resumed_push_matches<EPaperWeAct3C>(); }

TEST(EPaperWeAct3CTransfer, WaveshareBResumedPush) { expect_resumed_push_matches<EpaperWaveshareB>(); }

/// A second push starts again from the window setup and the red plane.
TEST(EPaperWeAct3CTransfer, SecondPushStartsAgain) {
  Testable<EPaperWeAct3C> display(16, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);
  display.fill(RED);

  display.run_push();
  const Bytes first_commands = bus.commands;
  bus.clear();
  display.run_push();

  EXPECT_EQ(bus.commands, first_commands);
  EXPECT_EQ(bus.data[0x24], (Bytes{0x00, 0x00}));
  EXPECT_EQ(bus.data[0x26], (Bytes{0xFF, 0xFF}));
}

}  // namespace
}  // namespace esphome::epaper_spi::testing
