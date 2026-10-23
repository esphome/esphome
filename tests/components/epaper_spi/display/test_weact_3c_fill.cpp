#include <gtest/gtest.h>

#include <vector>

#include "../common.h"
#include "esphome/components/epaper_spi/epaper_weact_3c.h"

namespace esphome::epaper_spi::testing {
namespace {

class TestableWeAct3C : public EPaperWeAct3C {
 public:
  TestableWeAct3C(uint16_t width, uint16_t height) : EPaperWeAct3C("test", width, height, nullptr, 0) {}

  void install(spi::SPIDelegate *delegate) {
    this->delegate_ = delegate;
    this->set_dc_pin(&this->dc);
    ASSERT_TRUE(this->init_buffer_(this->buffer_length_));
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

/// fill() writes the buffer without drawing pixels, so it must mark the frame drawn itself. Otherwise an update
/// whose only drawing is a fill (or the auto clear) is dropped once a refresh has reset the drawn area.
TEST(EPaperWeAct3CFill, FillOnlyUpdateAfterARefreshIsSent) {
  TestableWeAct3C display(16, 1);
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  display.fill(RED);
  display.run_update();
  bus.clear();

  display.fill(Color::WHITE);
  display.run_update();

  EXPECT_EQ(bus.data[0x26], (Bytes{0x00, 0x00})) << "fill-only update was skipped";
  EXPECT_EQ(bus.data[0x24], (Bytes{0xFF, 0xFF}));
}

}  // namespace
}  // namespace esphome::epaper_spi::testing
