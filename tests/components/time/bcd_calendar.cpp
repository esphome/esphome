#include <gtest/gtest.h>

#include <algorithm>

#include "esphome/components/time/real_time_clock.h"

namespace esphome::time::testing {

// 2026-04-16 23:59:58, a Thursday (day_of_week 5, one-hot bit 4).
static constexpr uint8_t RAW[7] = {0x58, 0x59, 0x23, 0x10, 0x16, 0x04, 0x26};

TEST(BcdCalendar, DecodesEveryField) {
  const ESPTime time = decode_bcd_calendar(RAW);
  EXPECT_EQ(time.second, 58);
  EXPECT_EQ(time.minute, 59);
  EXPECT_EQ(time.hour, 23);
  EXPECT_EQ(time.day_of_week, 5);
  EXPECT_EQ(time.day_of_month, 16);
  EXPECT_EQ(time.month, 4);
  EXPECT_EQ(time.year, 2026);
  EXPECT_EQ(time.timestamp, 1776383998);
  EXPECT_TRUE(time.is_valid(true, false));
}

TEST(BcdCalendar, MasksStatusBits) {
  // Bit 7 of seconds/minutes, the top two bits of hour and day and the top three of month are not time.
  const uint8_t raw[7] = {0xD8, 0xD9, 0xE3, 0x90, 0xD6, 0xE4, 0x26};
  const ESPTime time = decode_bcd_calendar(raw);
  EXPECT_EQ(time.second, 58);
  EXPECT_EQ(time.minute, 59);
  EXPECT_EQ(time.hour, 23);
  EXPECT_EQ(time.day_of_week, 5);
  EXPECT_EQ(time.day_of_month, 16);
  EXPECT_EQ(time.month, 4);
}

TEST(BcdCalendar, EmptyWeekdayDecodesAsSunday) {
  uint8_t raw[7];
  std::copy(RAW, RAW + 7, raw);
  raw[3] = 0;
  EXPECT_EQ(decode_bcd_calendar(raw).day_of_week, 1);
}

TEST(BcdCalendar, EncodeRoundTrips) {
  uint8_t raw[7];
  encode_bcd_calendar(decode_bcd_calendar(RAW), raw);
  for (size_t i = 0; i < 7; i++)
    EXPECT_EQ(raw[i], RAW[i]) << "byte " << i;
}

}  // namespace esphome::time::testing
