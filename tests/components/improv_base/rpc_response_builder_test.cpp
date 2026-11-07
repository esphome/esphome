#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <improv.h>

namespace esphome::improv_base::testing {

namespace {

std::vector<uint8_t> build_with_builder(improv::Command command, const std::vector<std::string> &datum,
                                        bool add_checksum) {
  std::array<uint8_t, improv::RPC_RESPONSE_MAX_SIZE> buf;
  improv::RpcResponseBuilder builder(buf, command);
  for (const auto &str : datum) {
    EXPECT_TRUE(builder.add_string(str.c_str(), str.size()));
  }
  auto out = builder.finish(add_checksum);
  return {out.begin(), out.end()};
}

}  // namespace

// The serial path sends builder output where build_rpc_response bytes went before,
// so the two must match exactly, including the trailing 0x00 when checksums are off.
TEST(RpcResponseBuilder, ByteIdenticalToBuildRpcResponse) {
  const std::vector<std::string> device_info = {"ESPHome", "2026.9.0", "ESP32", "test-device"};
  const std::vector<std::string> network = {"MySSID", "-67", "YES"};
  const std::vector<std::string> empty = {};
  const std::vector<std::string> max_payload = {std::string(254, 'x')};

  for (bool add_checksum : {false, true}) {
    for (const auto *datum : {&device_info, &network, &empty, &max_payload}) {
      EXPECT_EQ(build_with_builder(improv::GET_DEVICE_INFO, *datum, add_checksum),
                improv::build_rpc_response(improv::GET_DEVICE_INFO, *datum, add_checksum));
    }
  }
}

// Golden bytes independent of the library: command, data length, string entries,
// then the trailing byte (0x00 without checksum, additive checksum with).
TEST(RpcResponseBuilder, GoldenBytes) {
  EXPECT_EQ(build_with_builder(improv::GET_WIFI_NETWORKS, {}, false), (std::vector<uint8_t>{0x04, 0x00, 0x00}));
  EXPECT_EQ(build_with_builder(improv::GET_WIFI_NETWORKS, {"ab"}, false),
            (std::vector<uint8_t>{0x04, 0x03, 0x02, 'a', 'b', 0x00}));
  // Checksum: 0x04 + 0x03 + 0x02 + 'a' + 'b' = 0xCC
  EXPECT_EQ(build_with_builder(improv::GET_WIFI_NETWORKS, {"ab"}, true),
            (std::vector<uint8_t>{0x04, 0x03, 0x02, 'a', 'b', 0xCC}));
}

// esp32_improv calls finish() and build_rpc_response() with no checksum flag,
// so the two defaults must agree
TEST(RpcResponseBuilder, DefaultChecksumFlagMatches) {
  const std::vector<std::string> urls = {"https://example.com"};
  std::array<uint8_t, improv::RPC_RESPONSE_MAX_SIZE> buf;
  improv::RpcResponseBuilder builder(buf, improv::WIFI_SETTINGS);
  for (const auto &str : urls) {
    EXPECT_TRUE(builder.add_string(str.c_str(), str.size()));
  }
  auto out = builder.finish();
  EXPECT_EQ(std::vector<uint8_t>(out.begin(), out.end()), improv::build_rpc_response(improv::WIFI_SETTINGS, urls));
}

TEST(RpcResponseBuilder, PayloadBudget) {
  std::array<uint8_t, improv::RPC_RESPONSE_MAX_SIZE> buf;

  // 254 byte string fills the payload exactly; a second entry no longer fits
  improv::RpcResponseBuilder full(buf, improv::GET_DEVICE_INFO);
  const std::string big(254, 'x');
  EXPECT_TRUE(full.add_string(big.c_str(), big.size()));
  EXPECT_FALSE(full.add_string("y", 1));

  // 255 byte string can never fit (its length byte would exceed the budget)
  improv::RpcResponseBuilder over(buf, improv::GET_DEVICE_INFO);
  const std::string too_big(255, 'y');
  EXPECT_FALSE(over.add_string(too_big.c_str(), too_big.size()));
  // A wildly out of range length must not wrap the position arithmetic
  EXPECT_FALSE(over.add_string("z", static_cast<size_t>(-1)));
  auto out = over.finish(false);
  EXPECT_EQ(std::vector<uint8_t>(out.begin(), out.end()), (std::vector<uint8_t>{0x03, 0x00, 0x00}));
}

TEST(RpcResponseBuilder, FinishIsIdempotent) {
  std::array<uint8_t, improv::RPC_RESPONSE_MAX_SIZE> buf;
  improv::RpcResponseBuilder builder(buf, improv::GET_DEVICE_INFO);
  EXPECT_TRUE(builder.add_string("abc", 3));
  auto first = builder.finish(true);
  const std::vector<uint8_t> expected(first.begin(), first.end());

  EXPECT_FALSE(builder.add_string("late", 4));
  auto again = builder.finish(true);
  EXPECT_EQ(std::vector<uint8_t>(again.begin(), again.end()), expected);
  // The checksum flag on a later call is ignored
  auto no_checksum = builder.finish(false);
  EXPECT_EQ(std::vector<uint8_t>(no_checksum.begin(), no_checksum.end()), expected);
}

}  // namespace esphome::improv_base::testing
