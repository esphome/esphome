#include <gtest/gtest.h>
#include <optional>
#include <vector>

#include "esphome/components/gree/gree.h"
#include "esphome/components/remote_base/remote_base.h"

namespace esphome::gree::testing {

class CapturingTransmitter : public remote_base::RemoteTransmitterBase {
 public:
  CapturingTransmitter() : RemoteTransmitterBase(nullptr) {}

  const remote_base::RawTimings &last_data() const { return this->last_data_; }
  uint32_t last_carrier_frequency() const { return this->last_carrier_frequency_; }

 protected:
  void send_internal(uint32_t, uint32_t) override {
    this->last_data_ = this->temp_.get_data();
    this->last_carrier_frequency_ = this->temp_.get_carrier_frequency();
  }

 private:
  remote_base::RawTimings last_data_{};
  uint32_t last_carrier_frequency_{0};
};

class TestableGreeClimate : public GreeClimate {
 public:
  using GreeClimate::transmit_state;
};

// Decode one payload byte from mark/space pairs (LSB first).
// RemoteTransmitData stores marks as positive and spaces as negative durations.
static bool decode_byte(const remote_base::RawTimings &timings, size_t *index, uint8_t *out) {
  uint8_t value = 0;
  for (uint8_t bit = 0; bit < 8; bit++) {
    if (*index + 1 >= timings.size()) {
      return false;
    }
    const int32_t mark = timings[(*index)++];
    const int32_t space = timings[(*index)++];
    if (mark <= 0 || space >= 0) {
      return false;
    }
    if (-space > 1000) {
      value |= static_cast<uint8_t>(1u << bit);
    }
  }
  *out = value;
  return true;
}

static bool expect_mark_space(const remote_base::RawTimings &timings, size_t *index) {
  if (*index + 1 >= timings.size()) {
    return false;
  }
  const int32_t mark = timings[(*index)++];
  const int32_t space = timings[(*index)++];
  return mark > 0 && space < 0;
}

// Extract three YAP blocks into the 25-byte logical buffer (HeatpumpIR layout).
static bool decode_yap_frame(const remote_base::RawTimings &timings, uint8_t out[25]) {
  size_t index = 0;
  for (size_t block = 0; block < 3; block++) {
    if (!expect_mark_space(timings, &index)) {
      return false;
    }
    for (size_t i = 0; i < 4; i++) {
      if (!decode_byte(timings, &index, &out[block * 8 + i])) {
        return false;
      }
    }
    // fixed 010 marker: three mark/space pairs
    for (int i = 0; i < 3; i++) {
      if (!expect_mark_space(timings, &index)) {
        return false;
      }
    }
    // gap mark/space before second half
    if (!expect_mark_space(timings, &index)) {
      return false;
    }
    for (size_t i = 5; i < 9; i++) {
      if (!decode_byte(timings, &index, &out[block * 8 + i])) {
        return false;
      }
    }
    // trailing mark
    if (index >= timings.size()) {
      return false;
    }
    if (timings[index] <= 0) {
      return false;
    }
    index++;
    // inter-block message space (negative) between the three repeats
    if (block + 1 < 3) {
      if (index >= timings.size()) {
        return false;
      }
      if (timings[index] >= 0) {
        return false;
      }
      index++;
    }
  }
  return true;
}

static uint8_t yap_checksum(const uint8_t *buffer) {
  const uint8_t sum = (buffer[0] & 0x0F) + (buffer[1] & 0x0F) + (buffer[2] & 0x0F) + (buffer[3] & 0x0F) +
                      ((buffer[5] & 0xF0) >> 4) + ((buffer[6] & 0xF0) >> 4) + ((buffer[7] & 0xF0) >> 4) + 0x0A;
  return static_cast<uint8_t>(((sum & 0x0F) << 4) | (buffer[7] & 0x0F));
}

TEST(GreeClimateTest, HeatCoolHiddenWithoutHeatByDefault) {
  GreeClimate climate;
  climate.set_supports_heat(false);
  climate.set_supports_cool(true);
  climate.set_supports_heat_cool(false);
  EXPECT_FALSE(climate.get_traits().supports_mode(climate::CLIMATE_MODE_HEAT_COOL));
}

TEST(GreeClimateTest, HeatCoolOverrideAdvertisedWithoutHeat) {
  GreeClimate climate;
  climate.set_supports_heat(false);
  climate.set_supports_cool(true);
  climate.set_supports_heat_cool(true);
  auto traits = climate.get_traits();
  EXPECT_TRUE(traits.supports_mode(climate::CLIMATE_MODE_HEAT_COOL));
  EXPECT_FALSE(traits.supports_mode(climate::CLIMATE_MODE_HEAT));
}

TEST(GreeClimateTest, Yap1fSetModelOnlyAdvertisesSwingOff) {
  GreeClimate climate;
  climate.set_model(GREE_YAP1F);
  auto traits = climate.get_traits();
  EXPECT_TRUE(traits.supports_swing_mode(climate::CLIMATE_SWING_OFF));
  EXPECT_FALSE(traits.supports_swing_mode(climate::CLIMATE_SWING_VERTICAL));
  EXPECT_FALSE(traits.supports_swing_mode(climate::CLIMATE_SWING_HORIZONTAL));
  EXPECT_FALSE(traits.supports_swing_mode(climate::CLIMATE_SWING_BOTH));
}

TEST(GreeClimateTest, Yap1fSetModelDoesNotRebuildYanSwingModes) {
  // Negative control for the YAN regression: only YAP1F replaces swing_modes_.
  GreeClimate climate;
  climate.set_model(GREE_YAN);
  auto traits = climate.get_traits();
  EXPECT_TRUE(traits.supports_swing_mode(climate::CLIMATE_SWING_OFF));
  EXPECT_TRUE(traits.supports_swing_mode(climate::CLIMATE_SWING_VERTICAL));
  EXPECT_FALSE(traits.supports_swing_mode(climate::CLIMATE_SWING_HORIZONTAL));
  EXPECT_FALSE(traits.supports_swing_mode(climate::CLIMATE_SWING_BOTH));
}

TEST(GreeClimateTest, Yap1fTransmitCool24HighGoldenFrame) {
  CapturingTransmitter transmitter;
  TestableGreeClimate climate;
  climate.set_transmitter(&transmitter);
  climate.set_model(GREE_YAP1F);
  climate.mode = climate::CLIMATE_MODE_COOL;
  climate.target_temperature = 24.0f;
  climate.fan_mode = climate::CLIMATE_FAN_HIGH;
  climate.swing_mode = climate::CLIMATE_SWING_OFF;

  climate.transmit_state();

  ASSERT_FALSE(transmitter.last_data().empty());
  EXPECT_EQ(transmitter.last_carrier_frequency(), GREE_YAP_IR_FREQUENCY);

  uint8_t frame[25] = {0};
  ASSERT_TRUE(decode_yap_frame(transmitter.last_data(), frame));

  // power on + cool + fan high, temp 24C (offset from 16), light bit default, fixed bytes
  EXPECT_EQ(frame[0], static_cast<uint8_t>(GREE_FAN_3 | GREE_MODE_COOL | GREE_MODE_ON));
  EXPECT_EQ(frame[1], 8);
  EXPECT_EQ(frame[2], GREE_MODE_BIT_LIGHT);
  EXPECT_EQ(frame[3], 0x50);
  EXPECT_EQ(frame[4], 0x00);
  EXPECT_EQ(frame[5], 0xC2);
  EXPECT_EQ(frame[8], yap_checksum(frame));
  EXPECT_EQ(frame[11], 0x70);
  EXPECT_EQ(frame[16], yap_checksum(frame + 8));
  EXPECT_EQ(frame[19], 0xA0);
  EXPECT_EQ(frame[23], 0xA0);
  EXPECT_EQ(frame[24], 0x00);
}

TEST(GreeClimateTest, NonYap1fByte5UnchangedWithoutFixedVane) {
  // Negative control: non-YAP1F path still seeds remote_state[5] = 0x20 when no fixed vane.
  CapturingTransmitter transmitter;
  TestableGreeClimate climate;
  climate.set_transmitter(&transmitter);
  climate.set_model(GREE_GENERIC);
  climate.mode = climate::CLIMATE_MODE_COOL;
  climate.target_temperature = 24.0f;
  climate.fan_mode = climate::CLIMATE_FAN_AUTO;
  climate.swing_mode = climate::CLIMATE_SWING_OFF;

  climate.transmit_state();
  ASSERT_FALSE(transmitter.last_data().empty());
  EXPECT_EQ(transmitter.last_carrier_frequency(), GREE_IR_FREQUENCY);

  // Generic 8-byte frame: header + 4 bytes + 010 marker + gap + 4 bytes + final mark/space.
  const auto &timings = transmitter.last_data();
  size_t index = 0;
  ASSERT_TRUE(expect_mark_space(timings, &index));  // header
  for (int i = 0; i < 4; i++) {
    uint8_t discarded = 0;
    ASSERT_TRUE(decode_byte(timings, &index, &discarded));
  }
  for (int i = 0; i < 3; i++) {
    ASSERT_TRUE(expect_mark_space(timings, &index));  // fixed 010
  }
  ASSERT_TRUE(expect_mark_space(timings, &index));  // message gap
  uint8_t second_half[4] = {0};
  for (uint8_t &byte : second_half) {
    ASSERT_TRUE(decode_byte(timings, &index, &byte));
  }
  EXPECT_EQ(second_half[1], 0x20);
}
}  // namespace esphome::gree::testing
