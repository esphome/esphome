#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/ccs811/ccs811.h"

namespace esphome::ccs811::testing {
namespace {

constexpr uint8_t ADDRESS = 0x5A;
constexpr uint8_t STATUS_DATA_READY = 1 << 3;
constexpr uint8_t ENV_DATA_REG = 0x05;

// Reports data ready, reads every other register as zero and keeps the last ENV_DATA write.
class FakeBus final : public i2c::I2CBus {
 public:
  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                             size_t out_size) override {
    EXPECT_EQ(address, ADDRESS);
    if (size == 0)
      return i2c::ERROR_OK;
    if (data[0] == ENV_DATA_REG && size == 5)
      this->env_data.assign(data + 1, data + size);
    for (size_t i = 0; i < out_size; i++)
      out[i] = data[0] == 0x00 ? STATUS_DATA_READY : 0;
    return i2c::ERROR_OK;
  }

  std::vector<uint8_t> env_data;
};

class CCS811EnvDataTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->ccs811_.set_i2c_bus(&this->bus_);
    this->ccs811_.set_i2c_address(ADDRESS);
    this->ccs811_.set_humidity_sensor(&this->humidity_);
    this->ccs811_.set_temperature_sensor(&this->temperature_);
  }

  std::vector<uint8_t> send_(float humidity, float temperature) {
    this->humidity_.state = humidity;
    this->temperature_.state = temperature;
    this->ccs811_.update();
    return this->bus_.env_data;
  }

  FakeBus bus_;
  sensor::Sensor humidity_;
  sensor::Sensor temperature_;
  CCS811Component ccs811_;
};

// ENV_DATA holds each value in 1/512 steps, temperature offset by 25 °C; 50 %RH and 25 °C are both 0x6400.
}  // namespace

TEST_F(CCS811EnvDataTest, DatasheetDefaultsEncodeExactly) {
  EXPECT_EQ(this->send_(50.0f, 25.0f), (std::vector<uint8_t>{0x64, 0x00, 0x64, 0x00}));
}

TEST_F(CCS811EnvDataTest, FractionsRoundToNearestStep) {
  // 42.348 %RH = 21682.2 steps, 20.5 °C + 25 = 23296 steps
  EXPECT_EQ(this->send_(42.348f, 20.5f), (std::vector<uint8_t>{0x54, 0xB2, 0x5B, 0x00}));
  // 33.9995 %RH = 17407.7 steps rounds to 34 %RH; -0.0005 °C + 25 = 12799.7 steps rounds to 25 °C
  EXPECT_EQ(this->send_(33.9995f, -0.0005f), (std::vector<uint8_t>{0x44, 0x00, 0x32, 0x00}));
}

TEST_F(CCS811EnvDataTest, MissingValuesUseDefaults) {
  EXPECT_EQ(this->send_(NAN, NAN), (std::vector<uint8_t>{0x64, 0x00, 0x64, 0x00}));
}

}  // namespace esphome::ccs811::testing
