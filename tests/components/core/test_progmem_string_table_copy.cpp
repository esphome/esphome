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

}  // namespace esphome::testing
