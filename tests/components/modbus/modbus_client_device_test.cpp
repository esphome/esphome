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
  void on_write_multiple_registers(uint16_t start_address, uint16_t count, ResponseStatus status) override {
    this->write_multiple_registers_calls.push_back({start_address, count, status});
  }
  void on_write_multiple_coils(uint16_t start_address, uint16_t count, ResponseStatus status) override {
    this->write_multiple_coils_calls.push_back({start_address, count, status});
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
  std::vector<WriteCall> write_multiple_registers_calls;
  std::vector<WriteCall> write_multiple_coils_calls;
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
  device.on_error(request, static_cast<ModbusExceptionCode>(exception[1]));

  ASSERT_EQ(device.write_single_register_calls.size(), 1u);
  const auto &call = device.write_single_register_calls.front();
  EXPECT_EQ(call.address, 1);
  EXPECT_EQ(call.value, 3);
  EXPECT_EQ(call.status, ModbusExceptionCode::ILLEGAL_DATA_ADDRESS);
}

TEST(ModbusClientDeviceFanOut, ReadErrorReportsEmptyDataAndStatus) {
  RecordingDevice device;
  const uint8_t request[] = {0x03, 0x01, 0x00, 0x00, 0x02};
  const uint8_t exception[] = {0x83, 0x02};
  device.on_error(request, static_cast<ModbusExceptionCode>(exception[1]));

  ASSERT_EQ(device.holding_calls.size(), 1u);
  const auto &call = device.holding_calls.front();
  EXPECT_EQ(call.start_address, 0x100);
  EXPECT_TRUE(call.registers.empty());
  EXPECT_EQ(call.status, ModbusExceptionCode::ILLEGAL_DATA_ADDRESS);
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
  device.on_error(request, static_cast<ModbusExceptionCode>(exception[1]));
  ASSERT_EQ(device.custom_statuses.size(), 2u);
  EXPECT_EQ(device.custom_statuses.back(), ModbusExceptionCode::ILLEGAL_DATA_ADDRESS);
  EXPECT_TRUE(device.custom_responses.back().empty());
}

TEST(ModbusClientDeviceFanOut, WriteMultipleAcksReportStartAndCount) {
  RecordingDevice device;
  const uint8_t reg_request[] = {0x10, 0x00, 0x20, 0x00, 0x02, 0x04, 0x00, 0x01, 0x00, 0x02};
  const uint8_t reg_ack[] = {0x10, 0x00, 0x20, 0x00, 0x02};
  device.on_response(reg_request, reg_ack);
  const uint8_t coil_request[] = {0x0F, 0x00, 0x30, 0x00, 0x0A, 0x02, 0xFF, 0x03};
  const uint8_t coil_ack[] = {0x0F, 0x00, 0x30, 0x00, 0x0A};
  device.on_response(coil_request, coil_ack);

  ASSERT_EQ(device.write_multiple_registers_calls.size(), 1u);
  EXPECT_EQ(device.write_multiple_registers_calls.front().address, 0x20);
  EXPECT_EQ(device.write_multiple_registers_calls.front().value, 2);
  ASSERT_EQ(device.write_multiple_coils_calls.size(), 1u);
  EXPECT_EQ(device.write_multiple_coils_calls.front().address, 0x30);
  EXPECT_EQ(device.write_multiple_coils_calls.front().value, 10);
}

}  // namespace esphome::modbus::testing
