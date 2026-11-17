#include <gtest/gtest.h>

#include <algorithm>

#include "esphome/components/cstxxx/cstxxx.h"

namespace esphome::cstxxx::testing {

TEST(CSTxxxDecodeTouch, FirstRecordStartsTheReport) {
  const uint8_t report[report_size(1)] = {0x16, 0x12, 0x34, 0x56, 0x78, 0x01, SYNC_BYTE};
  auto touch = decode_touch(report, 0);
  EXPECT_EQ(touch.id, 1);
  EXPECT_EQ(touch.event, 0x06);
  EXPECT_EQ(touch.x, 0x125);
  EXPECT_EQ(touch.y, 0x346);
  EXPECT_EQ(touch.z, 0x78);
}

TEST(CSTxxxDecodeTouch, LaterRecordsSkipTheStatusBytes) {
  uint8_t report[report_size(3)]{};
  report[FINGER_COUNT_IDX] = 3;
  report[SYNC_IDX] = SYNC_BYTE;
  const uint8_t third[5] = {0x26, 0xFF, 0x00, 0xF0, 0x05};
  std::copy(third, third + 5, report + 12);
  auto touch = decode_touch(report, 2);
  EXPECT_EQ(touch.id, 2);
  EXPECT_EQ(touch.x, 0xFFF);
  EXPECT_EQ(touch.y, 0x000);
  EXPECT_EQ(touch.z, 5);
}

}  // namespace esphome::cstxxx::testing
