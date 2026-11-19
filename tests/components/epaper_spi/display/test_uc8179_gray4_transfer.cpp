#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "../common.h"
#include "esphome/components/epaper_spi/epaper_spi_uc8179_gray4.h"

namespace esphome::epaper_spi::testing {

class TestableUC8179Gray4 : public EPaperUC8179Gray4 {
 public:
  TestableUC8179Gray4(uint16_t width, uint16_t height) : EPaperUC8179Gray4("test", width, height, nullptr, 0) {}

  void install(spi::SPIDelegate *delegate) {
    this->delegate_ = delegate;
    this->set_dc_pin(&this->dc);
    ASSERT_TRUE(this->init_buffer_(this->buffer_length_));
  }

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

namespace {

constexpr uint8_t DTM1 = 0x10;
constexpr uint8_t DTM2 = 0x13;
constexpr uint8_t LUT_VCOM = 0x20;
constexpr uint8_t LUT_WW = 0x21;
constexpr uint8_t LUT_KW = 0x22;
constexpr uint8_t LUT_WK = 0x23;
constexpr uint8_t LUT_KK = 0x24;
constexpr uint8_t VDH = 1;  // towards black
constexpr uint8_t VDL = 2;  // towards white

/// A grey that lands squarely on each of the four levels.
Color color_for_level(uint8_t level) {
  static const uint8_t GRAYS[4] = {0, 64, 128, 255};
  const uint8_t v = GRAYS[level];
  return Color(v, v, v);
}

void draw_row(TestableUC8179Gray4 &display, int y, const std::vector<uint8_t> &levels) {
  for (size_t x = 0; x != levels.size(); x++)
    display.draw_pixel_at((int) x, y, color_for_level(levels[x]));
}

/// How many frames a waveform holds a voltage for; 0 counts every frame, whatever the voltage.
int frames_at(const Bytes &waveform, uint8_t voltage) {
  int frames = 0;
  for (size_t step = 0; step + 6 <= waveform.size(); step += 6) {
    for (int slot = 0; slot < 4; slot++) {
      if (voltage == 0 || ((waveform[step] >> (6 - 2 * slot)) & 0x03) == voltage)
        frames += waveform[step + 1 + slot] * waveform[step + 5];
    }
  }
  return frames;
}

}  // namespace

/// DTM2 then DTM1 choose a pixel's waveform: 01 white, 00 light grey, 10 dark grey and 11 black.
/// So DTM2 is the level's high bit inverted, and DTM1 is set where the level's two bits are equal.
TEST(EPaperUC8179Gray4, SplitsEachLevelAcrossBothPlanes) {
  TestableUC8179Gray4 display(8, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  draw_row(display, 0, {0, 1, 2, 3, 0, 1, 2, 3});
  display.run_push();

  //         levels  0 1 2 3 0 1 2 3
  // high bit        0 0 1 1 0 0 1 1 = 0x33, inverted 0xCC
  // bits equal      1 0 0 1 1 0 0 1 = 0x99
  EXPECT_EQ(bus.data[DTM2], (Bytes{0xCC}));
  EXPECT_EQ(bus.data[DTM1], (Bytes{0x99}));
}

/// Two buffer bytes (4 pixels each) make one plane byte (8 pixels), leftmost pixel in the most
/// significant bit. An asymmetric row catches a swapped pair or reversed bit order.
TEST(EPaperUC8179Gray4, PacksPixelsLeftmostFirstAcrossSourceBytes) {
  TestableUC8179Gray4 display(16, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  draw_row(display, 0, {3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2});
  display.run_push();

  EXPECT_EQ(bus.data[DTM2], (Bytes{0x7F, 0xFE}));
  EXPECT_EQ(bus.data[DTM1], (Bytes{0xFF, 0xFE}));
}

/// The panel must take its waveforms from registers, and get all five before the image.
TEST(EPaperUC8179Gray4, SendsWaveformsBeforeTheImage) {
  TestableUC8179Gray4 display(8, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);
  display.run_push();

  EXPECT_EQ(bus.data[0x00], (Bytes{0x3F})) << "panel setting does not select waveforms from registers";
  const auto first_plane = std::find(bus.commands.begin(), bus.commands.end(), DTM1);
  ASSERT_NE(first_plane, bus.commands.end());
  for (const uint8_t lut : {LUT_VCOM, LUT_WW, LUT_KW, LUT_WK, LUT_KK}) {
    EXPECT_EQ(bus.data[lut].size(), 42u) << "waveform 0x" << std::hex << (int) lut;
    EXPECT_LT(std::find(bus.commands.begin(), bus.commands.end(), lut), first_plane)
        << "waveform 0x" << std::hex << (int) lut << " was not sent before the image";
  }
}

/// A waveform which pushes more one way than the other leaves charge in the panel on every
/// refresh. All must also last as long as the VCOM waveform, which the controller steps alongside.
TEST(EPaperUC8179Gray4, EveryWaveformIsBalancedAndAsLongAsTheOthers) {
  TestableUC8179Gray4 display(8, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);
  display.run_push();

  const int length = frames_at(bus.data[LUT_VCOM], 0);
  EXPECT_GT(length, 0);
  for (const uint8_t lut : {LUT_WW, LUT_KW, LUT_WK, LUT_KK}) {
    const Bytes &waveform = bus.data[lut];
    EXPECT_EQ(frames_at(waveform, VDH), frames_at(waveform, VDL)) << "waveform 0x" << std::hex << (int) lut;
    EXPECT_EQ(frames_at(waveform, 0), length) << "waveform 0x" << std::hex << (int) lut;
  }
}

/// The last push decides the level: the further towards white, the lighter. White is on the
/// K-to-W waveform, which the border follows.
TEST(EPaperUC8179Gray4, LastPushGrowsFromBlackToWhite) {
  TestableUC8179Gray4 display(8, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);
  display.run_push();

  // The third step: bytes 12..17, the push in its first slot
  auto last_push = [&](uint8_t lut) { return bus.data[lut][13]; };
  auto last_voltage = [&](uint8_t lut) { return (uint8_t) ((bus.data[lut][12] >> 6) & 0x03); };
  EXPECT_EQ(last_voltage(LUT_WK), VDL);
  EXPECT_EQ(last_voltage(LUT_WW), VDL);
  EXPECT_EQ(last_voltage(LUT_KW), VDL);
  EXPECT_GT(last_push(LUT_WK), 0);
  EXPECT_LT(last_push(LUT_WK), last_push(LUT_WW));
  EXPECT_LT(last_push(LUT_WW), last_push(LUT_KW));
  EXPECT_EQ(frames_at(Bytes(bus.data[LUT_KK].begin() + 12, bus.data[LUT_KK].end()), VDL), 0)
      << "black is pushed towards white after clearing";
}

/// A transfer which runs out of time resumes where it stopped, without starting a plane again.
TEST(EPaperUC8179Gray4, ResumesAfterYielding) {
  TestableUC8179Gray4 display(1024, 4);
  RecordingDelegate bus(&display.dc, 6);
  display.install(&bus);

  EXPECT_GT(display.run_push(), 1);

  EXPECT_EQ(bus.data[DTM1].size(), 512u);
  EXPECT_EQ(bus.data[DTM2].size(), 512u);
  EXPECT_EQ(std::count(bus.commands.begin(), bus.commands.end(), DTM1), 1);
  EXPECT_EQ(std::count(bus.commands.begin(), bus.commands.end(), DTM2), 1);
  EXPECT_EQ(std::count(bus.commands.begin(), bus.commands.end(), LUT_VCOM), 1);
}

}  // namespace esphome::epaper_spi::testing
