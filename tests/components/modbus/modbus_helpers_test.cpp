#include <gtest/gtest.h>

#include "esphome/components/modbus/modbus_helpers.h"

namespace esphome::modbus::helpers {

using FC = ModbusFunctionCode;

// --- server_frame_length ---------------------------------------------------
// Frame layout: address(1) + function(1) + ... + CRC(2). Fixtures borrowed from
// tests/integration/fixtures/uart_mock_modbus.yaml.

TEST(ModbusServerFrameLength, TooShortReturnsMinimum) {
  const uint8_t frame[] = {0x01};
  EXPECT_EQ(server_frame_length(frame, 1), MIN_FRAME_SIZE);
}

TEST(ModbusServerFrameLength, ReadHoldingUsesByteCount) {
  // inject_rx for basic_register: 2 data bytes -> 5 + 2 = 7
  const uint8_t frame[] = {0x01, 0x03, 0x02, 0x01, 0x03, 0xF9, 0xD5};
  EXPECT_EQ(server_frame_length(frame, sizeof(frame)), 7);
}

TEST(ModbusServerFrameLength, ReadByteCountCappedAtMax) {
  const uint8_t frame[] = {0x01, 0x03, 0xFF};  // claim 255 bytes
  EXPECT_EQ(server_frame_length(frame, sizeof(frame)), 5 + MAX_NUM_OF_REGISTERS_TO_READ * 2);
}

TEST(ModbusServerFrameLength, ReadMissingByteCountReturnsHeaderOnly) {
  const uint8_t frame[] = {0x01, 0x03};
  EXPECT_EQ(server_frame_length(frame, sizeof(frame)), 5);
}

TEST(ModbusServerFrameLength, ExceptionResponse) {
  // exception_response fixture: function code 0x83 has the exception bit set
  const uint8_t frame[] = {0x01, 0x83, 0x02, 0xC0, 0xF1};
  EXPECT_EQ(server_frame_length(frame, sizeof(frame)), 5);
}

TEST(ModbusServerFrameLength, WriteResponsesAreFixed) {
  for (FC fc :
       {FC::WRITE_SINGLE_COIL, FC::WRITE_SINGLE_REGISTER, FC::WRITE_MULTIPLE_COILS, FC::WRITE_MULTIPLE_REGISTERS}) {
    const uint8_t frame[] = {0x01, static_cast<uint8_t>(fc)};
    EXPECT_EQ(server_frame_length(frame, sizeof(frame)), 8) << "fc=" << static_cast<int>(fc);
  }
}

TEST(ModbusServerFrameLength, MiscFixedAndUnknown) {
  const uint8_t mask[] = {0x01, static_cast<uint8_t>(FC::MASK_WRITE_REGISTER)};
  const uint8_t fifo[] = {0x01, static_cast<uint8_t>(FC::READ_FIFO_QUEUE)};
  const uint8_t unknown[] = {0x01, 0x42};
  EXPECT_EQ(server_frame_length(mask, sizeof(mask)), 10);
  EXPECT_EQ(server_frame_length(fifo, sizeof(fifo)), 6);
  EXPECT_EQ(server_frame_length(unknown, sizeof(unknown)), MIN_FRAME_SIZE);
}

// --- client_frame_length ---------------------------------------------------

TEST(ModbusClientFrameLength, TooShortReturnsMinimum) {
  const uint8_t frame[] = {0x01};
  EXPECT_EQ(client_frame_length(frame, 1), MIN_FRAME_SIZE);
}

TEST(ModbusClientFrameLength, ReadAndWriteSingleAreFixed) {
  // basic_register request fixture is a read-holding request -> 8 bytes
  const uint8_t read[] = {0x01, 0x03, 0x00, 0x03, 0x00, 0x01, 0x74, 0x0A};
  EXPECT_EQ(client_frame_length(read, sizeof(read)), 8);
  for (FC fc : {FC::READ_COILS, FC::READ_DISCRETE_INPUTS, FC::READ_INPUT_REGISTERS, FC::WRITE_SINGLE_COIL,
                FC::WRITE_SINGLE_REGISTER}) {
    const uint8_t frame[] = {0x01, static_cast<uint8_t>(fc)};
    EXPECT_EQ(client_frame_length(frame, sizeof(frame)), 8) << "fc=" << static_cast<int>(fc);
  }
}

TEST(ModbusClientFrameLength, WriteMultipleUsesByteCount) {
  // write 2 registers (4 data bytes): addr(2)+qty(2)+count(1) then data; count is frame[6]
  const uint8_t frame[] = {0x01, 0x10, 0x00, 0x00, 0x00, 0x02, 0x04, 0x00, 0x0B, 0x00, 0x16};
  EXPECT_EQ(client_frame_length(frame, sizeof(frame)), 9 + 4);
}

TEST(ModbusClientFrameLength, WriteMultipleByteCountCapped) {
  const uint8_t frame[] = {0x01, 0x0F, 0x00, 0x00, 0x00, 0x02, 0xFF};
  EXPECT_EQ(client_frame_length(frame, sizeof(frame)), 9 + MAX_NUM_OF_REGISTERS_TO_WRITE * 2);
}

TEST(ModbusClientFrameLength, WriteMultipleMissingByteCount) {
  const uint8_t frame[] = {0x01, 0x10, 0x00, 0x00, 0x00, 0x02};
  EXPECT_EQ(client_frame_length(frame, sizeof(frame)), 9);
}

TEST(ModbusClientFrameLength, MiscFixedAndUnknown) {
  const uint8_t mask[] = {0x01, static_cast<uint8_t>(FC::MASK_WRITE_REGISTER)};
  const uint8_t fifo[] = {0x01, static_cast<uint8_t>(FC::READ_FIFO_QUEUE)};
  const uint8_t unknown[] = {0x01, 0x42};
  EXPECT_EQ(client_frame_length(mask, sizeof(mask)), 10);
  EXPECT_EQ(client_frame_length(fifo, sizeof(fifo)), 6);
  EXPECT_EQ(client_frame_length(unknown, sizeof(unknown)), MIN_FRAME_SIZE);
}

// --- create_client_pdu -----------------------------------------------------
// PDU = function code + data (no address, no CRC).

TEST(ModbusCreateClientPdu, ReadHolding) {
  auto pdu = create_client_pdu(FC::READ_HOLDING_REGISTERS, 0x0003, 1);
  const std::vector<uint8_t> expected{0x03, 0x00, 0x03, 0x00, 0x01};
  EXPECT_EQ(std::vector<uint8_t>(pdu.begin(), pdu.end()), expected);
}

TEST(ModbusCreateClientPdu, WriteSingleOmitsQuantity) {
  const uint8_t values[] = {0x00, 0x0B};
  auto pdu = create_client_pdu(FC::WRITE_SINGLE_REGISTER, 0x0003, 1, values, sizeof(values));
  const std::vector<uint8_t> expected{0x06, 0x00, 0x03, 0x00, 0x0B};
  EXPECT_EQ(std::vector<uint8_t>(pdu.begin(), pdu.end()), expected);
}

TEST(ModbusCreateClientPdu, WriteSingleTooFewValuesReturnsEmpty) {
  const uint8_t values[] = {0x00};
  auto pdu = create_client_pdu(FC::WRITE_SINGLE_COIL, 0x0003, 1, values, sizeof(values));
  EXPECT_TRUE(pdu.empty());
}

TEST(ModbusCreateClientPdu, WriteMultipleIncludesByteCount) {
  const uint8_t values[] = {0x00, 0x0B, 0x00, 0x16};
  auto pdu = create_client_pdu(FC::WRITE_MULTIPLE_REGISTERS, 0x0000, 2, values, sizeof(values));
  const std::vector<uint8_t> expected{0x10, 0x00, 0x00, 0x00, 0x02, 0x04, 0x00, 0x0B, 0x00, 0x16};
  EXPECT_EQ(std::vector<uint8_t>(pdu.begin(), pdu.end()), expected);
}

TEST(ModbusCreateClientPdu, WriteMultipleOverCapacityReturnsEmpty) {
  std::vector<uint8_t> values(MAX_PDU_SIZE - 6 + 1, 0xAA);
  auto pdu = create_client_pdu(FC::WRITE_MULTIPLE_REGISTERS, 0x0000, 1, values.data(), values.size());
  EXPECT_TRUE(pdu.empty());
}

TEST(ModbusHelpersTest, PayloadToNumberRejectsOffsetAtEndOfBuffer) {
  const std::vector<uint8_t> data{0x12, 0x34};
  EXPECT_EQ(payload_to_number(data, SensorValueType::U_WORD, 2, 0xFFFFFFFF), 0);
}

TEST(ModbusHelpersTest, PayloadToNumberRejectsTruncatedMultiRegisterValue) {
  const std::vector<uint8_t> data{0x12, 0x34, 0x56};
  EXPECT_EQ(payload_to_number(data, SensorValueType::U_DWORD, 0, 0xFFFFFFFF), 0);
}

TEST(ModbusHelpersTest, PayloadToNumberDecodesValidWord) {
  const std::vector<uint8_t> data{0x12, 0x34};
  EXPECT_EQ(payload_to_number(data, SensorValueType::U_WORD, 0, 0xFFFFFFFF), 0x1234);
}

}  // namespace esphome::modbus::helpers
