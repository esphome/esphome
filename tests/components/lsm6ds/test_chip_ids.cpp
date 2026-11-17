#include <gtest/gtest.h>

#include <cstring>

#include "esphome/components/lsm6ds/lsm6ds.h"

namespace esphome::lsm6ds::testing {

// WHO_AM_I values and temperature sensitivities from ST's datasheets:
// LSM6DS3 DocID026899, LSM6DS3TR-C DocID030071, LSM6DSO DS12140

static const char *name_of(uint8_t who_am_i) {
  const LogString *name = chip_name_from_who_am_i(who_am_i);
  return name == nullptr ? nullptr : LOG_STR_ARG(name);
}

TEST(LSM6DSChipIds, Lsm6ds3) {
  EXPECT_STREQ(name_of(0x69), "LSM6DS3");
  EXPECT_EQ(temperature_sensitivity_from_who_am_i(0x69), 16);
}

TEST(LSM6DSChipIds, Lsm6ds3trC) {
  EXPECT_STREQ(name_of(0x6A), "LSM6DS3TR-C");
  EXPECT_EQ(temperature_sensitivity_from_who_am_i(0x6A), 256);
}

TEST(LSM6DSChipIds, Lsm6dso) {
  EXPECT_STREQ(name_of(0x6C), "LSM6DSO");
  EXPECT_EQ(temperature_sensitivity_from_who_am_i(0x6C), 256);
}

TEST(LSM6DSChipIds, UnknownIsRejected) {
  EXPECT_EQ(name_of(0x00), nullptr);
  EXPECT_EQ(name_of(0x6B), nullptr);
  EXPECT_EQ(name_of(0xFF), nullptr);
}

}  // namespace esphome::lsm6ds::testing
