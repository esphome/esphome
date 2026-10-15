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

TEST(ProgmemStringTableCopy, StringsStayValidWhileTheCopyLives) {
  ProgmemStringTableCopy<Names> strings;
  const char *key = strings.get(1);
  const char *other = strings.get(2);
  EXPECT_EQ(std::strlen(key), 9U);
  EXPECT_STREQ(key, "entity_id");
  EXPECT_STREQ(other, "rgb_color");
}

TEST(ProgmemStringTableFind, FindsWholeStringsOnly) {
  EXPECT_EQ(progmem_string_table_find<Names>("entity_id", 9), 1);
  EXPECT_EQ(progmem_string_table_find<Names>("rgb_color", 9), 2);
  EXPECT_EQ(progmem_string_table_find<Names>("entity", 6), -1);
  EXPECT_EQ(progmem_string_table_find<Names>("entity_id_x", 11), -1);
  EXPECT_EQ(progmem_string_table_find<Names>("", 0), -1);
}

}  // namespace esphome::testing
