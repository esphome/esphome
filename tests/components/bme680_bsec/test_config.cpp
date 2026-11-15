#include <gtest/gtest.h>

#include <tuple>

#include "esphome/components/bme680_bsec/bme680_bsec.h"

// The unit test build defines USE_BSEC and builds against the library double in __sdk; clang-tidy does not.
#ifdef USE_BSEC

namespace esphome::bme680_bsec::testing {
namespace {

// Sample rate, supply voltage, and the first byte of the blob __init__.py writes for that pair.
using ConfigCase = std::tuple<SampleRate, SupplyVoltage, uint8_t>;

class BSECConfigTest : public ::testing::TestWithParam<ConfigCase> {};

TEST_P(BSECConfigTest, SetupPassesTheSelectedBlob) {
  auto [sample_rate, supply_voltage, first] = GetParam();
  bsec_double::config_size = 0;
  std::memset(bsec_double::config, 0xEE, sizeof(bsec_double::config));

  BME680BSECComponent component;
  component.set_sample_rate(sample_rate);
  component.set_supply_voltage(supply_voltage);
  component.setup();

  // The whole blob, copied out of the selected table, padded with zeros to the blob size
  EXPECT_EQ(bsec_double::config_size, BSEC_MAX_PROPERTY_BLOB_SIZE);
  EXPECT_EQ(bsec_double::config[0], first);
  EXPECT_EQ(bsec_double::config[1], first + 1);
  EXPECT_EQ(bsec_double::config[2], first + 2);
  for (size_t i = 3; i < BSEC_MAX_PROPERTY_BLOB_SIZE; i++)
    EXPECT_EQ(bsec_double::config[i], 0) << "byte " << i;
}

INSTANTIATE_TEST_SUITE_P(AllBlobs, BSECConfigTest,
                         ::testing::Values(ConfigCase{SAMPLE_RATE_ULP, SUPPLY_VOLTAGE_3V3, 0x10},
                                           ConfigCase{SAMPLE_RATE_ULP, SUPPLY_VOLTAGE_1V8, 0x20},
                                           ConfigCase{SAMPLE_RATE_LP, SUPPLY_VOLTAGE_3V3, 0x30},
                                           ConfigCase{SAMPLE_RATE_LP, SUPPLY_VOLTAGE_1V8, 0x40}));

}  // namespace
}  // namespace esphome::bme680_bsec::testing

#endif  // USE_BSEC
