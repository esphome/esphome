#include <gtest/gtest.h>

#include <cstddef>
#include <string>

#include "esphome/components/json/json_util.h"
#include "esphome/core/string_ref.h"

using esphome::ProgmemStringRef;
using esphome::StringRef;
using esphome::json::JsonBuilder;

namespace {

std::string serialize(JsonBuilder &builder) {
  char out[128];
  size_t len = builder.serialize_to(out, sizeof(out));
  EXPECT_LT(len, sizeof(out));
  return std::string(out, len);
}

}  // namespace

TEST(JsonLinked, WholeViewsSerializeAsTheirStrings) {
  static constexpr char ICON[] = "mdi:thermometer";
  JsonBuilder builder;
  JsonObject root = builder.root();
  root["ram"] = esphome::json::linked(StringRef(ICON));
  root["flash"] = esphome::json::linked(ProgmemStringRef::from_lit(ICON));
  EXPECT_EQ(serialize(builder), R"({"ram":"mdi:thermometer","flash":"mdi:thermometer"})");
}

TEST(JsonLinked, EmptyViewsSerializeAsEmptyStrings) {
  JsonBuilder builder;
  JsonObject root = builder.root();
  root["ram"] = esphome::json::linked(StringRef());
  root["flash"] = esphome::json::linked(ProgmemStringRef());
  EXPECT_EQ(serialize(builder), R"({"ram":"","flash":""})");
}
