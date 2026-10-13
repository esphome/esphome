#include <gtest/gtest.h>

#include <cstring>

#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::core::testing {

PROGMEM_STRING_TABLE(SmallStrings, "OFF", "HEAT", "COOL", "UNKNOWN");

// Eight strings of 40 characters: a 328-byte blob, past what one-byte offsets can address.
PROGMEM_STRING_TABLE(LargeStrings, "0123456789012345678901234567890123456789",
                     "1123456789012345678901234567890123456789", "2123456789012345678901234567890123456789",
                     "3123456789012345678901234567890123456789", "4123456789012345678901234567890123456789",
                     "5123456789012345678901234567890123456789", "6123456789012345678901234567890123456789",
                     "7123456789012345678901234567890123456789");

TEST(ProgmemStringTable, SmallTableKeepsOneByteOffsets) {
  static_assert(SmallStrings::BLOB_SIZE <= 255);
  static_assert(sizeof(SmallStrings::OFFSETS[0]) == 1);
  static_assert(SmallStrings::MAX_LENGTH == 7);
  EXPECT_STREQ(reinterpret_cast<const char *>(SmallStrings::get_log_str(0, SmallStrings::LAST_INDEX)), "OFF");
  EXPECT_STREQ(reinterpret_cast<const char *>(SmallStrings::get_log_str(2, SmallStrings::LAST_INDEX)), "COOL");
  EXPECT_STREQ(reinterpret_cast<const char *>(SmallStrings::get_log_str(200, SmallStrings::LAST_INDEX)), "UNKNOWN");
}

TEST(ProgmemStringTable, LargeTableUsesTwoByteOffsets) {
  static_assert(LargeStrings::BLOB_SIZE > 255);
  static_assert(sizeof(LargeStrings::OFFSETS[0]) == 2);
  static_assert(LargeStrings::MAX_LENGTH == 40);
  EXPECT_STREQ(reinterpret_cast<const char *>(LargeStrings::get_progmem_str(0, LargeStrings::LAST_INDEX)),
               "0123456789012345678901234567890123456789");
  EXPECT_STREQ(reinterpret_cast<const char *>(LargeStrings::get_progmem_str(7, LargeStrings::LAST_INDEX)),
               "7123456789012345678901234567890123456789");
  EXPECT_STREQ(reinterpret_cast<const char *>(LargeStrings::get_progmem_str(8, LargeStrings::LAST_INDEX)),
               "7123456789012345678901234567890123456789");
  EXPECT_EQ(LargeStrings::OFFSETS[7], 7 * 41);
}

}  // namespace esphome::core::testing
