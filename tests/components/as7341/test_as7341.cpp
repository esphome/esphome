#include <gtest/gtest.h>

#include <array>

#include "esphome/components/as7341/as7341.h"

namespace esphome::as7341::testing {

static constexpr uint8_t ADDRESS = 0x39;
static constexpr uint8_t SMUXEN = 1 << 4;
static constexpr uint8_t AVALID = 1 << 6;
static constexpr uint8_t NUM_ADC_CHANNELS = 6;

// The chip's registers behind an address pointer that moves on after every byte.
// SMUX commands finish at once and spectral data is always valid.
class RegisterBus final : public i2c::I2CBus {
 public:
  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                             size_t out_size) override {
    EXPECT_EQ(address, ADDRESS);
    if (size != 0) {
      this->pointer_ = data[0];
      for (size_t i = 1; i < size; i++)
        this->registers[this->pointer_++] = data[i];
    }
    this->registers[AS7341_ENABLE] &= ~SMUXEN;
    for (size_t i = 0; i < out_size; i++)
      out[i] = this->registers[this->pointer_++];
    return i2c::ERROR_OK;
  }

  std::array<uint8_t, 256> registers{};

 protected:
  uint8_t pointer_{0};
};

TEST(AS7341Test, ReadChannelsDecodesLowByteFirst) {
  RegisterBus bus;
  AS7341Component sensor;
  sensor.set_i2c_bus(&bus);
  sensor.set_i2c_address(ADDRESS);
  bus.registers[AS7341_STATUS2] = AVALID;

  // CHn_DATA_L holds the low byte and CHn_DATA_H, one address up, the high byte.
  for (uint8_t channel = 0; channel < NUM_ADC_CHANNELS; channel++) {
    bus.registers[AS7341_CH0_DATA_L + 2 * channel] = 0x34 + channel;
    bus.registers[AS7341_CH0_DATA_H + 2 * channel] = 0x12;
  }

  std::array<uint16_t, 2 * NUM_ADC_CHANNELS> data{};
  ASSERT_TRUE(sensor.read_channels(data.data()));
  for (uint8_t i = 0; i < data.size(); i++)
    EXPECT_EQ(data[i], 0x1234 + i % NUM_ADC_CHANNELS) << "value " << unsigned(i);
}

}  // namespace esphome::as7341::testing
