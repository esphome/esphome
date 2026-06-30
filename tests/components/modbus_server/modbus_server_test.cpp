#include <gtest/gtest.h>

#include "esphome/components/modbus_server/modbus_server.h"

namespace esphome::modbus_server {

using modbus::ModbusExceptionCode;
using modbus::RegisterValues;

namespace {

RegisterValues make_registers(std::initializer_list<uint16_t> values) {
  RegisterValues registers;
  for (uint16_t value : values)
    registers.push_back(value);
  return registers;
}

}  // namespace

// A single writable WORD register is applied and the handler reports success (nullopt).
TEST(ModbusServerWrite, SingleWordSucceeds) {
  ModbusServer server;
  int64_t written = -1;
  ServerRegister reg(0x0000, SensorValueType::U_WORD, 1);
  reg.write_lambda = [&written](int64_t value) {
    written = value;
    return true;
  };
  server.add_server_register(&reg);

  auto status = server.on_modbus_write_registers(0x0000, make_registers({0x1234}));
  EXPECT_FALSE(status.has_value());  // nullopt == success
  EXPECT_EQ(written, 0x1234);
}

// A multi-register value is decoded high word first and applied as a single number.
TEST(ModbusServerWrite, DwordSucceeds) {
  ModbusServer server;
  int64_t written = -1;
  ServerRegister reg(0x0000, SensorValueType::U_DWORD, 2);
  reg.write_lambda = [&written](int64_t value) {
    written = value;
    return true;
  };
  server.add_server_register(&reg);

  auto status = server.on_modbus_write_registers(0x0000, make_registers({0x1234, 0x5678}));
  EXPECT_FALSE(status.has_value());
  EXPECT_EQ(written, 0x12345678);
}

// Regression: a request that under-supplies a multi-register value is rejected before any
// write_lambda runs, so no register is partially written.
TEST(ModbusServerWrite, UnderSuppliedValueAppliesNothing) {
  ModbusServer server;
  bool word_written = false;
  ServerRegister word_reg(0x0000, SensorValueType::U_WORD, 1);
  word_reg.write_lambda = [&word_written](int64_t) {
    word_written = true;
    return true;
  };
  bool dword_written = false;
  ServerRegister dword_reg(0x0001, SensorValueType::U_DWORD, 2);  // needs two registers
  dword_reg.write_lambda = [&dword_written](int64_t) {
    dword_written = true;
    return true;
  };
  server.add_server_register(&word_reg);
  server.add_server_register(&dword_reg);

  // Two words supplied: one for the WORD at 0x0000, but only one of the two the DWORD at 0x0001 needs.
  auto status = server.on_modbus_write_registers(0x0000, make_registers({0x1111, 0x2222}));
  ASSERT_TRUE(status.has_value());
  if (status.has_value())
    EXPECT_EQ(status.value(), ModbusExceptionCode::ILLEGAL_DATA_VALUE);
  EXPECT_FALSE(word_written);  // the writable WORD must NOT have been applied
  EXPECT_FALSE(dword_written);
}

// A read-only register (no write_lambda) yields ILLEGAL_DATA_ADDRESS and applies nothing.
TEST(ModbusServerWrite, UnwritableRegisterRejected) {
  ModbusServer server;
  ServerRegister read_only(0x0000, SensorValueType::U_WORD, 1);  // no write_lambda set
  server.add_server_register(&read_only);

  auto status = server.on_modbus_write_registers(0x0000, make_registers({0x1234}));
  ASSERT_TRUE(status.has_value());
  if (status.has_value())
    EXPECT_EQ(status.value(), ModbusExceptionCode::ILLEGAL_DATA_ADDRESS);
}

// An address with no registered register yields ILLEGAL_DATA_ADDRESS.
TEST(ModbusServerWrite, UnmatchedAddressRejected) {
  ModbusServer server;
  auto status = server.on_modbus_write_registers(0x0005, make_registers({0x1234}));
  ASSERT_TRUE(status.has_value());
  if (status.has_value())
    EXPECT_EQ(status.value(), ModbusExceptionCode::ILLEGAL_DATA_ADDRESS);
}

// A write_lambda failing at runtime is the one non-atomic case: the earlier register is already
// applied, and the handler reports SERVICE_DEVICE_FAILURE.
TEST(ModbusServerWrite, CallbackFailureIsServiceDeviceFailure) {
  ModbusServer server;
  bool first_written = false;
  ServerRegister first(0x0000, SensorValueType::U_WORD, 1);
  first.write_lambda = [&first_written](int64_t) {
    first_written = true;
    return true;
  };
  ServerRegister second(0x0001, SensorValueType::U_WORD, 1);
  second.write_lambda = [](int64_t) { return false; };  // rejects at runtime
  server.add_server_register(&first);
  server.add_server_register(&second);

  auto status = server.on_modbus_write_registers(0x0000, make_registers({0xAAAA, 0xBBBB}));
  ASSERT_TRUE(status.has_value());
  if (status.has_value())
    EXPECT_EQ(status.value(), ModbusExceptionCode::SERVICE_DEVICE_FAILURE);
  EXPECT_TRUE(first_written);  // pre-validation passed, so the first write applied before the failure
}

// Function code 0x17 performs the write before the read, so a register read in the same transaction
// observes the value just written through the default per-register routing.
TEST(ModbusServerReadWrite, DefaultRoutingAppliesWriteBeforeRead) {
  ModbusServer server;
  uint16_t stored = 0x0000;
  ServerRegister reg(0x0000, SensorValueType::U_WORD, 1);
  reg.read_lambda = [&stored]() { return static_cast<int64_t>(stored); };
  reg.write_lambda = [&stored](int64_t value) {
    stored = static_cast<uint16_t>(value);
    return true;
  };
  server.add_server_register(&reg);

  RegisterValues read_registers;
  auto status = server.on_modbus_read_write_registers(0x0000, 1, 0x0000, make_registers({0xBEEF}), read_registers);
  EXPECT_FALSE(status.has_value());
  ASSERT_EQ(read_registers.size(), 1u);
  EXPECT_EQ(read_registers[0], 0xBEEF);
}

// When the write half fails, the default routing returns that exception and never serves the read.
TEST(ModbusServerReadWrite, DefaultRoutingWriteFailureSkipsRead) {
  ModbusServer server;
  bool read_called = false;
  ServerRegister read_reg(0x0000, SensorValueType::U_WORD, 1);
  read_reg.read_lambda = [&read_called]() {
    read_called = true;
    return static_cast<int64_t>(0x1234);
  };
  server.add_server_register(&read_reg);

  // Write targets an unregistered address, so the write fails and the read must be skipped.
  RegisterValues read_registers;
  auto status = server.on_modbus_read_write_registers(0x0000, 1, 0x0010, make_registers({0x0001}), read_registers);
  ASSERT_TRUE(status.has_value());
  if (status.has_value())
    EXPECT_EQ(status.value(), ModbusExceptionCode::ILLEGAL_DATA_ADDRESS);
  EXPECT_FALSE(read_called);
  EXPECT_TRUE(read_registers.empty());
}

// A configured transaction handler takes full control: it receives the request context, builds the
// response, and the per-register read/write handlers are bypassed entirely.
TEST(ModbusServerReadWrite, LambdaOverridesPerRegisterHandlers) {
  ModbusServer server;
  bool reg_read = false;
  bool reg_written = false;
  ServerRegister reg(0x0000, SensorValueType::U_WORD, 1);
  reg.read_lambda = [&reg_read]() {
    reg_read = true;
    return static_cast<int64_t>(0);
  };
  reg.write_lambda = [&reg_written](int64_t) {
    reg_written = true;
    return true;
  };
  server.add_server_register(&reg);

  uint16_t seen_read_address = 0;
  uint16_t seen_read_count = 0;
  uint16_t seen_write_address = 0;
  size_t seen_write_count = 0;
  uint16_t seen_write_value = 0;
  server.set_read_write_lambda([&](uint16_t read_address, uint16_t read_count, uint16_t write_address,
                                   const RegisterValues &write_values, RegisterValues &read_values) {
    seen_read_address = read_address;
    seen_read_count = read_count;
    seen_write_address = write_address;
    seen_write_count = write_values.size();
    if (!write_values.empty())
      seen_write_value = write_values[0];
    for (uint16_t i = 0; i < read_count; i++)
      read_values.push_back(0xA000 + i);
    return true;
  });

  // The register at 0x0000 has both handlers, so if routing fell through to them they would run.
  RegisterValues read_registers;
  auto status = server.on_modbus_read_write_registers(0x0000, 1, 0x0000, make_registers({0x0102}), read_registers);
  EXPECT_FALSE(status.has_value());
  EXPECT_FALSE(reg_read);
  EXPECT_FALSE(reg_written);
  EXPECT_EQ(seen_read_address, 0x0000);
  EXPECT_EQ(seen_read_count, 1);
  EXPECT_EQ(seen_write_address, 0x0000);
  EXPECT_EQ(seen_write_count, 1u);
  EXPECT_EQ(seen_write_value, 0x0102);
  ASSERT_EQ(read_registers.size(), 1u);
  EXPECT_EQ(read_registers[0], 0xA000);
}

// A transaction handler that rejects the request maps to an ILLEGAL_DATA_ADDRESS exception.
TEST(ModbusServerReadWrite, LambdaFailureIsIllegalDataAddress) {
  ModbusServer server;
  server.set_read_write_lambda(
      [](uint16_t, uint16_t, uint16_t, const RegisterValues &, RegisterValues &) { return false; });

  RegisterValues read_registers;
  auto status = server.on_modbus_read_write_registers(0x0000, 1, 0x0000, make_registers({0x0001}), read_registers);
  ASSERT_TRUE(status.has_value());
  if (status.has_value())
    EXPECT_EQ(status.value(), ModbusExceptionCode::ILLEGAL_DATA_ADDRESS);
}

}  // namespace esphome::modbus_server
