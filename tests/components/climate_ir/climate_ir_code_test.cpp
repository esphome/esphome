#include <gtest/gtest.h>
#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome::climate_ir::testing {

static constexpr IRCode FANS[] PROGMEM = {
    {climate::CLIMATE_FAN_AUTO, 0x10},
    {climate::CLIMATE_FAN_LOW, 0x20},
    {climate::CLIMATE_FAN_HIGH, 0x30},
};

TEST(ClimateIRCodeTest, FindByValue) {
  const auto entry = find_ir_code(FANS, climate::CLIMATE_FAN_LOW, false);
  ASSERT_TRUE(entry.has_value());
  EXPECT_EQ(entry->value, climate::CLIMATE_FAN_LOW);
  EXPECT_EQ(entry->code, 0x20);
}

TEST(ClimateIRCodeTest, FindByCode) {
  const auto entry = find_ir_code(FANS, 0x30, true);
  ASSERT_TRUE(entry.has_value());
  EXPECT_EQ(entry->value, climate::CLIMATE_FAN_HIGH);
}

TEST(ClimateIRCodeTest, FindMissing) {
  EXPECT_FALSE(find_ir_code(FANS, climate::CLIMATE_FAN_MEDIUM, false).has_value());
  // A code that is also a listed value must not match when looking up codes
  EXPECT_FALSE(find_ir_code(FANS, climate::CLIMATE_FAN_LOW, true).has_value());
}

TEST(ClimateIRCodeTest, ToCode) {
  EXPECT_EQ(to_ir_code(FANS, climate::CLIMATE_FAN_HIGH), 0x30);
  EXPECT_EQ(to_ir_code(FANS, climate::CLIMATE_FAN_MEDIUM), 0x10);
}

TEST(ClimateIRCodeTest, FromCode) {
  EXPECT_EQ(from_ir_code<climate::ClimateFanMode>(FANS, 0x20), climate::CLIMATE_FAN_LOW);
  EXPECT_EQ(from_ir_code<climate::ClimateFanMode>(FANS, 0x99), climate::CLIMATE_FAN_AUTO);
}

}  // namespace esphome::climate_ir::testing
