#include <gtest/gtest.h>

#include <algorithm>
#include <array>

#include "esphome/components/time/real_time_clock.h"

namespace esphome::time::testing {

// 2026-04-16 23:59:58, a Thursday (day_of_week 5), in each layout.
static constexpr uint32_t TIMESTAMP = 1776383998;
static constexpr uint8_t EPSON_RAW[7] = {0x58, 0x59, 0x23, 0x10, 0x16, 0x04, 0x26};
static constexpr uint8_t DS_RAW[7] = {0x58, 0x59, 0x23, 0x05, 0x16, 0x04, 0x26};
static constexpr uint8_t PCF_RAW[7] = {0x58, 0x59, 0x23, 0x16, 0x04, 0x04, 0x26};

static std::array<uint8_t, 7> patched(const uint8_t (&src)[7], size_t index, uint8_t value) {
  std::array<uint8_t, 7> raw;
  std::copy(src, src + 7, raw.begin());
  raw[index] = value;
  return raw;
}

template<BcdCalendarLayout LAYOUT> static void expect_round_trip(const uint8_t (&raw)[7]) {
  const ESPTime time = decode_bcd_calendar<LAYOUT>(raw);
  EXPECT_EQ(time.day_of_week, 5);
  EXPECT_EQ(time.day_of_month, 16);
  EXPECT_EQ(time.timestamp, TIMESTAMP);
  uint8_t out[7];
  encode_bcd_calendar<LAYOUT>(time, out);
  for (size_t i = 0; i < 7; i++)
    EXPECT_EQ(out[i], raw[i]) << "byte " << i;
}

TEST(BcdCalendar, DecodesEveryField) {
  const ESPTime time = decode_bcd_calendar<EPSON_BCD_CALENDAR>(EPSON_RAW);
  EXPECT_EQ(time.second, 58);
  EXPECT_EQ(time.minute, 59);
  EXPECT_EQ(time.hour, 23);
  EXPECT_EQ(time.day_of_week, 5);
  EXPECT_EQ(time.day_of_month, 16);
  EXPECT_EQ(time.month, 4);
  EXPECT_EQ(time.year, 2026);
  EXPECT_EQ(time.timestamp, TIMESTAMP);
  EXPECT_TRUE(time.is_valid(true, false));
}

TEST(BcdCalendar, MasksStatusBits) {
  // Bit 7 of seconds/minutes, the top two bits of hour and day and the top three of month are not time.
  const uint8_t raw[7] = {0xD8, 0xD9, 0xE3, 0x90, 0xD6, 0xE4, 0x26};
  const ESPTime time = decode_bcd_calendar<EPSON_BCD_CALENDAR>(raw);
  EXPECT_EQ(time.second, 58);
  EXPECT_EQ(time.minute, 59);
  EXPECT_EQ(time.hour, 23);
  EXPECT_EQ(time.day_of_week, 5);
  EXPECT_EQ(time.day_of_month, 16);
  EXPECT_EQ(time.month, 4);
}

TEST(BcdCalendar, EmptyWeekdayDecodesAsSunday) {
  EXPECT_EQ(decode_bcd_calendar<EPSON_BCD_CALENDAR>(patched(EPSON_RAW, 3, 0).data()).day_of_week, 1);
}

TEST(BcdCalendar, EveryLayoutRoundTrips) {
  expect_round_trip<EPSON_BCD_CALENDAR>(EPSON_RAW);
  expect_round_trip<DS_BCD_CALENDAR>(DS_RAW);
  expect_round_trip<PCF_BCD_CALENDAR>(PCF_RAW);
}

TEST(BcdCalendar, DsLayoutTwelveHourMode) {
  EXPECT_EQ(decode_bcd_calendar<DS_BCD_CALENDAR>(patched(DS_RAW, 2, 0x40 | 0x20 | 0x11).data()).hour, 23);  // 11 PM
  EXPECT_EQ(decode_bcd_calendar<DS_BCD_CALENDAR>(patched(DS_RAW, 2, 0x40 | 0x12).data()).hour, 0);          // 12 AM
  EXPECT_EQ(decode_bcd_calendar<DS_BCD_CALENDAR>(patched(DS_RAW, 2, 0x40 | 0x20 | 0x12).data()).hour, 12);  // 12 PM
}

TEST(BcdCalendar, DsLayoutCenturyBit) {
  const ESPTime time = decode_bcd_calendar<DS_BCD_CALENDAR>(patched(DS_RAW, 5, 0x84).data());
  EXPECT_EQ(time.year, 2126);
  // 2100 is not a leap year, so the timestamp is one day earlier than a plain every-four-years count gives.
  EXPECT_EQ(time.timestamp, 4932057598);
  uint8_t out[7];
  encode_bcd_calendar<DS_BCD_CALENDAR>(time, out);
  EXPECT_EQ(out[5], 0x84);
  EXPECT_EQ(out[6], 0x26);
}

TEST(BcdCalendar, PcfLayoutIgnoresMonthBit7) {
  EXPECT_EQ(decode_bcd_calendar<PCF_BCD_CALENDAR>(patched(PCF_RAW, 5, 0x84).data()).year, 2026);
}

}  // namespace esphome::time::testing
