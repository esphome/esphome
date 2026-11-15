#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "../common.h"
#include "esphome/components/epaper_spi/epaper_spi_inkplate2.h"
#include "esphome/components/epaper_spi/epaper_waveshare_bwr.h"

// What the Inkplate 2 and Waveshare BWR drivers send to the panel. These tests only use what the drivers had before
// they were built on EPaperBWR (the constructor, draw_pixel_at(), fill() and transfer_data()), and the expected bytes
// are the ones those drivers sent, so the same tests prove the shared code sends the same bytes.

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

/// 16x2 pixels, so 2 bytes per row and 4 bytes per plane: white, with black at (1, 0) and (8, 1), red at (2, 0)
/// and (15, 1).
template<typename Driver> void draw_test_pattern(Testable<Driver> &display) {
  display.fill(Color::WHITE);
  display.draw(1, 0, Color::BLACK);
  display.draw(2, 0, RED);
  display.draw(8, 1, Color::BLACK);
  display.draw(15, 1, RED);
}

/// A pattern of all three colors over a display large enough that each plane takes several chunks.
template<typename Driver> void draw_large_pattern(Testable<Driver> &display, int width, int height) {
  const Color colors[] = {Color::WHITE, Color::BLACK, RED};
  for (int y = 0; y != height; y++) {
    for (int x = 0; x != width; x++)
      display.draw(x, y, colors[(x * 7 + y * 3) % 3]);
  }
}

/// The black/white plane (0x10) goes out before the red plane (0x13), each command once per push.
void expect_bw_plane_first(const RecordingDelegate &bus) {
  EXPECT_EQ(std::count(bus.commands.begin(), bus.commands.end(), 0x10), 1);
  EXPECT_EQ(std::count(bus.commands.begin(), bus.commands.end(), 0x13), 1);
  const auto bw = std::find(bus.commands.begin(), bus.commands.end(), 0x10);
  const auto red = std::find(bus.commands.begin(), bus.commands.end(), 0x13);
  EXPECT_LT(bw, red);
}

// Inkplate 2: black/white plane 1=white, 0=black, with red pixels sent as 1; red plane 0=red.

TEST(EPaperBWRSameBytes, Inkplate2Pixels) {
  Testable<EPaperInkplate2> display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  draw_test_pattern(display);
  display.run_push();

  expect_bw_plane_first(bus);
  EXPECT_EQ(bus.data[0x10], (Bytes{0xBF, 0xFF, 0xFF, 0x7F}));
  EXPECT_EQ(bus.data[0x13], (Bytes{0xDF, 0xFF, 0xFF, 0xFE}));
}

TEST(EPaperBWRSameBytes, Inkplate2Fills) {
  Testable<EPaperInkplate2> display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  display.fill(Color::WHITE);
  display.run_push();
  EXPECT_EQ(bus.data[0x10], (Bytes{0xFF, 0xFF, 0xFF, 0xFF}));
  EXPECT_EQ(bus.data[0x13], (Bytes{0xFF, 0xFF, 0xFF, 0xFF}));

  bus.clear();
  display.fill(Color::BLACK);
  display.run_push();
  EXPECT_EQ(bus.data[0x10], (Bytes{0x00, 0x00, 0x00, 0x00}));
  EXPECT_EQ(bus.data[0x13], (Bytes{0xFF, 0xFF, 0xFF, 0xFF}));

  bus.clear();
  display.fill(RED);
  display.run_push();
  EXPECT_EQ(bus.data[0x10], (Bytes{0xFF, 0xFF, 0xFF, 0xFF}));
  EXPECT_EQ(bus.data[0x13], (Bytes{0x00, 0x00, 0x00, 0x00}));
}

// Waveshare BWR (UC8179 and UC8151D): black/white plane 1=white, 0=black, with red pixels sent as 1; red plane 1=red.

TEST(EPaperBWRSameBytes, WaveshareBWRPixels) {
  Testable<EPaperWaveshareBWR> display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  draw_test_pattern(display);
  display.run_push();

  expect_bw_plane_first(bus);
  EXPECT_EQ(bus.data[0x10], (Bytes{0xBF, 0xFF, 0xFF, 0x7F}));
  EXPECT_EQ(bus.data[0x13], (Bytes{0x20, 0x00, 0x00, 0x01}));
}

TEST(EPaperBWRSameBytes, WaveshareBWRFills) {
  Testable<EPaperWaveshareBWR> display(16, 2);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  display.fill(Color::WHITE);
  display.run_push();
  EXPECT_EQ(bus.data[0x10], (Bytes{0xFF, 0xFF, 0xFF, 0xFF}));
  EXPECT_EQ(bus.data[0x13], (Bytes{0x00, 0x00, 0x00, 0x00}));

  bus.clear();
  display.fill(Color::BLACK);
  display.run_push();
  EXPECT_EQ(bus.data[0x10], (Bytes{0x00, 0x00, 0x00, 0x00}));
  EXPECT_EQ(bus.data[0x13], (Bytes{0x00, 0x00, 0x00, 0x00}));

  bus.clear();
  display.fill(RED);
  display.run_push();
  EXPECT_EQ(bus.data[0x10], (Bytes{0xFF, 0xFF, 0xFF, 0xFF}));
  EXPECT_EQ(bus.data[0x13], (Bytes{0xFF, 0xFF, 0xFF, 0xFF}));
}

/// A push that yields to the main loop part way through, and resumes over several calls, sends the same bytes and
/// each plane command once, the same as a push that runs to completion in one call.
template<typename Driver> void expect_resumed_push_matches() {
  // 128x20 pixels is 320 bytes per plane, so each plane takes three chunks of up to MAX_TRANSFER_SIZE.
  constexpr int width = 128;
  constexpr int height = 20;

  Testable<Driver> reference(width, height);
  RecordingDelegate reference_bus(&reference.dc);
  reference.install(&reference_bus);
  draw_large_pattern(reference, width, height);
  EXPECT_EQ(reference.run_push(), 1);

  // Every data write takes longer than MAX_TRANSFER_TIME, so the push yields after each chunk.
  Testable<Driver> display(width, height);
  RecordingDelegate bus(&display.dc, MAX_TRANSFER_TIME + 1);
  display.install(&bus);
  draw_large_pattern(display, width, height);
  EXPECT_GT(display.run_push(), 1);

  expect_bw_plane_first(bus);
  EXPECT_EQ(bus.data[0x10].size(), 320u);
  EXPECT_EQ(bus.data[0x10], reference_bus.data[0x10]);
  EXPECT_EQ(bus.data[0x13], reference_bus.data[0x13]);
}

TEST(EPaperBWRSameBytes, Inkplate2ResumedPush) { expect_resumed_push_matches<EPaperInkplate2>(); }

TEST(EPaperBWRSameBytes, WaveshareBWRResumedPush) { expect_resumed_push_matches<EPaperWaveshareBWR>(); }

}  // namespace
}  // namespace esphome::epaper_spi::testing
