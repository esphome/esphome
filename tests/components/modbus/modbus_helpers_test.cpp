#include <gtest/gtest.h>

#include <memory>

#include "esphome/components/modbus/modbus_helpers.h"

namespace esphome::modbus::helpers {

using FC = FunctionCode;

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

TEST(ModbusClientFrameLength, ReadWriteMultipleByteCountCappedAtSpecLimit) {
  // FC 0x17's write byte count caps at the spec 6.17 limit of 121 registers (242 bytes), deliberately
  // tighter than FC 0x10's 123, so a corrupt byte count cannot make the parser wait past the real frame.
  const uint8_t pdu[] = {0x17, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0xFF};  // claims 255 bytes
  EXPECT_EQ(client_pdu_length(pdu, sizeof(pdu)), 10 + MAX_NUM_OF_REGISTERS_TO_WRITE_RW * 2);
}

TEST(ModbusClientFrameLength, ReadWriteMultipleUsesByteCount) {
  // read start(2) + read qty(2) + write start(2) + write qty(2) + byte count(1) then data
  const uint8_t frame[] = {0x01, 0x17, 0x9C, 0xB9, 0x00, 0x02, 0x9C, 0x41, 0x00, 0x02, 0x04, 0xAA, 0xBB, 0xCC, 0xDD};
  EXPECT_EQ(client_frame_length(frame, sizeof(frame)), 13 + 4);
}

TEST(ModbusClientFrameLength, ReadWriteMultipleMissingByteCount) {
  // header present up to the write quantity but the byte count byte (frame[10]) is absent
  const uint8_t frame[] = {0x01, 0x17, 0x9C, 0xB9, 0x00, 0x02, 0x9C, 0x41, 0x00, 0x02};
  EXPECT_EQ(client_frame_length(frame, sizeof(frame)), 13);
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

// --- file-record length cap --------------------------------------------------
// FC 0x14/0x15 are parsed only to keep the frame parser in sync; the byte count caps at 251
// (MAX_PDU_SIZE - 2), reproducing the released frame-relative bound of MAX_FRAME_SIZE - 5.

TEST(ModbusFileRecordCap, PduLengthCapsByteCountAt251) {
  const uint8_t pdu[] = {static_cast<uint8_t>(FC::READ_FILE_RECORD), 0xFF};  // claims 255 bytes
  EXPECT_EQ(server_pdu_length(pdu, sizeof(pdu)), 2 + (MAX_PDU_SIZE - 2));
  EXPECT_EQ(client_pdu_length(pdu, sizeof(pdu)), 2 + (MAX_PDU_SIZE - 2));
  // Frame wrappers: address(1) + PDU + CRC(2) stays within the RTU 256-byte frame limit.
  const uint8_t frame[] = {0x01, static_cast<uint8_t>(FC::WRITE_FILE_RECORD), 0xFF};
  EXPECT_EQ(server_frame_length(frame, sizeof(frame)), MAX_FRAME_SIZE);
  EXPECT_EQ(client_frame_length(frame, sizeof(frame)), MAX_FRAME_SIZE);
}

TEST(ModbusFileRecordCap, StandardChecksAcceptUpTo251) {
  // A full-length PDU at the cap: function(1) + byte count(1) + 251 data bytes = MAX_PDU_SIZE.
  std::vector<uint8_t> at_cap(MAX_PDU_SIZE, 0x00);
  at_cap[0] = static_cast<uint8_t>(FC::READ_FILE_RECORD);
  at_cap[1] = MAX_PDU_SIZE - 2;
  EXPECT_TRUE(is_server_pdu_standard(at_cap.data(), at_cap.size()));
  EXPECT_TRUE(is_client_pdu_standard(at_cap.data(), at_cap.size()));
  // Byte count 252 in the same 253-byte buffer: the parsed length still matches (capped), so this
  // exercises the byte-count bound itself rather than the length identity.
  at_cap[1] = MAX_PDU_SIZE - 1;
  EXPECT_FALSE(is_server_pdu_standard(at_cap.data(), at_cap.size()));
  EXPECT_FALSE(is_client_pdu_standard(at_cap.data(), at_cap.size()));
}

// --- is_client_pdu_standard / is_server_pdu_standard -------------------------
// The gatekeepers for the typed client dispatch: a PDU must be exactly its function code's standard
// shape, with byte count, quantity, and address range all consistent.

TEST(ModbusPduStandard, ClientReadAndWriteConformant) {
  const uint8_t read_regs[] = {0x03, 0x01, 0x00, 0x00, 0x02};
  EXPECT_TRUE(is_client_pdu_standard(read_regs, sizeof(read_regs)));
  const uint8_t write_regs[] = {0x10, 0x00, 0x20, 0x00, 0x02, 0x04, 0x00, 0x01, 0x00, 0x02};
  EXPECT_TRUE(is_client_pdu_standard(write_regs, sizeof(write_regs)));
  // 10 coils pack into 2 data bytes - the coil formula, not the register one.
  const uint8_t write_coils[] = {0x0F, 0x00, 0x30, 0x00, 0x0A, 0x02, 0xFF, 0x03};
  EXPECT_TRUE(is_client_pdu_standard(write_coils, sizeof(write_coils)));
}

TEST(ModbusPduStandard, ClientRejectsNonConformant) {
  // Truncated: header claims 4 data bytes, only 2 present.
  const uint8_t truncated[] = {0x10, 0x00, 0x20, 0x00, 0x02, 0x04, 0x00, 0x01};
  EXPECT_FALSE(is_client_pdu_standard(truncated, sizeof(truncated)));
  // Byte count disagrees with quantity (2 registers need 4 bytes, header says 2).
  const uint8_t inconsistent[] = {0x10, 0x00, 0x20, 0x00, 0x02, 0x02, 0x00, 0x01};
  EXPECT_FALSE(is_client_pdu_standard(inconsistent, sizeof(inconsistent)));
  // Coil write using the register byte-count formula (10 coils with 20 data bytes).
  const uint8_t coil_as_regs[] = {0x0F, 0x00, 0x30, 0x00, 0x0A, 0x14, 0, 0, 0, 0, 0, 0, 0,
                                  0,    0,    0,    0,    0,    0,    0, 0, 0, 0, 0, 0, 0};
  EXPECT_FALSE(is_client_pdu_standard(coil_as_regs, sizeof(coil_as_regs)));
  // Quantity zero and quantity beyond the per-function-code maximum.
  const uint8_t zero_qty[] = {0x03, 0x01, 0x00, 0x00, 0x00};
  EXPECT_FALSE(is_client_pdu_standard(zero_qty, sizeof(zero_qty)));
  const uint8_t too_many[] = {0x03, 0x01, 0x00, 0x00, 0x7E};  // 126 > 125
  EXPECT_FALSE(is_client_pdu_standard(too_many, sizeof(too_many)));
  // Address range overflow: 0xFFFF + 2 registers exceeds the 16-bit register space.
  const uint8_t wraps[] = {0x03, 0xFF, 0xFF, 0x00, 0x02};
  EXPECT_FALSE(is_client_pdu_standard(wraps, sizeof(wraps)));
}

TEST(ModbusPduStandard, ServerReadResponses) {
  const uint8_t ok[] = {0x03, 0x04, 0x00, 0x2A, 0x01, 0x00};
  EXPECT_TRUE(is_server_pdu_standard(ok, sizeof(ok)));
  // Byte-count header disagrees with the actual length.
  const uint8_t lying[] = {0x03, 0x06, 0x00, 0x2A, 0x01, 0x00};
  EXPECT_FALSE(is_server_pdu_standard(lying, sizeof(lying)));
  // An empty PDU (the on_error path) is not a standard response.
  EXPECT_FALSE(is_server_pdu_standard(ok, 0));
}

TEST(ModbusPduStandard, ServerResponsesRejectDegenerateShapes) {
  // A read response always carries data: byte count zero is non-conformant.
  const uint8_t zero_bc[] = {0x03, 0x00};
  EXPECT_FALSE(is_server_pdu_standard(zero_bc, sizeof(zero_bc)));
  // Registers are 2 bytes each: an odd byte count would silently truncate a register.
  const uint8_t odd_bc[] = {0x03, 0x03, 0x00, 0x01, 0x02};
  EXPECT_FALSE(is_server_pdu_standard(odd_bc, sizeof(odd_bc)));
  // Bit reads have no parity requirement: one packed byte is a fine coil response.
  const uint8_t coil_one_byte[] = {0x01, 0x01, 0x05};
  EXPECT_TRUE(is_server_pdu_standard(coil_one_byte, sizeof(coil_one_byte)));
  // A write-multiple echo claiming 65535 registers written is bounded like the request side.
  const uint8_t wild_echo[] = {0x10, 0x00, 0x00, 0xFF, 0xFF};
  EXPECT_FALSE(is_server_pdu_standard(wild_echo, sizeof(wild_echo)));
  const uint8_t ok_echo[] = {0x10, 0x00, 0x00, 0x00, 0x02};
  EXPECT_TRUE(is_server_pdu_standard(ok_echo, sizeof(ok_echo)));
}

TEST(ModbusPduStandard, SingleCoilValueMustBeCanonical) {
  // FC 0x05's value field allows exactly 0xFF00 (ON) and 0x0000 (OFF); anything else is non-standard.
  const uint8_t on[] = {0x05, 0x00, 0x10, 0xFF, 0x00};
  const uint8_t off[] = {0x05, 0x00, 0x10, 0x00, 0x00};
  const uint8_t junk[] = {0x05, 0x00, 0x10, 0x12, 0x34};
  EXPECT_TRUE(is_client_pdu_standard(on, sizeof(on)));
  EXPECT_TRUE(is_client_pdu_standard(off, sizeof(off)));
  EXPECT_FALSE(is_client_pdu_standard(junk, sizeof(junk)));
  EXPECT_TRUE(is_server_pdu_standard(on, sizeof(on)));  // the response echoes the request
  EXPECT_FALSE(is_server_pdu_standard(junk, sizeof(junk)));
}

TEST(ModbusPduStandard, NonStandardFunctionCodesAcceptedOnLengthAlone) {
  // Custom, unimplemented, and exception function codes have no standard shape to check: they are
  // accepted whenever the parsed length matches, so a dispatcher can still route them by function
  // code instead of having them rejected outright. This is the documented contract - see the header.
  const uint8_t custom[] = {0x42};  // user-defined space; 1 byte matches the MIN_PDU_SIZE fallback
  EXPECT_TRUE(is_client_pdu_standard(custom, sizeof(custom)));
  EXPECT_TRUE(is_server_pdu_standard(custom, sizeof(custom)));
  const uint8_t unimplemented[] = {0x07};  // READ_EXCEPTION_STATUS
  EXPECT_TRUE(is_server_pdu_standard(unimplemented, sizeof(unimplemented)));
  const uint8_t exception[] = {0x83, 0x02};  // exception response; length pinned to 2 bytes
  EXPECT_TRUE(is_server_pdu_standard(exception, sizeof(exception)));
  // The length identity still gates: extra bytes beyond the parsed fallback are non-conformant.
  const uint8_t custom_long[] = {0x42, 0x01};
  EXPECT_FALSE(is_client_pdu_standard(custom_long, sizeof(custom_long)));
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

// The generic write path requires the data length to agree exactly with the entity count
// (registers: 2 bytes each; coils: 8 packed per byte) - the same rule the response dispatch
// enforces via is_client_pdu_standard(), so a frame built here always passes that gate.
TEST(ModbusCreateClientPdu, WriteMultipleRejectsMismatchedDataLength) {
  const uint8_t values[] = {0x00, 0x0B, 0x00, 0x16};
  // 2 registers need exactly 4 data bytes.
  EXPECT_TRUE(create_client_pdu(FC::WRITE_MULTIPLE_REGISTERS, 0x0000, 2, values, 3).empty());
  EXPECT_FALSE(create_client_pdu(FC::WRITE_MULTIPLE_REGISTERS, 0x0000, 2, values, 4).empty());
  // 10 coils pack into exactly 2 data bytes - the coil formula, not the register one.
  EXPECT_FALSE(create_client_pdu(FC::WRITE_MULTIPLE_COILS, 0x0000, 10, values, 2).empty());
  EXPECT_TRUE(create_client_pdu(FC::WRITE_MULTIPLE_COILS, 0x0000, 10, values, 4).empty());
}

TEST(ModbusCreateClientPdu, WriteCoilsUseTheCoilLimitNotTheRegisterLimit) {
  // 200 coils: above the 123-register write limit but well within the 1968-coil limit; 25 data bytes.
  std::vector<uint8_t> values(25, 0xAA);
  auto pdu = create_client_pdu(FC::WRITE_MULTIPLE_COILS, 0x0000, 200, values.data(), values.size());
  ASSERT_FALSE(pdu.empty());
  EXPECT_EQ(pdu[5], 25);                                        // byte count uses the coil formula
  EXPECT_TRUE(is_client_pdu_standard(pdu.data(), pdu.size()));  // builder output passes the validator
  // Builder and validator agree at the top of the range too: 1969 coils rejected.
  std::vector<uint8_t> big((1969 + 7) / 8, 0x00);
  EXPECT_TRUE(create_client_pdu(FC::WRITE_MULTIPLE_COILS, 0x0000, 1969, big.data(), big.size()).empty());
}

// --- payload_to_number -----------------------------------------------------
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

TEST(ModbusHelpersTest, PayloadToNumberDecodesSwappedUnsignedWord) {
  const std::vector<uint8_t> data{0x34, 0x12};
  EXPECT_EQ(payload_to_number(std::span<const uint8_t>(data), SensorValueType::U_WORD_S, 0, 0xFFFFFFFF), 0x1234);
}

TEST(ModbusHelpersTest, PayloadToNumberDecodesSwappedSignedWord) {
  const std::vector<uint8_t> data{0xFE, 0xFF};
  EXPECT_EQ(payload_to_number(std::span<const uint8_t>(data), SensorValueType::S_WORD_S, 0, 0xFFFFFFFF), -2);
}

TEST(ModbusHelpersTest, PayloadToNumberAppliesBitmaskAfterSwap) {
  // Bytes {0x34,0x12} decode as U_WORD_S to 0x1234; mask 0xFF00 then right-shift by bit 8 -> 0x12
  const std::vector<uint8_t> data{0x34, 0x12};
  EXPECT_EQ(payload_to_number(std::span<const uint8_t>(data), SensorValueType::U_WORD_S, 0, 0xFF00), 0x12);
}

TEST(ModbusHelpersTest, PayloadToNumberAppliesBitmaskAfterSwapSigned) {
  // Bytes {0x34,0xFE} decode as S_WORD_S to 0xFE34 (negative); mask 0x00F0 then right-shift by bit 4 -> 0x3
  const std::vector<uint8_t> data{0x34, 0xFE};
  EXPECT_EQ(payload_to_number(std::span<const uint8_t>(data), SensorValueType::S_WORD_S, 0, 0x00F0), 0x3);
}

// --- registers_to_number ---------------------------------------------------
// Register words are host byte order; results must match the byte-based payload_to_number.

TEST(ModbusHelpersTest, RegistersToNumberDecodesWord) {
  const uint16_t registers[] = {0x1234};
  EXPECT_EQ(registers_to_number(registers, 1, SensorValueType::U_WORD), 0x1234);
}

TEST(ModbusHelpersTest, RegistersToNumberDecodesSwappedUnsignedWord) {
  const uint16_t registers[] = {0x3412};
  EXPECT_EQ(registers_to_number(registers, 1, SensorValueType::U_WORD_S), 0x1234);
}

TEST(ModbusHelpersTest, RegistersToNumberDecodesSwappedSignedWord) {
  const uint16_t registers[] = {0xFEFF};
  EXPECT_EQ(registers_to_number(registers, 1, SensorValueType::S_WORD_S), -2);
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

// --- packed bit helpers ------------------------------------------------------

TEST(ModbusHelpersTest, PackBitsAppendsToContainer) {
  // Bits are packed LSB first: the first value is bit 0 of the first byte, and the push_back
  // overload appends packed bytes onto a growable container preserving existing content.
  std::vector<bool> bits{true, false, true, true, false, false, false, false, true, true};
  std::vector<uint8_t> out{0x55};  // pre-existing content must be preserved
  pack_bits(out, bits);
  ASSERT_EQ(out.size(), 3u);  // leading byte + 2 packed bytes (10 bits)
  EXPECT_EQ(out[0], 0x55);
  EXPECT_EQ(out[1], 0x0D);  // 0b00001101
  EXPECT_EQ(out[2], 0x03);  // bits 8 and 9 -> bits 0,1 of second byte
}

// --- typed builders ----------------------------------------------------------

TEST(ModbusTypedBuilders, ReadPduWireBytes) {
  auto pdu = create_read_pdu(FC::READ_HOLDING_REGISTERS, 0x0102, 3);
  const std::vector<uint8_t> expected{0x03, 0x01, 0x02, 0x00, 0x03};
  EXPECT_EQ(std::vector<uint8_t>(pdu.begin(), pdu.end()), expected);
  EXPECT_TRUE(is_client_pdu_standard(pdu.data(), pdu.size()));
  // Reads that run past the 16-bit address space are refused.
  EXPECT_TRUE(create_read_pdu(FC::READ_HOLDING_REGISTERS, 0xFFFF, 2).empty());
}

TEST(ModbusTypedBuilders, WriteSinglePduWireBytes) {
  auto reg = create_write_single_register_pdu(0x0010, 0xABCD);
  const std::vector<uint8_t> expected_reg{0x06, 0x00, 0x10, 0xAB, 0xCD};
  EXPECT_EQ(std::vector<uint8_t>(reg.begin(), reg.end()), expected_reg);
  EXPECT_TRUE(is_client_pdu_standard(reg.data(), reg.size()));
  auto coil_on = create_write_single_coil_pdu(0x0011, true);
  auto coil_off = create_write_single_coil_pdu(0x0011, false);
  const std::vector<uint8_t> expected_on{0x05, 0x00, 0x11, 0xFF, 0x00};
  const std::vector<uint8_t> expected_off{0x05, 0x00, 0x11, 0x00, 0x00};
  EXPECT_EQ(std::vector<uint8_t>(coil_on.begin(), coil_on.end()), expected_on);
  EXPECT_EQ(std::vector<uint8_t>(coil_off.begin(), coil_off.end()), expected_off);
  EXPECT_TRUE(is_client_pdu_standard(coil_on.data(), coil_on.size()));
  EXPECT_TRUE(is_client_pdu_standard(coil_off.data(), coil_off.size()));
}

TEST(ModbusTypedBuilders, WriteRegistersPduWireBytes) {
  const uint16_t values[] = {0x000B, 0x0016};
  auto pdu = create_write_registers_pdu(0x0000, values);
  const std::vector<uint8_t> expected{0x10, 0x00, 0x00, 0x00, 0x02, 0x04, 0x00, 0x0B, 0x00, 0x16};
  EXPECT_EQ(std::vector<uint8_t>(pdu.begin(), pdu.end()), expected);
  EXPECT_TRUE(is_client_pdu_standard(pdu.data(), pdu.size()));
  // Writes that run past the 16-bit address space are refused.
  EXPECT_TRUE(create_write_registers_pdu(0xFFFF, values).empty());
}

TEST(ModbusTypedBuilders, WriteRegistersPduRejectsOverLimit) {
  std::vector<uint16_t> values(MAX_NUM_OF_REGISTERS_TO_WRITE + 1, 0xAAAA);
  EXPECT_TRUE(create_write_registers_pdu(0x0000, values).empty());
  values.pop_back();
  EXPECT_FALSE(create_write_registers_pdu(0x0000, values).empty());
}

TEST(ModbusTypedBuilders, ReadWriteMultipleRegistersPduWireBytes) {
  const uint16_t write_values[] = {0x000B, 0x0016};
  // Read 2 registers at 0x0010, write 2 registers at 0x0020.
  auto pdu = create_read_write_multiple_registers_pdu(0x0010, 2, 0x0020, write_values);
  const std::vector<uint8_t> expected{0x17, 0x00, 0x10, 0x00, 0x02, 0x00, 0x20,
                                      0x00, 0x02, 0x04, 0x00, 0x0B, 0x00, 0x16};
  EXPECT_EQ(std::vector<uint8_t>(pdu.begin(), pdu.end()), expected);
  EXPECT_TRUE(is_client_pdu_standard(pdu.data(), pdu.size()));
}

TEST(ModbusTypedBuilders, ReadWriteMultipleRegistersPduRejectsOutOfRange) {
  const uint16_t one_value[] = {0x0001};
  const uint16_t two_values[] = {0x0001, 0x0002};
  // Read count out of range (zero and above the read ceiling).
  EXPECT_TRUE(create_read_write_multiple_registers_pdu(0x0000, 0, 0x0020, one_value).empty());
  EXPECT_TRUE(
      create_read_write_multiple_registers_pdu(0x0000, MAX_NUM_OF_REGISTERS_TO_READ + 1, 0x0020, one_value).empty());
  // Write count out of range (empty, and above the read/write ceiling which is lower than a plain write).
  EXPECT_TRUE(create_read_write_multiple_registers_pdu(0x0000, 1, 0x0020, std::span<const uint16_t>()).empty());
  std::vector<uint16_t> too_many(MAX_NUM_OF_REGISTERS_TO_WRITE_RW + 1, 0xAAAA);
  EXPECT_TRUE(create_read_write_multiple_registers_pdu(0x0000, 1, 0x0020, too_many).empty());
  // Both blocks at their respective ceilings are accepted.
  std::vector<uint16_t> at_write_limit(MAX_NUM_OF_REGISTERS_TO_WRITE_RW, 0xAAAA);
  EXPECT_FALSE(
      create_read_write_multiple_registers_pdu(0x0000, MAX_NUM_OF_REGISTERS_TO_READ, 0x0020, at_write_limit).empty());
  // A block that runs past the 16-bit address space is refused (read block, then write block).
  EXPECT_TRUE(create_read_write_multiple_registers_pdu(0xFFFF, 2, 0x0020, one_value).empty());
  EXPECT_TRUE(create_read_write_multiple_registers_pdu(0x0000, 2, 0xFFFF, two_values).empty());
  // Accept boundary: a block ending exactly at 0x10000 (last register 0xFFFF) still fits.
  EXPECT_FALSE(create_read_write_multiple_registers_pdu(0xFFFE, 2, 0x0000, one_value).empty());  // read ends at 0x10000
  EXPECT_FALSE(
      create_read_write_multiple_registers_pdu(0x0000, 1, 0xFFFF, one_value).empty());  // write ends at 0x10000
}

TEST(ModbusFunctionCodeClass, ReadWriteMultipleCountsAsBothReadAndWrite) {
  const auto rw = static_cast<uint8_t>(FC::READ_WRITE_MULTIPLE_REGISTERS);
  // 0x17 both reads and writes, but it is not a pure (retry-safe) read.
  EXPECT_TRUE(is_function_code_read(rw));
  EXPECT_TRUE(is_function_code_write(rw));
  EXPECT_FALSE(is_function_code_read_only(rw));
  // Pure reads are read and read-only, never write.
  const auto rd = static_cast<uint8_t>(FC::READ_HOLDING_REGISTERS);
  EXPECT_TRUE(is_function_code_read(rd));
  EXPECT_TRUE(is_function_code_read_only(rd));
  EXPECT_FALSE(is_function_code_write(rd));
  // Plain writes are write only.
  const auto wr = static_cast<uint8_t>(FC::WRITE_MULTIPLE_REGISTERS);
  EXPECT_TRUE(is_function_code_write(wr));
  EXPECT_FALSE(is_function_code_read(wr));
  EXPECT_FALSE(is_function_code_read_only(wr));
  // Mask-write register mutates via read-modify-write, so it classes as a write, never a read.
  const auto mask = static_cast<uint8_t>(FC::MASK_WRITE_REGISTER);
  EXPECT_TRUE(is_function_code_write(mask));
  EXPECT_FALSE(is_function_code_read(mask));
  EXPECT_FALSE(is_function_code_read_only(mask));
}

TEST(ModbusCreateClientPdu, ReadWriteMultipleReturnsEmpty) {
  // The generic builder cannot express 0x17's two blocks; callers use the dedicated builder instead.
  const uint16_t values[] = {0x0001};
  EXPECT_TRUE(create_client_pdu(FC::READ_WRITE_MULTIPLE_REGISTERS, 0x0000, 1, reinterpret_cast<const uint8_t *>(values),
                                sizeof(values))
                  .empty());
}

TEST(ModbusTypedBuilders, FloatToPayloadAppendsToExistingContent) {
  // The container overload appends - the semantic every migrated caller relies on when a lambda
  // has already put words into the buffer.
  std::vector<uint16_t> data{0x1234};
  float_to_payload(data, 1.0f, SensorValueType::U_WORD);
  ASSERT_EQ(data.size(), 2u);
  EXPECT_EQ(data[0], 0x1234);
  EXPECT_EQ(data[1], 0x0001);
}

// --- number_to_payload -----------------------------------------------------

TEST(ModbusHelpersTest, NumberToPayloadRoundTripsSwappedUnsignedWord) {
  std::vector<uint16_t> regs;
  number_to_payload(regs, 0x1234, SensorValueType::U_WORD_S);
  ASSERT_EQ(regs.size(), 1u);
  EXPECT_EQ(regs[0], 0x3412);
  EXPECT_EQ(registers_to_number(regs.data(), regs.size(), SensorValueType::U_WORD_S), 0x1234);
}

TEST(ModbusHelpersTest, NumberToPayloadRoundTripsSwappedSignedWord) {
  std::vector<uint16_t> regs;
  number_to_payload(regs, -2, SensorValueType::S_WORD_S);
  ASSERT_EQ(regs.size(), 1u);
  EXPECT_EQ(regs[0], 0xFEFF);
  EXPECT_EQ(registers_to_number(regs.data(), regs.size(), SensorValueType::S_WORD_S), -2);
}

TEST(ModbusCreateClientPdu, ExceptionFlaggedWriteCodesRejected) {
  // is_function_code_write() masks the exception bit; the builder must not.
  const uint8_t values[] = {0x00, 0x0B, 0x00, 0x16};
  EXPECT_TRUE(create_client_pdu(FunctionCode(0x90), 0x0000, 2, values, 4).empty());
  EXPECT_TRUE(create_client_pdu(FunctionCode(0x85), 0x0000, 1, values, 2).empty());
}

TEST(ModbusTypedBuilders, BoolSpanCoilBuilderRejectsOverLimit) {
  // This early guard is what keeps the 246-byte packing buffer from overflowing - the shared core's
  // identical check runs after packing, so it cannot protect it.
  auto big = std::make_unique<bool[]>(MAX_NUM_OF_COILS_TO_WRITE + 1);
  EXPECT_TRUE(create_write_coils_pdu(0, std::span<const bool>(big.get(), MAX_NUM_OF_COILS_TO_WRITE + 1)).empty());
}

TEST(ModbusCreateClientPdu, GenericCoilWriteMasksTrailingPadBits) {
  // 10 coils with junk in the pad bits of the last data byte: the generic path masks them like the
  // typed builder, so both produce identical wire bytes.
  const uint8_t values[] = {0xFF, 0xFF};
  auto pdu = create_client_pdu(FC::WRITE_MULTIPLE_COILS, 0x0000, 10, values, 2);
  ASSERT_FALSE(pdu.empty());
  EXPECT_EQ(pdu[pdu.size() - 1], 0x03);  // bits 8-9 kept, pad bits 10-15 zeroed
}

TEST(ModbusCreateClientPdu, SingleCoilValueValidated) {
  const uint8_t on[] = {0xFF, 0x00};
  const uint8_t junk[] = {0x01, 0x00};
  EXPECT_FALSE(create_client_pdu(FC::WRITE_SINGLE_COIL, 0x0003, 1, on, 2).empty());
  EXPECT_TRUE(create_client_pdu(FC::WRITE_SINGLE_COIL, 0x0003, 1, junk, 2).empty());
}

// --- create_write_coils_pdu (packed) ---------------------------------------

TEST(ModbusWriteCoilsPacked, MatchesBoolBuilder) {
  const bool coils[] = {true, false, true, true, false, false, true, false, true, true};
  uint8_t packed[] = {0b01001101, 0b00000011};
  auto from_bools = create_write_coils_pdu(0x13, coils);
  auto from_packed = create_write_coils_pdu(0x13, PackedBits(packed, 10));
  ASSERT_EQ(from_packed.size(), from_bools.size());
  EXPECT_EQ(0, memcmp(from_packed.data(), from_bools.data(), from_bools.size()));
}

TEST(ModbusWriteCoilsPacked, MasksUnusedTrailingBits) {
  uint8_t packed[] = {0xFF};
  auto pdu = create_write_coils_pdu(0, PackedBits(packed, 3));
  ASSERT_EQ(pdu.size(), 7u);
  EXPECT_EQ(pdu[6], 0x07);
}

TEST(ModbusWriteCoilsPacked, RejectsShortBufferAndZeroCount) {
  uint8_t packed[] = {0xFF};
  EXPECT_TRUE(create_write_coils_pdu(0, PackedBits(packed, 9)).empty());  // needs 2 bytes
  EXPECT_TRUE(create_write_coils_pdu(0, PackedBits(packed, 0)).empty());
}

TEST(ModbusHelpersTest, PackedBitsReadsLsbFirst) {
  const uint8_t packed[] = {0x0D, 0x03};  // bits 0,2,3 and 8,9
  PackedBits bits(packed, 11);
  EXPECT_EQ(bits.size(), 11u);
  EXPECT_TRUE(bits[0]);
  EXPECT_FALSE(bits[1]);
  EXPECT_TRUE(bits[2]);
  EXPECT_TRUE(bits[3]);
  EXPECT_FALSE(bits[7]);
  EXPECT_TRUE(bits[8]);
  EXPECT_TRUE(bits[9]);
  EXPECT_FALSE(bits[10]);
  EXPECT_EQ(bits.bytes().size(), 2u);
}

TEST(ModbusHelpersTest, MutablePackedBitsSetsAndClears) {
  uint8_t packed[2] = {0x00, 0xFF};
  MutablePackedBits bits(packed, 16);
  bits.set(0, true);
  bits.set(3, true);
  bits.set(9, false);
  EXPECT_EQ(packed[0], 0x09);  // bits 0 and 3
  EXPECT_EQ(packed[1], 0xFD);  // bit 9 (bit 1 of byte 1) cleared
}

TEST(ModbusHelpersTest, MutablePackedBitsRoundTripAndConversion) {
  const bool original[] = {true, true, false, true, false, false, false, false, true, false, true};
  constexpr uint16_t count = sizeof(original);
  uint8_t packed[(count + 7) / 8] = {};
  MutablePackedBits out(packed, count);
  for (uint16_t i = 0; i != count; i++)
    out.set(i, original[i]);
  PackedBits view = out;  // implicit conversion to the read-only view
  ASSERT_EQ(view.size(), count);
  for (uint16_t i = 0; i != count; i++)
    EXPECT_EQ(view[i], original[i]) << "bit " << i;
}

TEST(ModbusHelpersTest, PackedBitsViewContractsEnforced) {
  uint8_t buf[8] = {};
  PackedBits view(buf, 10);  // 10 bits -> 2 bytes, over an 8-byte buffer
  EXPECT_EQ(view.bytes().size(), 2u);

  MutablePackedBits bits(std::span<uint8_t>(buf, 2), 10);
  bits.set(9, true);    // in range: lands in byte 1
  bits.set(10, true);   // out of range: dropped
  bits.set(300, true);  // far out of range: dropped, no write past the span

  MutablePackedBits short_bits(std::span<uint8_t>(buf, 1), 10);  // contract-violating: 10 bits over 1 byte
  short_bits.set(9, false);  // within count_ but past the span: dropped (would clear bit 9 set above)
  EXPECT_EQ(buf[1], 0x02);
  for (size_t i = 2; i < sizeof(buf); i++)
    EXPECT_EQ(buf[i], 0) << "byte " << i;
}

// server_pdu_payload() must never classify an exception PDU as a read: [fc|0x80, code] is 2 bytes, and a
// read-offset of 2 would return an empty span, losing the exception code. The payload of an exception PDU
// is the exception code byte, for reads and writes alike.
TEST(ModbusServerPduPayload, ExceptionOfReadYieldsExceptionCode) {
  const uint8_t pdu[] = {0x83, 0x02};  // exception response to READ_HOLDING_REGISTERS
  auto payload = server_pdu_payload(pdu);
  ASSERT_EQ(payload.size(), 1u);
  EXPECT_EQ(payload[0], 0x02);
}

TEST(ModbusServerPduPayload, ExceptionOfWriteYieldsExceptionCode) {
  const uint8_t pdu[] = {0x86, 0x03};  // exception response to WRITE_SINGLE_REGISTER
  auto payload = server_pdu_payload(pdu);
  ASSERT_EQ(payload.size(), 1u);
  EXPECT_EQ(payload[0], 0x03);
}

}  // namespace esphome::modbus::helpers
