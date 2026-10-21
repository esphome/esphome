#include <gtest/gtest.h>

#include <string>

#include "esphome/components/json/json_util.h"
#include "esphome/core/string_ref.h"

using esphome::ProgmemStringRef;
using esphome::StringRef;

TEST(JsonLinked, WholeViewsSerializeAsTheirStrings) {
  static constexpr char ICON[] = "mdi:thermometer";
  JsonDocument doc;
  doc["ram"] = esphome::json::linked(StringRef(ICON));
  doc["flash"] = esphome::json::linked(ProgmemStringRef::from_lit(ICON));
  std::string out;
  serializeJson(doc, out);
  EXPECT_EQ(out, R"({"ram":"mdi:thermometer","flash":"mdi:thermometer"})");
}

TEST(JsonLinked, EmptyViewsSerializeAsEmptyStrings) {
  JsonDocument doc;
  doc["ram"] = esphome::json::linked(StringRef());
  doc["flash"] = esphome::json::linked(ProgmemStringRef());
  std::string out;
  serializeJson(doc, out);
  EXPECT_EQ(out, R"({"ram":"","flash":""})");
}
