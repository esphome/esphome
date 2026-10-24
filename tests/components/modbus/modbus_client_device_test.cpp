#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "esphome/components/modbus/modbus.h"

namespace esphome::modbus::testing {

namespace {

// Records every typed callback so tests can assert on the dispatch performed by the default
// on_response()/on_error() implementations.
class RecordingDevice : public ModbusClientDevice {
 public:
  struct ReadRegistersCall {
    uint16_t start_address;
    std::vector<uint16_t> registers;
    ResponseStatus status;
  };
  struct ReadBitsCall {
    uint16_t start_address;
    uint16_t count;
    std::vector<uint8_t> packed;
    ResponseStatus status;
  };
  struct WriteCall {
    uint16_t address;
    uint16_t value;
    ResponseStatus status;
  };

  void on_read_holding_registers(uint16_t start_address, std::span<const uint16_t> registers,
                                 ResponseStatus status) override {
    this->holding_calls.push_back({start_address, {registers.begin(), registers.end()}, status});
  }
  void on_read_input_registers(uint16_t start_address, std::span<const uint16_t> registers,
                               ResponseStatus status) override {
    this->input_calls.push_back({start_address, {registers.begin(), registers.end()}, status});
  }
  void on_read_coils(uint16_t start_address, PackedBits bits, ResponseStatus status) override {
    this->coil_calls.push_back({start_address, bits.size(), {bits.bytes().begin(), bits.bytes().end()}, status});
  }
  void on_read_discrete_inputs(uint16_t start_address, PackedBits bits, ResponseStatus status) override {
    this->discrete_calls.push_back({start_address, bits.size(), {bits.bytes().begin(), bits.bytes().end()}, status});
  }
  void on_write_single_register(uint16_t address, uint16_t value, ResponseStatus status) override {
    this->write_single_register_calls.push_back({address, value, status});
  }
  void on_write_single_coil(uint16_t address, bool value, ResponseStatus status) override {
    this->write_single_coil_calls.push_back({address, static_cast<uint16_t>(value), status});
  }
  void on_write_multiple_registers(uint16_t start_address, std::span<const uint16_t> registers,
                                   ResponseStatus status) override {
    this->write_multiple_registers_calls.push_back({start_address, {registers.begin(), registers.end()}, status});
  }
  void on_write_multiple_coils(uint16_t start_address, PackedBits bits, ResponseStatus status) override {
    this->write_multiple_coils_calls.push_back(
        {start_address, bits.size(), {bits.bytes().begin(), bits.bytes().end()}, status});
  }
  void on_custom_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu,
                          ResponseStatus status) override {
    this->custom_requests.emplace_back(request_pdu.begin(), request_pdu.end());
    this->custom_responses.emplace_back(response_pdu.begin(), response_pdu.end());
    this->custom_statuses.push_back(status);
  }

  std::vector<ReadRegistersCall> holding_calls;
  std::vector<ReadRegistersCall> input_calls;
  std::vector<ReadBitsCall> coil_calls;
  std::vector<ReadBitsCall> discrete_calls;
  std::vector<WriteCall> write_single_register_calls;
  std::vector<WriteCall> write_single_coil_calls;
  std::vector<ReadRegistersCall> write_multiple_registers_calls;
  std::vector<ReadBitsCall> write_multiple_coils_calls;
  std::vector<std::vector<uint8_t>> custom_requests;
  std::vector<std::vector<uint8_t>> custom_responses;
  std::vector<ResponseStatus> custom_statuses;
};

// Overrides only the generic callbacks to verify the typed defaults delegate to them.
class GenericDevice : public ModbusClientDevice {
 public:
  void on_read_registers(EntityType register_type, uint16_t start_address, std::span<const uint16_t> registers,
                         ResponseStatus status) override {
    this->register_type = register_type;
    this->start_address = start_address;
    this->registers.assign(registers.begin(), registers.end());
    this->calls++;
  }
  void on_read_bits(EntityType register_type, uint16_t start_address, PackedBits bits, ResponseStatus status) override {
    this->register_type = register_type;
    this->start_address = start_address;
    this->bit_count = bits.size();
    this->calls++;
  }
  EntityType register_type{EntityType::CUSTOM};
  uint16_t start_address{0};
  uint16_t bit_count{0};
  std::vector<uint16_t> registers;
  int calls{0};
};

}  // namespace

TEST(ModbusClientDeviceFanOut, ReadHoldingRegistersSuccess) {
  RecordingDevice device;
  const uint8_t request[] = {0x03, 0x01, 0x00, 0x00, 0x02};         // read 2 regs at 0x100
  const uint8_t response[] = {0x03, 0x04, 0x00, 0x2A, 0x01, 0x00};  // 0x002A, 0x0100
  device.on_response(request, response);

  ASSERT_EQ(device.holding_calls.size(), 1u);
  const auto &call = device.holding_calls.front();
  EXPECT_EQ(call.start_address, 0x100);
  EXPECT_EQ(call.registers, (std::vector<uint16_t>{0x002A, 0x0100}));
  EXPECT_FALSE(call.status.has_value());
}

// FC 0x17: the response carries only the read block, so it decodes as a holding-register read of the read
// start/count. The write half has no client-side ack callback - it is confirmed by a successful response.
TEST(ModbusClientDeviceFanOut, ReadWriteMultipleRegistersDeliversReadBlockAsHolding) {
  RecordingDevice device;
  // read 2 regs at 0x0010, write 1 reg (0x00FF) at 0x0020
  const uint8_t request[] = {0x17, 0x00, 0x10, 0x00, 0x02, 0x00, 0x20, 0x00, 0x01, 0x02, 0x00, 0xFF};
  const uint8_t response[] = {0x17, 0x04, 0x00, 0x2A, 0x01, 0x00};  // read-back: 0x002A, 0x0100
  device.on_response(request, response);

  ASSERT_EQ(device.holding_calls.size(), 1u);
  const auto &call = device.holding_calls.front();
  EXPECT_EQ(call.start_address, 0x0010);  // the READ start address, not the write
  EXPECT_EQ(call.registers, (std::vector<uint16_t>{0x002A, 0x0100}));
  EXPECT_FALSE(call.status.has_value());
  EXPECT_TRUE(device.write_multiple_registers_calls.empty());  // no separate write-ack on the client side
}

// A 0x17 response shorter than the requested read count is self-consistent but wrong; it must be diverted
// to on_custom_response(), never clamped and delivered as if complete.
TEST(ModbusClientDeviceFanOut, ReadWriteMultipleRegistersShortResponseGoesToCustom) {
  RecordingDevice device;
  const uint8_t request[] = {0x17, 0x00, 0x10, 0x00, 0x02, 0x00, 0x20, 0x00, 0x01, 0x02, 0x00, 0xFF};
  const uint8_t response[] = {0x17, 0x02, 0x00, 0x2A};  // only 1 register, but 2 were requested
  device.on_response(request, response);

  EXPECT_TRUE(device.holding_calls.empty());
  EXPECT_EQ(device.custom_requests.size(), 1u);
}

TEST(ModbusClientDeviceFanOut, ReadInputRegistersDelegateToGeneric) {
  GenericDevice device;
  const uint8_t request[] = {0x04, 0x00, 0x10, 0x00, 0x01};
  const uint8_t response[] = {0x04, 0x02, 0x12, 0x34};
  device.on_response(request, response);

  EXPECT_EQ(device.calls, 1);
  EXPECT_EQ(device.register_type, EntityType::INPUT_REGISTER);
  EXPECT_EQ(device.start_address, 0x10);
  EXPECT_EQ(device.registers, (std::vector<uint16_t>{0x1234}));
}

TEST(ModbusClientDeviceFanOut, ReadDiscreteInputsDelegateToGenericBits) {
  GenericDevice device;
  const uint8_t request[] = {0x02, 0x00, 0x20, 0x00, 0x05};  // 5 inputs at 0x20
  const uint8_t response[] = {0x02, 0x01, 0x15};
  device.on_response(request, response);

  EXPECT_EQ(device.calls, 1);
  EXPECT_EQ(device.register_type, EntityType::DISCRETE_INPUT);
  EXPECT_EQ(device.start_address, 0x20);
  EXPECT_EQ(device.bit_count, 5);
}

// A CRC-valid response whose length does not match its request cannot be decoded per the
// function-code contract: it goes to the catch-all with the raw PDUs, not to the typed callback.
TEST(ModbusClientDeviceFanOut, ReadRegistersMismatchedLengthGoesToCatchAll) {
  RecordingDevice device;
  // Request asks for 4 registers but the response only carries 1.
  const uint8_t request[] = {0x03, 0x00, 0x00, 0x00, 0x04};
  const uint8_t response[] = {0x03, 0x02, 0xBE, 0xEF};
  device.on_response(request, response);

  EXPECT_TRUE(device.holding_calls.empty());
  ASSERT_EQ(device.custom_responses.size(), 1u);
  EXPECT_EQ(device.custom_responses.front(), (std::vector<uint8_t>(response, response + sizeof(response))));
}

// Coil responses are validated the same way: byte count must be ceil(count / 8).
TEST(ModbusClientDeviceFanOut, ReadCoilsMismatchedLengthGoesToCatchAll) {
  RecordingDevice device;
  const uint8_t request[] = {0x01, 0x00, 0x13, 0x00, 0x13};  // 19 coils -> 3 packed bytes
  const uint8_t response[] = {0x01, 0x02, 0xCD, 0x6B};       // only 2
  device.on_response(request, response);

  EXPECT_TRUE(device.coil_calls.empty());
  EXPECT_EQ(device.custom_responses.size(), 1u);
}

TEST(ModbusClientDeviceFanOut, ReadCoilsSuccess) {
  RecordingDevice device;
  const uint8_t request[] = {0x01, 0x00, 0x13, 0x00, 0x13};  // 19 coils at 0x13
  const uint8_t response[] = {0x01, 0x03, 0xCD, 0x6B, 0x05};
  device.on_response(request, response);

  ASSERT_EQ(device.coil_calls.size(), 1u);
  const auto &call = device.coil_calls.front();
  EXPECT_EQ(call.start_address, 0x13);
  EXPECT_EQ(call.count, 19);
  EXPECT_EQ(call.packed, (std::vector<uint8_t>{0xCD, 0x6B, 0x05}));
  EXPECT_FALSE(call.status.has_value());
  // first coil = bit 0 of byte 0
  EXPECT_TRUE(helpers::bit_from_packed(0, call.packed));
  EXPECT_FALSE(helpers::bit_from_packed(1, call.packed));
}

TEST(ModbusClientDeviceFanOut, WriteSingleRegisterSuccess) {
  RecordingDevice device;
  const uint8_t request[] = {0x06, 0x00, 0x01, 0x00, 0x03};
  device.on_response(request, request);  // echo

  ASSERT_EQ(device.write_single_register_calls.size(), 1u);
  const auto &call = device.write_single_register_calls.front();
  EXPECT_EQ(call.address, 1);
  EXPECT_EQ(call.value, 3);
  EXPECT_FALSE(call.status.has_value());
}

TEST(ModbusClientDeviceFanOut, WriteSingleCoilSuccess) {
  RecordingDevice device;
  const uint8_t request[] = {0x05, 0x00, 0xAC, 0xFF, 0x00};
  device.on_response(request, request);

  ASSERT_EQ(device.write_single_coil_calls.size(), 1u);
  EXPECT_EQ(device.write_single_coil_calls.front().address, 0xAC);
  EXPECT_EQ(device.write_single_coil_calls.front().value, 1u);
}

TEST(ModbusClientDeviceFanOut, WriteErrorReportsRequestArgumentsAndStatus) {
  RecordingDevice device;
  const uint8_t request[] = {0x06, 0x00, 0x01, 0x00, 0x03};
  const uint8_t exception[] = {0x86, 0x02};  // ILLEGAL_DATA_ADDRESS
  device.on_error(request, static_cast<ExceptionCode>(exception[1]));

  ASSERT_EQ(device.write_single_register_calls.size(), 1u);
  const auto &call = device.write_single_register_calls.front();
  EXPECT_EQ(call.address, 1);
  EXPECT_EQ(call.value, 3);
  EXPECT_EQ(call.status, ExceptionCode::ILLEGAL_DATA_ADDRESS);
}

TEST(ModbusClientDeviceFanOut, ReadErrorReportsEmptyDataAndStatus) {
  RecordingDevice device;
  const uint8_t request[] = {0x03, 0x01, 0x00, 0x00, 0x02};
  const uint8_t exception[] = {0x83, 0x02};
  device.on_error(request, static_cast<ExceptionCode>(exception[1]));

  ASSERT_EQ(device.holding_calls.size(), 1u);
  const auto &call = device.holding_calls.front();
  EXPECT_EQ(call.start_address, 0x100);
  EXPECT_TRUE(call.registers.empty());
  EXPECT_EQ(call.status, ExceptionCode::ILLEGAL_DATA_ADDRESS);
}

TEST(ModbusClientDeviceFanOut, CustomFunctionCodeGoesToCatchAll) {
  RecordingDevice device;
  const uint8_t request[] = {0x47, 0x01, 0x02, 0x03, 0x04};
  const uint8_t response[] = {0x47, 0xAA, 0xBB};
  device.on_response(request, response);

  ASSERT_EQ(device.custom_requests.size(), 1u);
  EXPECT_EQ(device.custom_requests.front(), (std::vector<uint8_t>{0x47, 0x01, 0x02, 0x03, 0x04}));
  EXPECT_EQ(device.custom_responses.front(), (std::vector<uint8_t>{0x47, 0xAA, 0xBB}));
  EXPECT_FALSE(device.custom_statuses.front().has_value());
  EXPECT_TRUE(device.holding_calls.empty());

  // On failure the catch-all receives an empty response and the status (the exception code).
  const uint8_t exception[] = {0xC7, 0x02};
  device.on_error(request, static_cast<ExceptionCode>(exception[1]));
  ASSERT_EQ(device.custom_statuses.size(), 2u);
  EXPECT_EQ(device.custom_statuses.back(), ExceptionCode::ILLEGAL_DATA_ADDRESS);
  EXPECT_TRUE(device.custom_responses.back().empty());
}

// A write ack only echoes the start address and count, so the data that was written is decoded from the
// request PDU: [0] function code, [1..2] start address, [3..4] count, [5] byte count, [6..] data.
TEST(ModbusClientDeviceFanOut, WriteMultipleAcksReportStartAndData) {
  RecordingDevice device;
  // Write 2 registers (0x0001, 0x0002) at 0x0020: byte count 4, data from offset 6.
  const uint8_t reg_request[] = {0x10, 0x00, 0x20, 0x00, 0x02, 0x04, 0x00, 0x01, 0x00, 0x02};
  const uint8_t reg_ack[] = {0x10, 0x00, 0x20, 0x00, 0x02};
  device.on_response(reg_request, reg_ack);
  // Write 10 coils at 0x0030: byte count 2, packed bits 0xFF 0x03 from offset 6.
  const uint8_t coil_request[] = {0x0F, 0x00, 0x30, 0x00, 0x0A, 0x02, 0xFF, 0x03};
  const uint8_t coil_ack[] = {0x0F, 0x00, 0x30, 0x00, 0x0A};
  device.on_response(coil_request, coil_ack);

  ASSERT_EQ(device.write_multiple_registers_calls.size(), 1u);
  EXPECT_EQ(device.write_multiple_registers_calls.front().start_address, 0x20);
  EXPECT_EQ(device.write_multiple_registers_calls.front().registers, (std::vector<uint16_t>{0x0001, 0x0002}));
  ASSERT_EQ(device.write_multiple_coils_calls.size(), 1u);
  EXPECT_EQ(device.write_multiple_coils_calls.front().start_address, 0x30);
  EXPECT_EQ(device.write_multiple_coils_calls.front().count, 10);
  EXPECT_EQ(device.write_multiple_coils_calls.front().packed, (std::vector<uint8_t>{0xFF, 0x03}));
}

// A truncated request (byte-count header promises more data than the PDU carries) is not a standard
// write-multiple, so it is diverted to on_custom_response() - never clamped and delivered as if complete.
TEST(ModbusClientDeviceFanOut, WriteMultipleTruncatedRequestDispatchesAsCustom) {
  RecordingDevice device;
  // Header claims 2 registers / 4 data bytes, but only one register's worth is present.
  const uint8_t reg_request[] = {0x10, 0x00, 0x20, 0x00, 0x02, 0x04, 0x00, 0x01};
  const uint8_t reg_ack[] = {0x10, 0x00, 0x20, 0x00, 0x02};
  device.on_response(reg_request, reg_ack);

  EXPECT_TRUE(device.write_multiple_registers_calls.empty());
  ASSERT_EQ(device.custom_requests.size(), 1u);
  EXPECT_EQ(device.custom_requests.front(), (std::vector<uint8_t>{0x10, 0x00, 0x20, 0x00, 0x02, 0x04, 0x00, 0x01}));
}

// A request whose byte-count header disagrees with its own quantity field (here: 2 registers but a
// byte count of 2 instead of 4, with matching data) is non-standard and diverted to the catch-all.
TEST(ModbusClientDeviceFanOut, WriteMultipleInconsistentByteCountDispatchesAsCustom) {
  RecordingDevice device;
  const uint8_t reg_request[] = {0x10, 0x00, 0x20, 0x00, 0x02, 0x02, 0x00, 0x01};
  const uint8_t reg_ack[] = {0x10, 0x00, 0x20, 0x00, 0x02};
  device.on_response(reg_request, reg_ack);

  EXPECT_TRUE(device.write_multiple_registers_calls.empty());
  EXPECT_EQ(device.custom_requests.size(), 1u);
}

// An exception on a read still dispatches to the typed callback (empty data, status set): the gate must
// not require a standard response on the failure path, because on_error() delivers an empty response by
// design.
TEST(ModbusClientDeviceFanOut, ReadErrorWithEmptyResponseStillDispatchesTyped) {
  RecordingDevice device;
  const uint8_t read_request[] = {0x03, 0x01, 0x00, 0x00, 0x02};
  device.on_error(read_request, ExceptionCode::ILLEGAL_DATA_ADDRESS);

  ASSERT_EQ(device.holding_calls.size(), 1u);
  EXPECT_TRUE(device.holding_calls.front().registers.empty());
  EXPECT_EQ(device.holding_calls.front().status, ExceptionCode::ILLEGAL_DATA_ADDRESS);
  EXPECT_TRUE(device.custom_requests.empty());
}

// An error on a coil read must deliver a PackedBits view whose size() is zero - the count must never
// promise bits that have no bytes behind them (operator[] is unchecked).
TEST(ModbusClientDeviceFanOut, ReadCoilsErrorDeliversZeroCountBits) {
  RecordingDevice device;
  const uint8_t read_request[] = {0x01, 0x01, 0x00, 0x00, 0x0A};
  device.on_error(read_request, ExceptionCode::SERVICE_DEVICE_FAILURE);

  ASSERT_EQ(device.coil_calls.size(), 1u);
  EXPECT_EQ(device.coil_calls.front().count, 0);
  EXPECT_TRUE(device.coil_calls.front().packed.empty());
}

// Single-write acks: on success the delivered value is the device's echo (real read-back);
// on an exception it falls back to the request copy.
TEST(ModbusTypedDispatch, SingleWriteAckPrefersTheResponseEcho) {
  RecordingDevice device;
  const uint8_t request[] = {0x06, 0x00, 0x10, 0x00, 0x2A};
  const uint8_t echo_clamped[] = {0x06, 0x00, 0x10, 0x00, 0x28};  // device clamped 42 -> 40
  device.on_response(request, echo_clamped);
  ASSERT_EQ(device.write_single_register_calls.size(), 1u);
  EXPECT_EQ(device.write_single_register_calls.front().value, 0x0028);  // the echo, not the request

  device.on_error(request, ExceptionCode::ILLEGAL_DATA_VALUE);
  ASSERT_EQ(device.write_single_register_calls.size(), 2u);
  EXPECT_EQ(device.write_single_register_calls.back().value, 0x002A);  // exception: request copy
}

// Deprecated on_modbus_data() compatibility shim (pre-2026.8 API). Records the vectors delivered to
// the old callback so we can pin its payload framing against the pre-2026.7 behavior.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
namespace {
class LegacyDevice : public ModbusDevice {
 public:
  void on_modbus_data(const std::vector<uint8_t> &data) override { this->data_calls.push_back(data); }
  void on_modbus_error(uint8_t function_code, uint8_t exception_code) override {
    this->error_calls.emplace_back(function_code, exception_code);
  }
  std::vector<std::vector<uint8_t>> data_calls;
  std::vector<std::pair<uint8_t, uint8_t>> error_calls;
};
}  // namespace

// Custom (user-defined) function codes historically delivered the payload INCLUDING the function code
// byte (frame data_offset 1). External components such as the Century VS pump match that first byte
// against the code they sent, so dropping it (issue #17994) makes every response get ignored.
TEST(ModbusLegacyShim, CustomFunctionCodeKeepsFunctionCodeByte) {
  LegacyDevice device;
  const uint8_t request[] = {0x45, 0x01, 0x02};         // custom function 0x45
  const uint8_t response[] = {0x45, 0xAA, 0xBB, 0xCC};  // echo of the custom code + data
  device.on_response(request, response);

  ASSERT_EQ(device.data_calls.size(), 1u);
  EXPECT_EQ(device.data_calls.front(), (std::vector<uint8_t>{0x45, 0xAA, 0xBB, 0xCC}));
}

// Standard reads still strip the function code and byte-count header, matching the pre-2026.7 shim.
TEST(ModbusLegacyShim, StandardReadStripsHeader) {
  LegacyDevice device;
  const uint8_t request[] = {0x03, 0x01, 0x00, 0x00, 0x02};
  const uint8_t response[] = {0x03, 0x04, 0x00, 0x2A, 0x01, 0x00};
  device.on_response(request, response);

  ASSERT_EQ(device.data_calls.size(), 1u);
  EXPECT_EQ(device.data_calls.front(), (std::vector<uint8_t>{0x00, 0x2A, 0x01, 0x00}));
}
#pragma GCC diagnostic pop

}  // namespace esphome::modbus::testing
