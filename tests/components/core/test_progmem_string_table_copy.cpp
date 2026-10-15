#include <gtest/gtest.h>

#include <cstring>

#include "esphome/core/progmem.h"

namespace esphome::testing {

PROGMEM_STRING_TABLE(Names, "light.turn_on", "entity_id", "rgb_color");

TEST(ProgmemStringTableCopy, HandsOutEveryString) {
  ProgmemStringTableCopy<Names> strings;
  EXPECT_STREQ(strings.get(0), "light.turn_on");
  EXPECT_STREQ(strings.get(1), "entity_id");
  EXPECT_STREQ(strings.get(2), "rgb_color");
}

// The ESP8266 variant, built here with a plain memcpy behind progmem_memcpy
TEST(ProgmemStringTableStackCopy, HandsOutEveryString) {
  ProgmemStringTableStackCopy<Names> strings;
  EXPECT_STREQ(strings.get(0), "light.turn_on");
  EXPECT_STREQ(strings.get(1), "entity_id");
  EXPECT_STREQ(strings.get(2), "rgb_color");
}

TEST(ProgmemStringTableStackCopy, StringsLiveInTheCopy) {
  ProgmemStringTableStackCopy<Names> strings;
  const char *key = strings.get(1);
  const char *other = strings.get(2);
  EXPECT_EQ(std::strlen(key), 9U);
  EXPECT_STREQ(key, "entity_id");
  EXPECT_STREQ(other, "rgb_color");
  // The pointers go into the object, not into the table
  const char *object_start = reinterpret_cast<const char *>(&strings);
  EXPECT_GE(key, object_start);
  EXPECT_LT(key, object_start + sizeof(strings));
}

TEST(ProgmemStringTableFind, FindsWholeStringsOnly) {
  EXPECT_EQ(progmem_string_table_find<Names>("entity_id", 9), 1);
  EXPECT_EQ(progmem_string_table_find<Names>("rgb_color", 9), 2);
  EXPECT_EQ(progmem_string_table_find<Names>("entity", 6), -1);
  EXPECT_EQ(progmem_string_table_find<Names>("entity_id_x", 11), -1);
  EXPECT_EQ(progmem_string_table_find<Names>("", 0), -1);
}

// 130 strings, 520 bytes: over the one-byte offsets, so the blob is scanned
PROGMEM_STRING_TABLE(Big, "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "s12", "s13", "s14",
                     "s15", "s16", "s17", "s18", "s19", "s20", "s21", "s22", "s23", "s24", "s25", "s26", "s27", "s28",
                     "s29", "s30", "s31", "s32", "s33", "s34", "s35", "s36", "s37", "s38", "s39", "s40", "s41", "s42",
                     "s43", "s44", "s45", "s46", "s47", "s48", "s49", "s50", "s51", "s52", "s53", "s54", "s55", "s56",
                     "s57", "s58", "s59", "s60", "s61", "s62", "s63", "s64", "s65", "s66", "s67", "s68", "s69", "s70",
                     "s71", "s72", "s73", "s74", "s75", "s76", "s77", "s78", "s79", "s80", "s81", "s82", "s83", "s84",
                     "s85", "s86", "s87", "s88", "s89", "s90", "s91", "s92", "s93", "s94", "s95", "s96", "s97", "s98",
                     "s99", "s100", "s101", "s102", "s103", "s104", "s105", "s106", "s107", "s108", "s109", "s110",
                     "s111", "s112", "s113", "s114", "s115", "s116", "s117", "s118", "s119", "s120", "s121", "s122",
                     "s123", "s124", "s125", "s126", "s127", "s128", "s129");

TEST(ProgmemStringTableFind, ReachesEveryIndexOfAScannedTable) {
  EXPECT_EQ(progmem_string_table_find<Big>("s0", 2), 0);
  EXPECT_EQ(progmem_string_table_find<Big>("s127", 4), 127);
  EXPECT_EQ(progmem_string_table_find<Big>("s128", 4), 128);
  EXPECT_EQ(progmem_string_table_find<Big>("s129", 4), 129);
  EXPECT_EQ(progmem_string_table_find<Big>("s130", 4), -1);
}

}  // namespace esphome::testing
