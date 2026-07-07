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

  auto status = server.on_write_registers(0x0000, make_registers({0x1234}));
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

  auto status = server.on_write_registers(0x0000, make_registers({0x1234, 0x5678}));
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
  auto status = server.on_write_registers(0x0000, make_registers({0x1111, 0x2222}));
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

  auto status = server.on_write_registers(0x0000, make_registers({0x1234}));
  ASSERT_TRUE(status.has_value());
  if (status.has_value())
    EXPECT_EQ(status.value(), ModbusExceptionCode::ILLEGAL_DATA_ADDRESS);
}

// An address with no registered register yields ILLEGAL_DATA_ADDRESS.
TEST(ModbusServerWrite, UnmatchedAddressRejected) {
  ModbusServer server;
  auto status = server.on_write_registers(0x0005, make_registers({0x1234}));
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

  auto status = server.on_write_registers(0x0000, make_registers({0xAAAA, 0xBBBB}));
  ASSERT_TRUE(status.has_value());
  if (status.has_value())
    EXPECT_EQ(status.value(), ModbusExceptionCode::SERVICE_DEVICE_FAILURE);
  EXPECT_TRUE(first_written);  // pre-validation passed, so the first write applied before the failure
}

// --- on_read_registers --------------------------------------------------

TEST(ModbusServerRead, SingleWordSucceeds) {
  ModbusServer server;
  ServerRegister reg(0x0000, SensorValueType::U_WORD, 1);
  reg.read_lambda = []() -> int64_t { return 0x1234; };
  server.add_server_register(&reg);

  RegisterValues out;
  auto status = server.on_read_registers(0x0000, 1, out);
  EXPECT_FALSE(status.has_value());
  ASSERT_EQ(out.size(), 1u);
  EXPECT_EQ(out[0], 0x1234);
}

TEST(ModbusServerRead, DwordReturnsTwoWordsHighFirst) {
  ModbusServer server;
  ServerRegister reg(0x0000, SensorValueType::U_DWORD, 2);
  reg.read_lambda = []() -> int64_t { return 0x12345678; };
  server.add_server_register(&reg);

  RegisterValues out;
  auto status = server.on_read_registers(0x0000, 2, out);
  EXPECT_FALSE(status.has_value());
  ASSERT_EQ(out.size(), 2u);
  EXPECT_EQ(out[0], 0x1234);
  EXPECT_EQ(out[1], 0x5678);
}

// Starting inside a multi-register value is rejected with ILLEGAL_DATA_ADDRESS -- not masked by the courtesy
// default -- and the read_lambda is never invoked.
TEST(ModbusServerRead, StartInsideValueRejected) {
  ModbusServer server;
  bool read_called = false;
  ServerRegister reg(0x0010, SensorValueType::U_DWORD, 2);  // occupies 0x0010 and 0x0011
  reg.read_lambda = [&read_called]() -> int64_t {
    read_called = true;
    return 0;
  };
  server.set_server_courtesy_response(
      ServerCourtesyResponse{.enabled = true, .register_last_address = 0xFFFF, .register_value = 0xABCD});
  server.add_server_register(&reg);

  RegisterValues out;
  auto status = server.on_read_registers(0x0011, 1, out);  // the second cell of the DWORD
  ASSERT_TRUE(status.has_value());
  if (status.has_value())
    EXPECT_EQ(status.value(), ModbusExceptionCode::ILLEGAL_DATA_ADDRESS);
  EXPECT_FALSE(read_called);
}

// A read that stops short of a value's end clips it -> ILLEGAL_DATA_ADDRESS, and the read_lambda is not invoked.
TEST(ModbusServerRead, ClippedTailRejected) {
  ModbusServer server;
  bool read_called = false;
  ServerRegister reg(0x0000, SensorValueType::U_DWORD, 2);
  reg.read_lambda = [&read_called]() -> int64_t {
    read_called = true;
    return 0;
  };
  server.add_server_register(&reg);

  RegisterValues out;
  auto status = server.on_read_registers(0x0000, 1, out);  // only 1 of the DWORD's 2 registers
  ASSERT_TRUE(status.has_value());
  if (status.has_value())
    EXPECT_EQ(status.value(), ModbusExceptionCode::ILLEGAL_DATA_ADDRESS);
  EXPECT_FALSE(read_called);
}

// A write-only register (no read_lambda) is not readable -> ILLEGAL_DATA_ADDRESS, not a courtesy default.
TEST(ModbusServerRead, WriteOnlyRegisterRejected) {
  ModbusServer server;
  ServerRegister reg(0x0000, SensorValueType::U_WORD, 1);  // no read_lambda set
  server.set_server_courtesy_response(
      ServerCourtesyResponse{.enabled = true, .register_last_address = 0xFFFF, .register_value = 0xABCD});
  server.add_server_register(&reg);

  RegisterValues out;
  auto status = server.on_read_registers(0x0000, 1, out);
  ASSERT_TRUE(status.has_value());
  if (status.has_value())
    EXPECT_EQ(status.value(), ModbusExceptionCode::ILLEGAL_DATA_ADDRESS);
}

// An unregistered address with courtesy enabled returns the default value for each cell.
TEST(ModbusServerRead, CourtesyDefaultForUnregistered) {
  ModbusServer server;
  server.set_server_courtesy_response(
      ServerCourtesyResponse{.enabled = true, .register_last_address = 0xFFFF, .register_value = 0xABCD});

  RegisterValues out;
  auto status = server.on_read_registers(0x0005, 2, out);
  EXPECT_FALSE(status.has_value());
  ASSERT_EQ(out.size(), 2u);
  EXPECT_EQ(out[0], 0xABCD);
  EXPECT_EQ(out[1], 0xABCD);
}

// An unregistered address with courtesy disabled is rejected.
TEST(ModbusServerRead, UnregisteredRejectedWithoutCourtesy) {
  ModbusServer server;
  RegisterValues out;
  auto status = server.on_read_registers(0x0005, 1, out);
  ASSERT_TRUE(status.has_value());
  if (status.has_value())
    EXPECT_EQ(status.value(), ModbusExceptionCode::ILLEGAL_DATA_ADDRESS);
}

// --- partial reads (opt-in) ----------------------------------------------------

// With allow_partial_read, reading only the first register of a DWORD returns its high word.
TEST(ModbusServerRead, PartialReadHighWord) {
  ModbusServer server;
  ServerRegister reg(0x0010, SensorValueType::U_DWORD, 2);
  reg.allow_partial_read = true;
  reg.read_lambda = []() -> int64_t { return 0x12345678; };
  server.add_server_register(&reg);

  RegisterValues out;
  auto status = server.on_read_registers(0x0010, 1, out);
  EXPECT_FALSE(status.has_value());
  ASSERT_EQ(out.size(), 1u);
  EXPECT_EQ(out[0], 0x1234);
}

// With allow_partial_read, starting at the interior cell returns the low word.
TEST(ModbusServerRead, PartialReadLowWordFromInterior) {
  ModbusServer server;
  ServerRegister reg(0x0010, SensorValueType::U_DWORD, 2);
  reg.allow_partial_read = true;
  reg.read_lambda = []() -> int64_t { return 0x12345678; };
  server.add_server_register(&reg);

  RegisterValues out;
  auto status = server.on_read_registers(0x0011, 1, out);
  EXPECT_FALSE(status.has_value());
  ASSERT_EQ(out.size(), 1u);
  EXPECT_EQ(out[0], 0x5678);
}

// Slicing is in wire order, so a reversed value type partials correctly: U_DWORD_R emits the low word
// first, so 0x0010 holds 0x5678 and 0x0011 holds 0x1234.
TEST(ModbusServerRead, PartialReadReversedType) {
  ModbusServer server;
  ServerRegister reg(0x0010, SensorValueType::U_DWORD_R, 2);
  reg.allow_partial_read = true;
  reg.read_lambda = []() -> int64_t { return 0x12345678; };
  server.add_server_register(&reg);

  RegisterValues first;
  ASSERT_FALSE(server.on_read_registers(0x0010, 1, first).has_value());
  ASSERT_EQ(first.size(), 1u);
  EXPECT_EQ(first[0], 0x5678);

  RegisterValues second;
  ASSERT_FALSE(server.on_read_registers(0x0011, 1, second).has_value());
  ASSERT_EQ(second.size(), 1u);
  EXPECT_EQ(second[0], 0x1234);
}

}  // namespace esphome::modbus_server
