#include <gtest/gtest.h>

#include <cstring>

#include "esphome/components/modbus_tcp_uart/mbap.h"

#ifdef USE_HOST

namespace esphome::modbus_tcp_uart::testing {
namespace {

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

TEST(MbapTest, WriteRtuAddsUnitAndCrc) {
  const uint8_t pdu[] = {0x03, 0x00, 0x00, 0x00, 0x01};
  uint8_t frame[16];
  ASSERT_EQ(write_rtu(frame, 0x01, pdu, sizeof(pdu)), sizeof(pdu) + 3);
  const uint8_t want[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0x84, 0x0A};
  EXPECT_EQ(std::memcmp(frame, want, sizeof(want)), 0);
  EXPECT_TRUE(rtu_crc_ok(frame, sizeof(want)));
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

TEST(MbapTest, FrameLengthByFunctionCode) {
  // Unit, function code and CRC are 4 bytes of each frame; the function code gives the rest.
  struct Row {
    uint8_t function;
    bool reply;
    size_t length;
  };
  const Row rows[] = {
      {0x03, false, 8},   // read: address and quantity
      {0x06, false, 8},   // write one register: address and value
      {0x06, true, 8},    // its reply echoes the request
      {0x10, true, 8},    // reply to a write of several registers: address and quantity
      {0x14, false, 5},   // file record: a byte count, 0 until it arrives
      {0x16, false, 10},  // mask write: address, AND mask and OR mask
      {0x17, false, 13},  // read/write: two addresses, two quantities and a byte count, 0 until it arrives
      {0x17, true, 5},    // its reply is like a read reply
      {0x18, false, 6},   // read FIFO queue: the pointer address
      {0x18, true, 6},    // its reply: a 2-byte byte count, 0 until it arrives
      {0x83, true, 5},    // exception reply to a read: the exception code
      {0x41, false, 0},   // a code without a known length: take_rtu looks for its CRC
  };
  for (const Row &row : rows) {
    const uint8_t start[] = {0x01, row.function};
    EXPECT_EQ(rtu_frame_length(start, sizeof(start), row.reply), row.length)
        << "function " << static_cast<int>(row.function);
  }
}

TEST(MbapTest, FrameLengthAddsTheByteCount) {
  // Reply to a read of two registers: byte count 4, so 5 + 4 bytes.
  const uint8_t read_reply[] = {0x01, 0x03, 0x04};
  EXPECT_EQ(rtu_frame_length(read_reply, sizeof(read_reply), true), 9u);
  // Until the byte count arrives it counts as 0.
  EXPECT_EQ(rtu_frame_length(read_reply, 2, true), 5u);
  // Write of two registers: address, quantity and byte count 4 come first, so 9 + 4 bytes.
  const uint8_t write_request[] = {0x01, 0x10, 0x00, 0x00, 0x00, 0x02, 0x04};
  EXPECT_EQ(rtu_frame_length(write_request, sizeof(write_request), false), 13u);
  // Read/write that writes two registers: both addresses, both quantities and byte count 4 first, so 13 + 4 bytes.
  const uint8_t read_write_request[] = {0x01, 0x17, 0x00, 0x00, 0x00, 0x02, 0x00, 0x10, 0x00, 0x02, 0x04};
  EXPECT_EQ(rtu_frame_length(read_write_request, sizeof(read_write_request), false), 17u);
}

TEST(MbapTest, TakeRtuUsesTheLengthOfTheFunctionCode) {
  // 01 03 02 12 34 B5 33: a reply to a one-register read, then the start of the next frame.
  const uint8_t two[] = {0x01, 0x03, 0x02, 0x12, 0x34, 0xB5, 0x33, 0x01, 0x03};
  size_t len = 0;
  EXPECT_EQ(take_rtu(two, 1, true, &len), RtuTake::NEED_MORE);
  EXPECT_EQ(take_rtu(two, 6, true, &len), RtuTake::NEED_MORE);
  ASSERT_EQ(take_rtu(two, sizeof(two), true, &len), RtuTake::FRAME);
  EXPECT_EQ(len, 7u);
  // The same bytes as a request: function 3 asks for 8 bytes, and the CRC does not match there.
  EXPECT_EQ(take_rtu(two, sizeof(two), false, &len), RtuTake::BAD);
}

TEST(MbapTest, TakeRtuFindsAnExceptionReply) {
  uint8_t frame[5] = {0x04, 0x83, 0x02};
  const uint16_t crc = crc16(frame, 3);
  frame[3] = crc & 0xFF;
  frame[4] = crc >> 8;
  size_t len = 0;
  ASSERT_EQ(take_rtu(frame, sizeof(frame), true, &len), RtuTake::FRAME);
  EXPECT_EQ(len, 5u);
}

TEST(MbapTest, TakeRtuEndsAnUnknownFunctionAtItsCrc) {
  uint8_t frame[7] = {0x01, 0x41, 0xAA, 0xBB, 0, 0, 0x99};
  const uint16_t crc = crc16(frame, 4);
  frame[4] = crc & 0xFF;
  frame[5] = crc >> 8;
  size_t len = 0;
  EXPECT_EQ(take_rtu(frame, 5, false, &len), RtuTake::NEED_MORE);
  ASSERT_EQ(take_rtu(frame, sizeof(frame), false, &len), RtuTake::FRAME);
  EXPECT_EQ(len, 6u);
}

TEST(MbapTest, TakeRtuTakesTheZeroThatEndsACrc) {
  // Function 0x41 has no known length. With the data 00 10 its CRC ends in 0x00, the last byte on the wire.
  uint8_t frame[6] = {0x01, 0x41, 0x00, 0x10};
  const uint16_t crc = crc16(frame, 4);
  frame[4] = crc & 0xFF;
  frame[5] = crc >> 8;
  ASSERT_EQ(crc >> 8, 0);
  // So the CRC already matches one byte before the end.
  ASSERT_TRUE(rtu_crc_ok(frame, sizeof(frame) - 1));
  // Taken whole, as a reply and as a request.
  size_t len = 0;
  ASSERT_EQ(take_rtu(frame, sizeof(frame), true, &len), RtuTake::FRAME);
  EXPECT_EQ(len, sizeof(frame));
  ASSERT_EQ(take_rtu(frame, sizeof(frame), false, &len), RtuTake::FRAME);
  EXPECT_EQ(len, sizeof(frame));
}

TEST(MbapTest, TakeRtuTakesStrayZerosIntoTheFrame) {
  // Frame 02 49 02 AA BB has CRC 0x8FD4, sent low byte first: D4 8F. Two stray 0x00 follow in the same read.
  const uint8_t bytes[] = {0x02, 0x49, 0x02, 0xAA, 0xBB, 0xD4, 0x8F, 0x00, 0x00};
  ASSERT_EQ(crc16(bytes, 5), 0x8FD4);
  // A 0x00 behind a frame keeps the CRC at 0, so it checks after 7, 8 and 9 bytes. The last match ends the frame:
  // all 9 bytes, as a reply and as a request.
  size_t len = 0;
  ASSERT_EQ(take_rtu(bytes, sizeof(bytes), true, &len), RtuTake::FRAME);
  EXPECT_EQ(len, 9u);
  ASSERT_EQ(take_rtu(bytes, sizeof(bytes), false, &len), RtuTake::FRAME);
  EXPECT_EQ(len, 9u);
}

TEST(MbapTest, TakeRtuGoesOnPastAMatchAtFourBytes) {
  // Frame 02 49 C1 has CRC 0x0026, sent low byte first: 26 00. One stray 0x00 follows in the same read.
  const uint8_t bytes[] = {0x02, 0x49, 0xC1, 0x26, 0x00, 0x00};
  ASSERT_EQ(crc16(bytes, 3), 0x0026);
  // Its CRC ends in 0x00: after the low byte 26 the running CRC equals the high byte 00, so it already checks after
  // 4 bytes, the first length the scan tries, and again after 5 and 6. The last match ends the frame: all 6 bytes.
  size_t len = 0;
  ASSERT_EQ(take_rtu(bytes, sizeof(bytes), true, &len), RtuTake::FRAME);
  EXPECT_EQ(len, 6u);
}

TEST(MbapTest, TakeRtuSplitsTwoFramesAfterTheZerosBetweenThem) {
  // Two replies in one read: 02 49 02 AA BB with CRC D4 8F, two stray 0x00, then 03 49 01 CC with CRC D0 73.
  const uint8_t bytes[] = {0x02, 0x49, 0x02, 0xAA, 0xBB, 0xD4, 0x8F, 0x00, 0x00, 0x03, 0x49, 0x01, 0xCC, 0xD0, 0x73};
  // Both 0x00 keep the CRC at 0, so the first frame takes them: 9 bytes. The address 03 then moves the CRC off 0,
  // and no later length checks.
  size_t len = 0;
  ASSERT_EQ(take_rtu(bytes, sizeof(bytes), true, &len), RtuTake::FRAME);
  EXPECT_EQ(len, 9u);
  // The second reply then starts the buffer and is taken on its own: 6 bytes.
  ASSERT_EQ(take_rtu(bytes + 9, sizeof(bytes) - 9, true, &len), RtuTake::FRAME);
  EXPECT_EQ(len, 6u);
}

TEST(MbapTest, TakeRtuReadsTheByteCountOfAFifoReply) {
  // The Read FIFO Queue reply of [APP 6.18]: byte count 6, FIFO count 2, values 0x01B8 and 0x1284, CRC.
  const uint8_t reply[] = {0x01, 0x18, 0x00, 0x06, 0x00, 0x02, 0x01, 0xB8, 0x12, 0x84, 0x19, 0x18};
  size_t len = 0;
  // Unit, function code, the 2-byte byte count and CRC are 6 bytes; with the 6 counted bytes the reply has 12.
  EXPECT_EQ(take_rtu(reply, 6, true, &len), RtuTake::NEED_MORE);
  ASSERT_EQ(take_rtu(reply, sizeof(reply), true, &len), RtuTake::FRAME);
  EXPECT_EQ(len, 12u);
}

}  // namespace
}  // namespace esphome::modbus_tcp_uart::testing

#endif  // USE_HOST
