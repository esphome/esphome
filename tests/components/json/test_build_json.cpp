#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "esphome/components/json/json_util.h"

using esphome::json::build_json;
using esphome::json::json_build_t;
using esphome::json::JsonBuildRef;

namespace {

// NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
void build_plain(JsonObject root) { root["plain"] = true; }
bool build_returns_bool(JsonObject root) { return root["kept"].set(true); }
void build_const_ref(const JsonObject &root) { root["ref"] = true; }

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

// Builders json_build_t accepted before still build, through the std::function overload
TEST(BuildJson, OtherBuildersStillBuild) {
  EXPECT_EQ(std::string(build_json(build_returns_bool)), R"({"kept":true})");
  EXPECT_EQ(std::string(build_json(build_const_ref)), R"({"ref":true})");
  const auto const_counter = [n = 0](JsonObject root) mutable { root["n"] = ++n; };
  EXPECT_EQ(std::string(build_json(const_counter)), R"({"n":1})");
  EXPECT_EQ(std::string(build_json(const_counter)), R"({"n":1})");  // copied each call, as before
  static_assert(!esphome::json::JsonBuildCallable<const decltype(const_counter) &>);
}

// Callables the fast path takes too: an rvalue JsonObject parameter, and a deleted operator&
struct NoAddressBuilder {
  void operator()(JsonObject root) const { root["no_address"] = true; }
  void operator&() const = delete;  // NOLINT(google-runtime-operator): the case under test
};

TEST(BuildJson, UnusualCallablesTakeTheFastPath) {
  EXPECT_EQ(std::string(build_json([](JsonObject &&root) { root["rvalue"] = true; })), R"({"rvalue":true})");
  const NoAddressBuilder no_address;
  EXPECT_EQ(std::string(build_json(no_address)), R"({"no_address":true})");
}
// NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

}  // namespace
