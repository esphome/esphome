#include <gtest/gtest.h>

#include <cstring>

#include "esphome/components/modbus_tcp/mbap.h"

namespace {

using esphome::modbus_tcp::Mbap;
using esphome::modbus_tcp::MbapTake;
using esphome::modbus_tcp::rtu_crc_ok;
using esphome::modbus_tcp::take_mbap;
using esphome::modbus_tcp::write_mbap;

TEST(MbapTest, RoundTrip) {
  const uint8_t pdu[] = {0x03, 0x00, 0x00, 0x00, 0x01};
  uint8_t frame[16];
  size_t n = write_mbap(frame, sizeof(frame), 0x1234, 0x11, pdu, sizeof(pdu));
  ASSERT_EQ(n, 7u + sizeof(pdu));

  Mbap out;
  size_t used = 0;
  ASSERT_EQ(take_mbap(frame, n, &out, &used), MbapTake::FRAME);
  EXPECT_EQ(used, n);
  EXPECT_EQ(out.txn, 0x1234);
  EXPECT_EQ(out.unit, 0x11);
  EXPECT_EQ(out.pdu_len, sizeof(pdu));
  EXPECT_EQ(std::memcmp(out.pdu, pdu, sizeof(pdu)), 0);
}

TEST(MbapTest, ShortBufferNeedsMore) {
  uint8_t frame[8] = {};
  Mbap out;
  size_t used = 99;
  EXPECT_EQ(take_mbap(frame, 6, &out, &used), MbapTake::NEED_MORE);
  EXPECT_EQ(used, 0u);

  // Length says 5 more bytes, but only the header is here.
  uint8_t short_body[8] = {0x00, 0x01, 0x00, 0x00, 0x00, 0x05, 0x11, 0x03};
  used = 99;
  EXPECT_EQ(take_mbap(short_body, sizeof(short_body), &out, &used), MbapTake::NEED_MORE);
  EXPECT_EQ(used, 0u);
}

TEST(MbapTest, RejectsALengthOutsideTheSpec) {
  Mbap out;
  size_t used = 99;
  uint8_t too_small[7] = {0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x11};
  EXPECT_EQ(take_mbap(too_small, sizeof(too_small), &out, &used), MbapTake::BAD);
  EXPECT_EQ(used, 1u);

  uint8_t too_big[7] = {0x00, 0x01, 0x00, 0x00, 0x00, 0xFF, 0x11};
  used = 0;
  EXPECT_EQ(take_mbap(too_big, sizeof(too_big), &out, &used), MbapTake::BAD);
  EXPECT_EQ(used, 1u);
}

TEST(MbapTest, RtuCrc) {
  const uint8_t frame[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0x84, 0x0A};
  EXPECT_TRUE(rtu_crc_ok(frame, sizeof(frame)));
  EXPECT_FALSE(rtu_crc_ok(frame, 3));
  EXPECT_FALSE(rtu_crc_ok(frame, sizeof(frame) - 1));
}

TEST(MbapTest, BadProtocolDropsOneByte) {
  uint8_t frame[8] = {0, 1, 0, 1, 0, 2, 1, 3};
  Mbap out;
  size_t used = 0;
  EXPECT_EQ(take_mbap(frame, sizeof(frame), &out, &used), MbapTake::BAD);
  EXPECT_EQ(used, 1u);
}

TEST(MbapTest, WriteRejectsAnEmptyPdu) {
  uint8_t frame[8];
  EXPECT_EQ(write_mbap(frame, sizeof(frame), 1, 1, frame, 0), 0u);
}

}  // namespace
