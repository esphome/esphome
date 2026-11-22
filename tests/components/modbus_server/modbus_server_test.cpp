#include <gtest/gtest.h>

#include "esphome/components/modbus_server/modbus_server.h"

namespace esphome::modbus_server {

using modbus::ExceptionCode;
using modbus::RegisterValues;

namespace {

RegisterValues make_registers(std::initializer_list<uint16_t> values) {
  RegisterValues registers;
  for (uint16_t value : values)
    registers.push_back(value);
  return registers;
}

// Lambdas must be captureless, so they record into these; each test resets what it reads back.
int64_t written_value;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
uint8_t write_calls;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
bool read_called;       // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
bool bit_state[2];      // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

template<typename T> bool record_write(uint16_t /*address*/, T value) {
  written_value = static_cast<int64_t>(value);
  write_calls++;
  return true;
}

template<typename T> bool reject_write(uint16_t /*address*/, T /*value*/) { return false; }

bool record_bit_write(uint16_t address, bool value) {
  bit_state[address] = value;
  write_calls++;
  return true;
}

template<typename T> T stored_value;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// A read lambda's value comes out of read() as raw, and write(raw) hands the write lambda the same value.
template<SensorValueType VALUE_TYPE> void expect_round_trip(RegisterValueType<VALUE_TYPE> value, int64_t raw) {
  using T = RegisterValueType<VALUE_TYPE>;
  SCOPED_TRACE(static_cast<int>(VALUE_TYPE));
  ServerRegister reg(0x0042, VALUE_TYPE, 1);
  stored_value<T> = value;
  reg.set_read_lambda<VALUE_TYPE>([](uint16_t address) -> optional<T> {
    EXPECT_EQ(address, 0x0042);
    return stored_value<T>;
  });
  EXPECT_EQ(reg.read(), optional<int64_t>(raw));

  stored_value<T> = T{};
  reg.set_write_lambda<VALUE_TYPE>([](uint16_t address, T x) {
    EXPECT_EQ(address, 0x0042);
    stored_value<T> = x;
    return true;
  });
  EXPECT_TRUE(reg.write(raw));
  EXPECT_EQ(stored_value<T>, value);
}

}  // namespace

TEST(ModbusServerRegister, ReadAndWriteRoundTripEveryValueType) {
  expect_round_trip<SensorValueType::U_WORD>(0xBEEF, 0xBEEF);
  expect_round_trip<SensorValueType::U_WORD_S>(0xBEEF, 0xBEEF);
  expect_round_trip<SensorValueType::S_WORD>(-2, -2);
  expect_round_trip<SensorValueType::S_WORD_S>(-2, -2);
  expect_round_trip<SensorValueType::U_DWORD>(0xDEADBEEF, 0xDEADBEEF);
  expect_round_trip<SensorValueType::U_DWORD_R>(0xDEADBEEF, 0xDEADBEEF);
  expect_round_trip<SensorValueType::S_DWORD>(-100000, -100000);
  expect_round_trip<SensorValueType::S_DWORD_R>(-100000, -100000);
  expect_round_trip<SensorValueType::U_QWORD>(0xFEDCBA9876543210ULL, static_cast<int64_t>(0xFEDCBA9876543210ULL));
  expect_round_trip<SensorValueType::U_QWORD_R>(0xFEDCBA9876543210ULL, static_cast<int64_t>(0xFEDCBA9876543210ULL));
  expect_round_trip<SensorValueType::S_QWORD>(-1234567890123LL, -1234567890123LL);
  expect_round_trip<SensorValueType::S_QWORD_R>(-1234567890123LL, -1234567890123LL);
  expect_round_trip<SensorValueType::FP32>(1.5f, 0x3FC00000);
  expect_round_trip<SensorValueType::FP32_R>(-1.5f, 0xBFC00000);
}

// The log text follows the value type: unsigned, signed, or a float stored as its bits.
TEST(ModbusServerRegister, FormatValueFollowsValueType) {
  struct Case {
    SensorValueType value_type;
    int64_t value;
    const char *text;
  };
  static constexpr Case CASES[] = {
      {SensorValueType::U_WORD, 65535, "65535"},    {SensorValueType::U_QWORD_R, -1, "18446744073709551615"},
      {SensorValueType::S_WORD_S, -5, "-5"},        {SensorValueType::S_QWORD, INT64_MIN, "-9223372036854775808"},
      {SensorValueType::FP32_R, 0x3FC00000, "1.5"},
  };
  for (const auto &c : CASES) {
    ServerRegister reg(0x0000, c.value_type, 1);
    char buf[ServerRegister::FORMAT_VALUE_BUF_SIZE]{};
    EXPECT_STREQ(reg.format_value(c.value, buf, sizeof(buf)), c.text);
  }
}

// A single writable WORD register is applied and the handler reports success (nullopt).
TEST(ModbusServerWrite, SingleWordSucceeds) {
  ModbusServer server;
  written_value = -1;
  ServerRegister reg(0x0000, SensorValueType::U_WORD, 1);
  reg.set_write_lambda<SensorValueType::U_WORD>(record_write<uint16_t>);
  server.add_server_register(&reg);

  auto status = server.on_write_registers(0x0000, make_registers({0x1234}));
  EXPECT_FALSE(status.has_value());  // nullopt == success
  EXPECT_EQ(written_value, 0x1234);
}

TEST(ModbusServerWrite, SwappedWordSucceeds) {
  ModbusServer server;
  written_value = -1;
  ServerRegister reg(0x0000, SensorValueType::U_WORD_S, 1);
  reg.set_write_lambda<SensorValueType::U_WORD_S>(record_write<uint16_t>);
  server.add_server_register(&reg);

  auto status = server.on_write_registers(0x0000, make_registers({0x3412}));
  EXPECT_FALSE(status.has_value());
  EXPECT_EQ(written_value, 0x1234);
}

// A multi-register value is decoded high word first and applied as a single number.
TEST(ModbusServerWrite, DwordSucceeds) {
  ModbusServer server;
  written_value = -1;
  ServerRegister reg(0x0000, SensorValueType::U_DWORD, 2);
  reg.set_write_lambda<SensorValueType::U_DWORD>(record_write<uint32_t>);
  server.add_server_register(&reg);

  auto status = server.on_write_registers(0x0000, make_registers({0x1234, 0x5678}));
  EXPECT_FALSE(status.has_value());
  EXPECT_EQ(written_value, 0x12345678);
}

// Regression: a request that under-supplies a multi-register value is rejected before any
// write_lambda runs, so no register is partially written.
TEST(ModbusServerWrite, UnderSuppliedValueAppliesNothing) {
  ModbusServer server;
  write_calls = 0;
  ServerRegister word_reg(0x0000, SensorValueType::U_WORD, 1);
  word_reg.set_write_lambda<SensorValueType::U_WORD>(record_write<uint16_t>);
  ServerRegister dword_reg(0x0001, SensorValueType::U_DWORD, 2);  // needs two registers
  dword_reg.set_write_lambda<SensorValueType::U_DWORD>(record_write<uint32_t>);
  server.add_server_register(&word_reg);
  server.add_server_register(&dword_reg);

  // Two words supplied: one for the WORD at 0x0000, but only one of the two the DWORD at 0x0001 needs.
  auto status = server.on_write_registers(0x0000, make_registers({0x1111, 0x2222}));
  ASSERT_TRUE(status.has_value());
  if (status.has_value())
    EXPECT_EQ(status.value(), ExceptionCode::ILLEGAL_DATA_VALUE);
  EXPECT_EQ(write_calls, 0);  // the writable WORD must NOT have been applied
}

// A read-only register (no write_lambda) yields ILLEGAL_DATA_ADDRESS and applies nothing.
TEST(ModbusServerWrite, UnwritableRegisterRejected) {
  ModbusServer server;
  ServerRegister read_only(0x0000, SensorValueType::U_WORD, 1);  // no write_lambda set
  server.add_server_register(&read_only);

  auto status = server.on_write_registers(0x0000, make_registers({0x1234}));
  ASSERT_TRUE(status.has_value());
  if (status.has_value())
    EXPECT_EQ(status.value(), ExceptionCode::ILLEGAL_DATA_ADDRESS);
}

// A write to an address not covered by any configured register (on a populated server) yields
// ILLEGAL_DATA_ADDRESS.
TEST(ModbusServerWrite, UnmatchedAddressRejected) {
  ModbusServer server;
  ServerRegister reg(0x0000, SensorValueType::U_WORD, 1);
  reg.set_write_lambda<SensorValueType::U_WORD>(record_write<uint16_t>);
  server.add_server_register(&reg);

  auto status = server.on_write_registers(0x0005, make_registers({0x1234}));
  ASSERT_TRUE(status.has_value());
  if (status.has_value())
    EXPECT_EQ(status.value(), ExceptionCode::ILLEGAL_DATA_ADDRESS);
}

// A server with no registers configured does not implement the register-write function: ILLEGAL_FUNCTION.
TEST(ModbusServerWrite, EmptyServerRejectsWithIllegalFunction) {
  ModbusServer server;
  auto status = server.on_write_registers(0x0000, make_registers({0x1234}));
  ASSERT_TRUE(status.has_value());
  if (status.has_value())
    EXPECT_EQ(status.value(), ExceptionCode::ILLEGAL_FUNCTION);
}

// A write_lambda failing at runtime is the one non-atomic case: the earlier register is already
// applied, and the handler reports SERVICE_DEVICE_FAILURE.
TEST(ModbusServerWrite, CallbackFailureIsServiceDeviceFailure) {
  ModbusServer server;
  write_calls = 0;
  ServerRegister first(0x0000, SensorValueType::U_WORD, 1);
  first.set_write_lambda<SensorValueType::U_WORD>(record_write<uint16_t>);
  ServerRegister second(0x0001, SensorValueType::U_WORD, 1);
  second.set_write_lambda<SensorValueType::U_WORD>(reject_write<uint16_t>);  // rejects at runtime
  server.add_server_register(&first);
  server.add_server_register(&second);

  auto status = server.on_write_registers(0x0000, make_registers({0xAAAA, 0xBBBB}));
  ASSERT_TRUE(status.has_value());
  if (status.has_value())
    EXPECT_EQ(status.value(), ExceptionCode::SERVICE_DEVICE_FAILURE);
  EXPECT_EQ(write_calls, 1);  // pre-validation passed, so the first write applied before the failure
}

// --- on_read_registers --------------------------------------------------

TEST(ModbusServerRead, SingleWordSucceeds) {
  ModbusServer server;
  ServerRegister reg(0x0000, SensorValueType::U_WORD, 1);
  reg.set_read_lambda<SensorValueType::U_WORD>([](uint16_t) -> optional<uint16_t> { return 0x1234; });
  server.add_server_register(&reg);

  RegisterValues out;
  auto status = server.on_read_registers(0x0000, 1, out);
  EXPECT_FALSE(status.has_value());
  ASSERT_EQ(out.size(), 1u);
  EXPECT_EQ(out[0], 0x1234);
}

TEST(ModbusServerRead, SwappedWordReturnsByteSwappedRegister) {
  ModbusServer server;
  ServerRegister reg(0x0000, SensorValueType::U_WORD_S, 1);
  reg.set_read_lambda<SensorValueType::U_WORD_S>([](uint16_t) -> optional<uint16_t> { return 0x1234; });
  server.add_server_register(&reg);

  RegisterValues out;
  auto status = server.on_read_registers(0x0000, 1, out);
  EXPECT_FALSE(status.has_value());
  ASSERT_EQ(out.size(), 1u);
  EXPECT_EQ(out[0], 0x3412);
}

TEST(ModbusServerRead, DwordReturnsTwoWordsHighFirst) {
  ModbusServer server;
  ServerRegister reg(0x0000, SensorValueType::U_DWORD, 2);
  reg.set_read_lambda<SensorValueType::U_DWORD>([](uint16_t) -> optional<uint32_t> { return 0x12345678; });
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
  read_called = false;
  ServerRegister reg(0x0010, SensorValueType::U_DWORD, 2);  // occupies 0x0010 and 0x0011
  reg.set_read_lambda<SensorValueType::U_DWORD>([](uint16_t) -> optional<uint32_t> {
    read_called = true;
    return 0;
  });
  server.set_server_courtesy_response(
      ServerCourtesyResponse{.enabled = true, .register_last_address = 0xFFFF, .register_value = 0xABCD});
  server.add_server_register(&reg);

  RegisterValues out;
  auto status = server.on_read_registers(0x0011, 1, out);  // the second cell of the DWORD
  ASSERT_TRUE(status.has_value());
  if (status.has_value())
    EXPECT_EQ(status.value(), ExceptionCode::ILLEGAL_DATA_ADDRESS);
  EXPECT_FALSE(read_called);
}

// A read that stops short of a value's end clips it -> ILLEGAL_DATA_ADDRESS, and the read_lambda is not invoked.
TEST(ModbusServerRead, ClippedTailRejected) {
  ModbusServer server;
  read_called = false;
  ServerRegister reg(0x0000, SensorValueType::U_DWORD, 2);
  reg.set_read_lambda<SensorValueType::U_DWORD>([](uint16_t) -> optional<uint32_t> {
    read_called = true;
    return 0;
  });
  server.add_server_register(&reg);

  RegisterValues out;
  auto status = server.on_read_registers(0x0000, 1, out);  // only 1 of the DWORD's 2 registers
  ASSERT_TRUE(status.has_value());
  if (status.has_value())
    EXPECT_EQ(status.value(), ExceptionCode::ILLEGAL_DATA_ADDRESS);
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
    EXPECT_EQ(status.value(), ExceptionCode::ILLEGAL_DATA_ADDRESS);
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

// An unregistered address on a populated server (courtesy disabled) is rejected with ILLEGAL_DATA_ADDRESS.
TEST(ModbusServerRead, UnregisteredRejectedWithoutCourtesy) {
  ModbusServer server;
  ServerRegister reg(0x0000, SensorValueType::U_WORD, 1);
  reg.set_read_lambda<SensorValueType::U_WORD>([](uint16_t) -> optional<uint16_t> { return 0x1234; });
  server.add_server_register(&reg);

  RegisterValues out;
  auto status = server.on_read_registers(0x0005, 1, out);
  ASSERT_TRUE(status.has_value());
  if (status.has_value())
    EXPECT_EQ(status.value(), ExceptionCode::ILLEGAL_DATA_ADDRESS);
}

// A server with no registers configured (courtesy disabled) does not implement the register-read
// function: ILLEGAL_FUNCTION.
TEST(ModbusServerRead, EmptyServerRejectsWithIllegalFunction) {
  ModbusServer server;
  RegisterValues out;
  auto status = server.on_read_registers(0x0005, 1, out);
  ASSERT_TRUE(status.has_value());
  if (status.has_value())
    EXPECT_EQ(status.value(), ExceptionCode::ILLEGAL_FUNCTION);
}

// A register read lambda returning an empty optional declines the read: the whole request is
// answered with SERVICE_DEVICE_FAILURE.
TEST(ModbusServerRead, ReadLambdaDecliningIsServiceDeviceFailure) {
  ModbusServer server;
  ServerRegister reg(0x0000, SensorValueType::U_WORD, 1);
  reg.set_read_lambda<SensorValueType::U_WORD>([](uint16_t) -> optional<uint16_t> { return {}; });
  server.add_server_register(&reg);

  RegisterValues out;
  auto status = server.on_read_registers(0x0000, 1, out);
  EXPECT_EQ(status, ExceptionCode::SERVICE_DEVICE_FAILURE);
}

// --- partial reads (opt-in) ----------------------------------------------------

// With allow_partial_read, reading only the first register of a DWORD returns its high word.
TEST(ModbusServerRead, PartialReadHighWord) {
  ModbusServer server;
  ServerRegister reg(0x0010, SensorValueType::U_DWORD, 2);
  reg.allow_partial_read = true;
  reg.set_read_lambda<SensorValueType::U_DWORD>([](uint16_t) -> optional<uint32_t> { return 0x12345678; });
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
  reg.set_read_lambda<SensorValueType::U_DWORD>([](uint16_t) -> optional<uint32_t> { return 0x12345678; });
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
  reg.set_read_lambda<SensorValueType::U_DWORD_R>([](uint16_t) -> optional<uint32_t> { return 0x12345678; });
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

// --- bits (coils / discrete inputs, one shared address space) -------------------

// Bits are read through the shared table regardless of which read function code arrived:
// the hub routes both 0x01 and 0x02 to on_read_bits().
TEST(ModbusServerBits, ReadSetsRequestedBits) {
  ModbusServer server;
  ServerBit bit0(0x0000);
  bit0.set_read_lambda([](uint16_t) -> optional<bool> { return true; });
  ServerBit bit1(0x0001);
  bit1.set_read_lambda([](uint16_t) -> optional<bool> { return false; });
  ServerBit bit2(0x0002);
  bit2.set_read_lambda([](uint16_t) -> optional<bool> { return true; });
  server.add_server_bit(&bit0);
  server.add_server_bit(&bit1);
  server.add_server_bit(&bit2);

  uint8_t packed[1] = {0};
  auto status = server.on_read_bits(0x0000, modbus::MutablePackedBits(packed, 3));
  EXPECT_FALSE(status.has_value());
  EXPECT_EQ(packed[0], 0b101);
}

// The read lambda receives the bit's address, so one lambda can serve several bits.
TEST(ModbusServerBits, ReadLambdaReceivesAddress) {
  ModbusServer server;
  ServerBit server_bit(0x0007);
  server_bit.set_read_lambda([](uint16_t address) -> optional<bool> { return address == 0x0007; });
  server.add_server_bit(&server_bit);

  uint8_t packed[1] = {0};
  auto status = server.on_read_bits(0x0007, modbus::MutablePackedBits(packed, 1));
  EXPECT_FALSE(status.has_value());
  EXPECT_EQ(packed[0], 0x01);
}

// An unregistered or write-only bit rejects the whole read with ILLEGAL_DATA_ADDRESS.
TEST(ModbusServerBits, UnreadableBitRejectsRead) {
  ModbusServer server;
  ServerBit readable(0x0000);
  readable.set_read_lambda([](uint16_t) -> optional<bool> { return true; });
  ServerBit write_only(0x0001);
  write_only.set_write_lambda([](uint16_t, bool) { return true; });
  server.add_server_bit(&readable);
  server.add_server_bit(&write_only);

  uint8_t packed[1] = {0};
  auto status = server.on_read_bits(0x0000, modbus::MutablePackedBits(packed, 2));
  EXPECT_EQ(status, ExceptionCode::ILLEGAL_DATA_ADDRESS);

  auto unregistered = server.on_read_bits(0x0005, modbus::MutablePackedBits(packed, 1));
  EXPECT_EQ(unregistered, ExceptionCode::ILLEGAL_DATA_ADDRESS);
}

// A read lambda returning an empty optional declines the read: the whole request is answered
// with SERVICE_DEVICE_FAILURE.
TEST(ModbusServerBits, ReadLambdaDecliningIsServiceDeviceFailure) {
  ModbusServer server;
  ServerBit ok(0x0000);
  ok.set_read_lambda([](uint16_t) -> optional<bool> { return true; });
  ServerBit declining(0x0001);
  declining.set_read_lambda([](uint16_t) -> optional<bool> { return {}; });
  server.add_server_bit(&ok);
  server.add_server_bit(&declining);

  uint8_t packed[1] = {0};
  auto status = server.on_read_bits(0x0000, modbus::MutablePackedBits(packed, 2));
  EXPECT_EQ(status, ExceptionCode::SERVICE_DEVICE_FAILURE);
}

// A multi-coil write applies every bit and reports success.
TEST(ModbusServerBits, WriteAppliesAllBits) {
  ModbusServer server;
  bit_state[0] = false;
  bit_state[1] = true;
  ServerBit bit0(0x0000);
  bit0.set_write_lambda(record_bit_write);
  ServerBit bit1(0x0001);
  bit1.set_write_lambda(record_bit_write);
  server.add_server_bit(&bit0);
  server.add_server_bit(&bit1);

  const uint8_t packed[1] = {0b01};  // bit0 on, bit1 off
  auto status = server.on_write_coils(0x0000, modbus::PackedBits(packed, 2));
  EXPECT_FALSE(status.has_value());
  EXPECT_TRUE(bit_state[0]);
  EXPECT_FALSE(bit_state[1]);
}

// Pre-flight atomicity: an unwritable bit anywhere in the span rejects the write before any
// bit is applied.
TEST(ModbusServerBits, UnwritableBitAppliesNothing) {
  ModbusServer server;
  write_calls = 0;
  ServerBit writable(0x0000);
  writable.set_write_lambda(record_bit_write);
  ServerBit read_only(0x0001);
  read_only.set_read_lambda([](uint16_t) -> optional<bool> { return false; });
  server.add_server_bit(&writable);
  server.add_server_bit(&read_only);

  const uint8_t packed[1] = {0b11};
  auto status = server.on_write_coils(0x0000, modbus::PackedBits(packed, 2));
  EXPECT_EQ(status, ExceptionCode::ILLEGAL_DATA_ADDRESS);
  EXPECT_EQ(write_calls, 0);  // the writable bit must NOT have been applied
}

// A write lambda failing at runtime is the one non-atomic case: earlier bits stay applied and
// the handler reports SERVICE_DEVICE_FAILURE (mirrors the register behavior).
TEST(ModbusServerBits, CallbackFailureIsServiceDeviceFailure) {
  ModbusServer server;
  write_calls = 0;
  ServerBit first(0x0000);
  first.set_write_lambda(record_bit_write);
  ServerBit second(0x0001);
  second.set_write_lambda([](uint16_t, bool) { return false; });  // rejects at runtime
  server.add_server_bit(&first);
  server.add_server_bit(&second);

  const uint8_t packed[1] = {0b11};
  auto status = server.on_write_coils(0x0000, modbus::PackedBits(packed, 2));
  EXPECT_EQ(status, ExceptionCode::SERVICE_DEVICE_FAILURE);
  EXPECT_EQ(write_calls, 1);
}

}  // namespace esphome::modbus_server
