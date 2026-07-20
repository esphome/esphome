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

TEST(ModbusCreateClientPdu, UnsupportedFunctionCodeReturnsEmpty) {
  auto pdu = create_client_pdu(FC::READ_FIFO_QUEUE, 0x0000, 1);
  EXPECT_TRUE(pdu.empty());
}

TEST(ModbusCreateClientPdu, ZeroEntitiesReturnsEmpty) {
  auto pdu = create_client_pdu(FC::READ_HOLDING_REGISTERS, 0x0000, 0);
  EXPECT_TRUE(pdu.empty());
}

TEST(ModbusCreateClientPdu, WriteWithoutValuesReturnsEmpty) {
  auto pdu = create_client_pdu(FC::WRITE_MULTIPLE_REGISTERS, 0x0000, 1, nullptr, 0);
  EXPECT_TRUE(pdu.empty());
}

TEST(ModbusCreateClientPdu, ReadHoldingOverMaxReturnsEmpty) {
  auto pdu = create_client_pdu(FC::READ_HOLDING_REGISTERS, 0x0000, MAX_NUM_OF_REGISTERS_TO_READ + 1);
  EXPECT_TRUE(pdu.empty());
}

// Regression: coils allow up to 2000 entities, well above the 125 register limit.
// A switch fall-through previously subjected coil/discrete reads to the register limit.
TEST(ModbusCreateClientPdu, ReadCoilsAboveRegisterLimitIsValid) {
  const uint16_t quantity = MAX_NUM_OF_REGISTERS_TO_READ + 1;  // 126: valid for coils, too many for registers
  auto pdu = create_client_pdu(FC::READ_COILS, 0x0000, quantity);
  const std::vector<uint8_t> expected{0x01, 0x00, 0x00, static_cast<uint8_t>(quantity >> 8),
                                      static_cast<uint8_t>(quantity & 0xFF)};
  EXPECT_EQ(std::vector<uint8_t>(pdu.begin(), pdu.end()), expected);
}

TEST(ModbusCreateClientPdu, ReadCoilsOverMaxReturnsEmpty) {
  auto pdu = create_client_pdu(FC::READ_COILS, 0x0000, MAX_NUM_OF_COILS_TO_READ + 1);
  EXPECT_TRUE(pdu.empty());
}

TEST(ModbusCreateClientPdu, ReadDiscreteInputsOverMaxReturnsEmpty) {
  auto pdu = create_client_pdu(FC::READ_DISCRETE_INPUTS, 0x0000, MAX_NUM_OF_DISCRETE_INPUTS_TO_READ + 1);
  EXPECT_TRUE(pdu.empty());
}

TEST(ModbusCreateClientPdu, WriteMultipleOverEntityLimitReturnsEmpty) {
  const uint8_t values[] = {0x00, 0x0B};
  auto pdu = create_client_pdu(FC::WRITE_MULTIPLE_REGISTERS, 0x0000, MAX_NUM_OF_REGISTERS_TO_WRITE + 1, values,
                               sizeof(values));
  EXPECT_TRUE(pdu.empty());
}

TEST(ModbusHelpersTest, PayloadToNumberRejectsOffsetAtEndOfBuffer) {
  const std::vector<uint8_t> data{0x12, 0x34};
  EXPECT_FALSE(payload_to_number(std::span<const uint8_t>(data), SensorValueType::U_WORD, 2, 0xFFFFFFFF).has_value());
}

TEST(ModbusHelpersTest, PayloadToNumberRejectsTruncatedMultiRegisterValue) {
  const std::vector<uint8_t> data{0x12, 0x34, 0x56};
  EXPECT_FALSE(payload_to_number(std::span<const uint8_t>(data), SensorValueType::U_DWORD, 0, 0xFFFFFFFF).has_value());
}

TEST(ModbusHelpersTest, PayloadToNumberDecodesValidWord) {
  const std::vector<uint8_t> data{0x12, 0x34};
  EXPECT_EQ(payload_to_number(std::span<const uint8_t>(data), SensorValueType::U_WORD, 0, 0xFFFFFFFF), 0x1234);
}

// --- registers_to_number ---------------------------------------------------
// Register words are host byte order; results must match the byte-based payload_to_number.

TEST(ModbusHelpersTest, RegistersToNumberDecodesWord) {
  const uint16_t registers[] = {0x1234};
  EXPECT_EQ(registers_to_number(registers, 1, SensorValueType::U_WORD), 0x1234);
}

TEST(ModbusHelpersTest, RegistersToNumberDecodesDwordHighWordFirst) {
  const uint16_t registers[] = {0x1234, 0x5678};
  EXPECT_EQ(registers_to_number(registers, 2, SensorValueType::U_DWORD), 0x12345678);
}

TEST(ModbusHelpersTest, RegistersToNumberDecodesAtSpanStart) {
  // The function decodes the value at the start of the span; the caller advances the pointer.
  const uint16_t registers[] = {0xAAAA, 0x1234};
  EXPECT_EQ(registers_to_number(registers + 1, 1, SensorValueType::U_WORD), 0x1234);
}

TEST(ModbusHelpersTest, RegistersToNumberMatchesPayloadToNumber) {
  // Same value via both decoders: registers (host order) vs big-endian bytes.
  const uint16_t registers[] = {0x8001, 0x0002};
  const std::vector<uint8_t> bytes{0x80, 0x01, 0x00, 0x02};
  for (auto value_type : {SensorValueType::S_DWORD, SensorValueType::U_DWORD, SensorValueType::S_DWORD_R}) {
    EXPECT_EQ(registers_to_number(registers, 2, value_type),
              payload_to_number(std::span<const uint8_t>(bytes), value_type, 0, 0xFFFFFFFF))
        << "value_type=" << static_cast<int>(value_type);
  }
}

TEST(ModbusHelpersTest, RegistersToNumberRejectsTruncatedMultiRegisterValue) {
  const uint16_t registers[] = {0x1234};
  EXPECT_FALSE(registers_to_number(registers, 1, SensorValueType::U_DWORD).has_value());
}

}  // namespace esphome::modbus::helpers
