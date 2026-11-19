#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>

#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/partition/light_partition.h"

namespace esphome::partition::testing {

namespace {

// One RGB pixel in memory
class RgbStrip : public light::AddressableLight {
 public:
  RgbStrip()
      : internal_accessor_(this->buffer_.data(), this->buffer_.data() + 3, light::ChannelColors{0, 1, 2},
                           &this->correction_) {
    this->accessor_ = &this->internal_accessor_;
  }
  int32_t size() const override { return 1; }
  void clear_effect_data() override {}
  light::LightTraits get_traits() override { return {}; }
  void write_state(light::LightState *state) override {}

 protected:
  std::array<uint8_t, 4> buffer_{};
  light::CorrectedBufferColorAccessor internal_accessor_;
};

}  // namespace

// The partition corrects a pixel before the strip drops its white level, so it has to drop it first.
TEST(PartitionWhite, UnusedWhiteDoesNotDarkenRgbSegment) {
  light::GammaTable table{};
  for (int i = 1; i < 256; i++)
    table.lut[i] = std::max(1.0, std::round(std::pow(i / 255.0, 2.8) * 65535.0));
  table.gamma_x100 = 280;

  RgbStrip strip;
  light::LightState strip_state(&strip);
  PartitionLightOutput partition({AddressableSegment(&strip_state, 0, 1, false)});
  light::LightState partition_state(&partition);
  partition_state.set_gamma_table(&table);
  partition.setup_state(&partition_state);
  partition_state.current_values.set_state(1.0f);
  partition_state.current_values.set_brightness(13 / 255.0f);
  partition.update_state(&partition_state);

  partition[0] = Color(128, 0, 0, 255);
  EXPECT_EQ(strip[0].get_raw(), Color(1, 0, 0, 0));
}

}  // namespace esphome::partition::testing
