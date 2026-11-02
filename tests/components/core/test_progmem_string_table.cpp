#include <gtest/gtest.h>

#include <cstring>

#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::core::testing {

PROGMEM_STRING_TABLE(SmallStrings, "OFF", "HEAT", "COOL", "UNKNOWN");

// Eight strings of 40 characters: a 328-byte blob, past what one-byte offsets can address, so it is scanned.
PROGMEM_STRING_TABLE(LargeStrings, "0123456789012345678901234567890123456789",
                     "1123456789012345678901234567890123456789", "2123456789012345678901234567890123456789",
                     "3123456789012345678901234567890123456789", "4123456789012345678901234567890123456789",
                     "5123456789012345678901234567890123456789", "6123456789012345678901234567890123456789",
                     "7123456789012345678901234567890123456789");

// Five 50-character strings pack into exactly 255 bytes; one more byte switches to scanning.
PROGMEM_STRING_TABLE(Exactly255, "01234567890123456789012345678901234567890123456789",
                     "01234567890123456789012345678901234567890123456789",
                     "01234567890123456789012345678901234567890123456789",
                     "01234567890123456789012345678901234567890123456789",
                     "01234567890123456789012345678901234567890123456789");
PROGMEM_STRING_TABLE(Exactly256, "01234567890123456789012345678901234567890123456789",
                     "01234567890123456789012345678901234567890123456789",
                     "01234567890123456789012345678901234567890123456789",
                     "01234567890123456789012345678901234567890123456789",
                     "01234567890123456789012345678901234567890123456789x");

TEST(ProgmemStringTable, ScansInsteadOfIndexingAfter255Bytes) {
  static_assert(Exactly255::BLOB_SIZE == 255);
  static_assert(Exactly255::Table::INDEXED);
  static_assert(Exactly256::BLOB_SIZE == 256);
  static_assert(!Exactly256::Table::INDEXED);
  static_assert(Exactly256::OFFSETS.empty());
  EXPECT_EQ(Exactly255::OFFSETS[4], 204);
  EXPECT_STREQ(reinterpret_cast<const char *>(Exactly256::get_progmem_str(4, 0)),
               "01234567890123456789012345678901234567890123456789x");
}

TEST(ProgmemStringTable, SmallTableKeepsOneByteOffsets) {
  static_assert(SmallStrings::BLOB_SIZE <= 255);
  static_assert(SmallStrings::Table::INDEXED);
  static_assert(SmallStrings::MAX_LENGTH == 7);
  EXPECT_STREQ(reinterpret_cast<const char *>(SmallStrings::get_log_str(0, SmallStrings::LAST_INDEX)), "OFF");
  EXPECT_STREQ(reinterpret_cast<const char *>(SmallStrings::get_log_str(2, SmallStrings::LAST_INDEX)), "COOL");
  EXPECT_STREQ(reinterpret_cast<const char *>(SmallStrings::get_log_str(200, SmallStrings::LAST_INDEX)), "UNKNOWN");
}

TEST(ProgmemStringTable, LargeTableIsScanned) {
  static_assert(LargeStrings::BLOB_SIZE > 255);
  static_assert(!LargeStrings::Table::INDEXED);
  static_assert(LargeStrings::MAX_LENGTH == 40);
  EXPECT_STREQ(reinterpret_cast<const char *>(LargeStrings::get_progmem_str(0, LargeStrings::LAST_INDEX)),
               "0123456789012345678901234567890123456789");
  EXPECT_STREQ(reinterpret_cast<const char *>(LargeStrings::get_progmem_str(7, LargeStrings::LAST_INDEX)),
               "7123456789012345678901234567890123456789");
  EXPECT_STREQ(reinterpret_cast<const char *>(LargeStrings::get_progmem_str(8, LargeStrings::LAST_INDEX)),
               "7123456789012345678901234567890123456789");
  EXPECT_STREQ(reinterpret_cast<const char *>(LargeStrings::get_progmem_str(3, LargeStrings::LAST_INDEX)),
               "3123456789012345678901234567890123456789");
}

TEST(ProgmemStringTable, StrstrFindsTableEntryInRamString) {
  const char *line = "MODE HEAT ON";
  const auto *heat = reinterpret_cast<ESPHOME_PGM_P>(SmallStrings::get_progmem_str(1, SmallStrings::LAST_INDEX));
  const auto *cool = reinterpret_cast<ESPHOME_PGM_P>(SmallStrings::get_progmem_str(2, SmallStrings::LAST_INDEX));
  EXPECT_EQ(ESPHOME_strstr_P(line, heat), line + 5);
  EXPECT_EQ(ESPHOME_strstr_P(line, cool), nullptr);
}

}  // namespace esphome::core::testing
