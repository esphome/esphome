#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "esphome/components/json/json_util.h"

using esphome::json::build_json;
using esphome::json::json_build_t;
using esphome::json::JsonBuildRef;

namespace {

void build_plain(JsonObject root) { root["plain"] = true; }

// 16 bytes of captures on 32-bit, past what std::function keeps inline
TEST(BuildJson, LargeCaptureLambdaMatchesStdFunction) {
  uint32_t a = 1, b = 2, c = 3;
  const char *name = "cover";
  auto fill = [a, b, c, name](JsonObject root) {
    root["a"] = a;
    root["b"] = b;
    root["c"] = c;
    root["name"] = name;
  };
  static_assert(sizeof(fill) > 8);
  const json_build_t as_function = fill;
  const std::string expected = R"({"a":1,"b":2,"c":3,"name":"cover"})";
  EXPECT_EQ(std::string(build_json(fill)), expected);
  EXPECT_EQ(std::string(build_json(as_function)), expected);
  EXPECT_EQ(std::string(build_json([&](JsonObject root) { fill(root); })), expected);
}

TEST(BuildJson, CallableKindsAllBuild) {
  int calls = 0;
  json_build_t mutable_function = [&calls](JsonObject root) { root["n"] = ++calls; };
  EXPECT_EQ(std::string(build_json(mutable_function)), R"({"n":1})");
  EXPECT_EQ(std::string(build_json(json_build_t(mutable_function))), R"({"n":2})");

  auto counter = [n = 0](JsonObject root) mutable { root["n"] = ++n; };
  EXPECT_EQ(std::string(build_json(counter)), R"({"n":1})");
  EXPECT_EQ(std::string(build_json(counter)), R"({"n":2})");  // ran in place, not on a copy

  EXPECT_EQ(std::string(build_json(build_plain)), R"({"plain":true})");
  EXPECT_EQ(std::string(build_json(&build_plain)), R"({"plain":true})");

  const JsonBuildRef ref = esphome::json::json_build_ref(counter);
  EXPECT_EQ(std::string(build_json(ref)), R"({"n":3})");
}

}  // namespace
