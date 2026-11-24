#include <gtest/gtest.h>

#include "esphome/components/canopen/automation.h"
#include "esphome/components/canopen/types.h"

namespace esphome::canopen::testing {

static SDOReply reply(uint32_t value, uint8_t size) {
  return SDOReply{SDOResult::SDO_RESULT_SUCCESS, 0, value, size, nullptr, 0};
}

TEST(CanopenSDOReply, Unsigned) {
  EXPECT_EQ(reply(0xFF, 1).as<uint8_t>(), 0xFF);
  EXPECT_EQ(reply(0xFFFF, 2).as<uint16_t>(), 0xFFFF);
  EXPECT_EQ(reply(0xA59A08F5, 4).as<uint32_t>(), 0xA59A08F5);
}

TEST(CanopenSDOReply, SignExtension) {
  EXPECT_EQ(reply(0xFF, 1).as<int8_t>(), -1);
  EXPECT_EQ(reply(0x80, 1).as<int32_t>(), -128);
  EXPECT_EQ(reply(0x7F, 1).as<int32_t>(), 127);
  EXPECT_EQ(reply(0xFFFE, 2).as<int16_t>(), -2);
  EXPECT_EQ(reply(0x8000, 2).as<int32_t>(), -32768);
  EXPECT_EQ(reply(0xFFFFFFFF, 4).as<int32_t>(), -1);
}

TEST(CanopenSDOReply, Float) { EXPECT_FLOAT_EQ(reply(0x3FC00000, 4).as<float>(), 1.5f); }

struct Seen {
  uint32_t value{0};
  SDOResult error{SDOResult::SDO_RESULT_SUCCESS};
};
static Seen &seen() {
  static Seen s;
  return s;
}
static void on_value(const uint32_t &x) { seen().value = x; }
static void on_error(const SDOResult &result, const uint32_t & /*abort_code*/) { seen().error = result; }

TEST(CanopenSDOReply, ReadReplyRunsTheMatchingAutomation) {
  seen() = {};
  sdo_read_reply<on_value, on_error>(nullptr, reply(42, 4));
  EXPECT_EQ(seen().value, 42u);
  sdo_read_reply<on_value, on_error>(nullptr, SDOReply{SDOResult::SDO_RESULT_TIMEOUT, 0x05040000, 0, 0, nullptr, 0});
  EXPECT_EQ(seen().error, SDOResult::SDO_RESULT_TIMEOUT);
  // an automation that isn't configured is skipped
  sdo_read_reply<nullptr, on_error>(nullptr, reply(7, 4));
  EXPECT_EQ(seen().value, 42u);
}

}  // namespace esphome::canopen::testing
