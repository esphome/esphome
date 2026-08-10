#include <gtest/gtest.h>

#include <cstdint>
#include <span>
#include <vector>

#include "common.h"
#include "esphome/components/modbus/modbus.h"
#include "esphome/core/hal.h"

namespace esphome::modbus {

namespace {

// A server device backed by a small coil array: reads deliver the stored bits, writes apply them.
class CoilDevice : public ModbusServerDevice {
 public:
  explicit CoilDevice(uint8_t address) { this->set_address(address); }

  ResponseStatus on_read_coils(uint16_t start_address, MutablePackedBits bits) override {
    this->read_count++;
    for (uint16_t i = 0; i < bits.size(); i++)
      bits.set(i, this->coils[start_address + i]);
    return std::nullopt;
  }

  ResponseStatus on_write_coils(uint16_t start_address, PackedBits bits) override {
    this->write_count++;
    this->last_write_count = bits.size();
    for (uint16_t i = 0; i < bits.size(); i++)
      this->coils[start_address + i] = bits[i];
    return std::nullopt;
  }

  bool coils[32] = {};
  int read_count{0};
  int write_count{0};
  uint16_t last_write_count{0};
};

// A device with no bit handlers, to exercise the ILLEGAL_FUNCTION defaults.
class NoBitsDevice : public ModbusServerDevice {
 public:
  explicit NoBitsDevice(uint8_t address) { this->set_address(address); }
};

// Distinguishes the two bit-read entry points: each fills a different pattern and counts its calls, so a
// test can prove FC 0x01 vs 0x02 dispatch routes to the right handler (and not merely that bits came back).
class DualReadDevice : public ModbusServerDevice {
 public:
  explicit DualReadDevice(uint8_t address) { this->set_address(address); }

  ResponseStatus on_read_coils(uint16_t start_address, MutablePackedBits bits) override {
    this->coil_reads++;
    bits.set(0, true);  // pattern 0x01
    return std::nullopt;
  }
  ResponseStatus on_read_discrete_inputs(uint16_t start_address, MutablePackedBits bits) override {
    this->discrete_reads++;
    bits.set(1, true);  // pattern 0x02
    return std::nullopt;
  }

  int coil_reads{0};
  int discrete_reads{0};
};

// Overrides only on_read_bits() - the shared fallback the header documents that on_read_coils() and
// on_read_discrete_inputs() default to. Both FC 0x01 and FC 0x02 must reach it.
class BitsOnlyDevice : public ModbusServerDevice {
 public:
  explicit BitsOnlyDevice(uint8_t address) { this->set_address(address); }
  ResponseStatus on_read_bits(uint16_t start_address, MutablePackedBits bits) override {
    this->calls++;
    bits.set(0, true);  // set bit 0 so the response proves the fallback ran
    return std::nullopt;
  }
  int calls{0};
};

using testing::RecordingUART;

// Exposes the client-frame parser so a fully CRC-framed request can be pushed through the hub.
class TestServerHub : public ModbusServerHub {
 public:
  bool tx_blocked() override { return false; }

  void prime_send_timestamps_for_test() {
    uint32_t now = millis();
    this->last_modbus_byte_ = now;
    this->last_send_ = now;
  }

  bool process_full_client_frame_for_test(uint8_t address, uint8_t function_code, const uint8_t *pdu_data,
                                          size_t pdu_data_len) {
    this->rx_buffer_.clear();
    this->rx_buffer_.reserve(pdu_data_len + 4);
    this->rx_buffer_.push_back(address);
    this->rx_buffer_.push_back(function_code);
    this->rx_buffer_.insert(this->rx_buffer_.end(), pdu_data, pdu_data + pdu_data_len);
    uint16_t crc = crc16(this->rx_buffer_.data(), this->rx_buffer_.size());
    this->rx_buffer_.push_back(crc & 0xFF);
    this->rx_buffer_.push_back(crc >> 8);
    return this->parse_modbus_client_frame_();
  }
};

struct CoilFixture {
  CoilFixture() {
    hub.set_uart_parent(&uart);
    hub.prime_send_timestamps_for_test();
    hub.register_device(&device);
  }
  TestServerHub hub;
  RecordingUART uart;
  CoilDevice device{0x02};
};

}  // namespace

// A coil read returns byte count + packed bits, set by the handler directly in the response buffer.
TEST(ModbusServerCoils, ReadCoilsReturnsPackedBits) {
  CoilFixture f;
  f.device.coils[0] = true;
  f.device.coils[2] = true;
  f.device.coils[3] = true;
  f.device.coils[9] = true;

  // FC 0x01: start 0x0000, quantity 10 -> 2 packed bytes
  const uint8_t pdu_data[] = {0x00, 0x00, 0x00, 0x0A};
  ASSERT_TRUE(f.hub.process_full_client_frame_for_test(0x02, static_cast<uint8_t>(FunctionCode::READ_COILS), pdu_data,
                                                       sizeof(pdu_data)));

  EXPECT_EQ(f.device.read_count, 1);
  // Response: address(1) + fc(1) + byte count(1) + packed(2) + CRC(2)
  ASSERT_EQ(f.uart.written.size(), 7u);
  EXPECT_EQ(f.uart.written[0], 0x02);
  EXPECT_EQ(f.uart.written[1], static_cast<uint8_t>(FunctionCode::READ_COILS));
  EXPECT_EQ(f.uart.written[2], 2u);    // byte count
  EXPECT_EQ(f.uart.written[3], 0x0D);  // coils 0,2,3
  EXPECT_EQ(f.uart.written[4], 0x02);  // coil 9 -> bit 1 of byte 1
}

// A device overriding only on_read_bits() - the documented fallback - still serves both FC 0x01 (coils)
// and FC 0x02 (discrete inputs), since on_read_coils()/on_read_discrete_inputs() default to it.
TEST(ModbusServerCoils, ReadBitsFallbackServesBothCoilsAndDiscreteInputs) {
  TestServerHub hub;
  RecordingUART uart;
  hub.set_uart_parent(&uart);
  hub.prime_send_timestamps_for_test();
  BitsOnlyDevice device{0x05};
  hub.register_device(&device);

  const uint8_t pdu_data[] = {0x00, 0x00, 0x00, 0x01};  // start 0x0000, quantity 1

  ASSERT_TRUE(hub.process_full_client_frame_for_test(0x05, static_cast<uint8_t>(FunctionCode::READ_COILS), pdu_data,
                                                     sizeof(pdu_data)));
  EXPECT_EQ(device.calls, 1);
  // address(1) + fc(1) + byte count(1) + packed(1) + CRC(2); bit 0 set -> 0x01
  ASSERT_EQ(uart.written.size(), 6u);
  EXPECT_EQ(uart.written[1], static_cast<uint8_t>(FunctionCode::READ_COILS));
  EXPECT_EQ(uart.written[3], 0x01);

  uart.written.clear();
  ASSERT_TRUE(hub.process_full_client_frame_for_test(0x05, static_cast<uint8_t>(FunctionCode::READ_DISCRETE_INPUTS),
                                                     pdu_data, sizeof(pdu_data)));
  EXPECT_EQ(device.calls, 2);
  ASSERT_EQ(uart.written.size(), 6u);
  EXPECT_EQ(uart.written[1], static_cast<uint8_t>(FunctionCode::READ_DISCRETE_INPUTS));
  EXPECT_EQ(uart.written[3], 0x01);
}

// A multiple-coil write hands the handler the packed wire bytes and echoes the request header.
TEST(ModbusServerCoils, WriteMultipleCoilsAppliesPackedBits) {
  CoilFixture f;

  // FC 0x0F: start 0x0000, quantity 10, byte count 2, packed values 0x0D 0x02
  const uint8_t pdu_data[] = {0x00, 0x00, 0x00, 0x0A, 0x02, 0x0D, 0x02};
  ASSERT_TRUE(f.hub.process_full_client_frame_for_test(0x02, static_cast<uint8_t>(FunctionCode::WRITE_MULTIPLE_COILS),
                                                       pdu_data, sizeof(pdu_data)));

  EXPECT_EQ(f.device.write_count, 1);
  EXPECT_EQ(f.device.last_write_count, 10u);
  EXPECT_TRUE(f.device.coils[0]);
  EXPECT_FALSE(f.device.coils[1]);
  EXPECT_TRUE(f.device.coils[2]);
  EXPECT_TRUE(f.device.coils[3]);
  EXPECT_TRUE(f.device.coils[9]);
  EXPECT_FALSE(f.device.coils[10]);
  // Response echoes start address + quantity: address(1) + fc(1) + start(2) + quantity(2) + CRC(2)
  ASSERT_EQ(f.uart.written.size(), 8u);
  EXPECT_EQ(f.uart.written[1], static_cast<uint8_t>(FunctionCode::WRITE_MULTIPLE_COILS));
}

// A single-coil write (FC 0x05) is normalized to a one-bit packed buffer.
TEST(ModbusServerCoils, WriteSingleCoilNormalizedToOneBit) {
  CoilFixture f;

  const uint8_t pdu_on[] = {0x00, 0x03, 0xFF, 0x00};  // coil 3 ON
  ASSERT_TRUE(f.hub.process_full_client_frame_for_test(0x02, static_cast<uint8_t>(FunctionCode::WRITE_SINGLE_COIL),
                                                       pdu_on, sizeof(pdu_on)));
  EXPECT_EQ(f.device.last_write_count, 1u);
  EXPECT_TRUE(f.device.coils[3]);

  f.uart.written.clear();
  f.hub.prime_send_timestamps_for_test();
  const uint8_t pdu_off[] = {0x00, 0x03, 0x00, 0x00};  // coil 3 OFF
  ASSERT_TRUE(f.hub.process_full_client_frame_for_test(0x02, static_cast<uint8_t>(FunctionCode::WRITE_SINGLE_COIL),
                                                       pdu_off, sizeof(pdu_off)));
  EXPECT_FALSE(f.device.coils[3]);
  EXPECT_EQ(f.device.write_count, 2);
}

// An invalid single-coil value (not 0xFF00/0x0000) is rejected with ILLEGAL_DATA_VALUE, no write.
TEST(ModbusServerCoils, InvalidSingleCoilValueRejected) {
  CoilFixture f;

  const uint8_t pdu_data[] = {0x00, 0x03, 0x12, 0x34};
  ASSERT_TRUE(f.hub.process_full_client_frame_for_test(0x02, static_cast<uint8_t>(FunctionCode::WRITE_SINGLE_COIL),
                                                       pdu_data, sizeof(pdu_data)));

  EXPECT_EQ(f.device.write_count, 0);
  ASSERT_EQ(f.uart.written.size(), 5u);  // one exception frame
  EXPECT_EQ(f.uart.written[1], static_cast<uint8_t>(FunctionCode::WRITE_SINGLE_COIL) | 0x80);
  EXPECT_EQ(f.uart.written[2], static_cast<uint8_t>(ExceptionCode::ILLEGAL_DATA_VALUE));
}

// Read quantity validation lives in the shared read-request parser, so the register and bit reads cannot
// drift apart. These pin both ends of the range for coils; the register case below pins that the same
// parser is on that path too.
TEST(ModbusServerCoils, ZeroCoilReadQuantityRejected) {
  CoilFixture f;

  // FC 0x01: start 0x0000, quantity 0 - a read of nothing is out of spec.
  const uint8_t pdu_data[] = {0x00, 0x00, 0x00, 0x00};
  ASSERT_TRUE(f.hub.process_full_client_frame_for_test(0x02, static_cast<uint8_t>(FunctionCode::READ_COILS), pdu_data,
                                                       sizeof(pdu_data)));

  EXPECT_EQ(f.device.read_count, 0);
  ASSERT_EQ(f.uart.written.size(), 5u);  // one exception frame
  EXPECT_EQ(f.uart.written[1], static_cast<uint8_t>(FunctionCode::READ_COILS) | 0x80);
  EXPECT_EQ(f.uart.written[2], static_cast<uint8_t>(ExceptionCode::ILLEGAL_DATA_VALUE));
}

TEST(ModbusServerCoils, OverLimitCoilReadQuantityRejected) {
  CoilFixture f;

  // One past MAX_NUM_OF_COILS_TO_READ (2000 = 0x07D0), which no frame could carry anyway.
  const uint8_t pdu_data[] = {0x00, 0x00, 0x07, 0xD1};
  ASSERT_TRUE(f.hub.process_full_client_frame_for_test(0x02, static_cast<uint8_t>(FunctionCode::READ_COILS), pdu_data,
                                                       sizeof(pdu_data)));

  EXPECT_EQ(f.device.read_count, 0);
  ASSERT_EQ(f.uart.written.size(), 5u);
  EXPECT_EQ(f.uart.written[2], static_cast<uint8_t>(ExceptionCode::ILLEGAL_DATA_VALUE));
}

// The register read path shares that parser, so a zero quantity is rejected there identically. Lives
// beside the coil cases deliberately: together they are what stops the shared parser being bypassed on
// one side without the other noticing.
TEST(ModbusServerCoils, ZeroRegisterReadQuantityRejectedByTheSameParser) {
  CoilFixture f;

  const uint8_t pdu_data[] = {0x00, 0x00, 0x00, 0x00};
  ASSERT_TRUE(f.hub.process_full_client_frame_for_test(0x02, static_cast<uint8_t>(FunctionCode::READ_HOLDING_REGISTERS),
                                                       pdu_data, sizeof(pdu_data)));

  ASSERT_EQ(f.uart.written.size(), 5u);
  EXPECT_EQ(f.uart.written[1], static_cast<uint8_t>(FunctionCode::READ_HOLDING_REGISTERS) | 0x80);
  EXPECT_EQ(f.uart.written[2], static_cast<uint8_t>(ExceptionCode::ILLEGAL_DATA_VALUE));
}

// A device without bit handlers rejects coil requests with ILLEGAL_FUNCTION via the defaults.
TEST(ModbusServerCoils, UnhandledCoilReadIsIllegalFunction) {
  TestServerHub hub;
  RecordingUART uart;
  hub.set_uart_parent(&uart);
  hub.prime_send_timestamps_for_test();
  NoBitsDevice device(0x02);
  hub.register_device(&device);

  const uint8_t pdu_data[] = {0x00, 0x00, 0x00, 0x08};
  ASSERT_TRUE(hub.process_full_client_frame_for_test(0x02, static_cast<uint8_t>(FunctionCode::READ_COILS), pdu_data,
                                                     sizeof(pdu_data)));

  ASSERT_EQ(uart.written.size(), 5u);
  EXPECT_EQ(uart.written[1], static_cast<uint8_t>(FunctionCode::READ_COILS) | 0x80);
  EXPECT_EQ(uart.written[2], static_cast<uint8_t>(ExceptionCode::ILLEGAL_FUNCTION));
}

// The view contracts are enforced, not merely documented: bytes() returns exactly ceil(size()/8) bytes
// even over a larger buffer (forwarding it can never leak trailing buffer content), and set() drops
// out-of-range bits instead of writing past the span (on the server read path that span wraps a stack
// response buffer).
TEST(ModbusServerCoils, PackedBitsViewContractsEnforced) {
  uint8_t buf[8] = {};
  PackedBits view(buf, 10);  // 10 bits -> 2 bytes, over an 8-byte buffer
  EXPECT_EQ(view.bytes().size(), 2u);

  PackedBits short_view(std::span<const uint8_t>(buf, 1), 10);  // contract-violating: 10 bits over 1 byte
  EXPECT_EQ(short_view.bytes().size(), 1u);  // clamped to the real span, not a fabricated 2-byte span

  MutablePackedBits bits(std::span<uint8_t>(buf, 2), 10);
  bits.set(9, true);    // in range: lands in byte 1
  bits.set(10, true);   // out of range: dropped
  bits.set(300, true);  // far out of range: dropped, no write past the span
  EXPECT_EQ(buf[1], 0x02);
  for (size_t i = 2; i < sizeof(buf); i++)
    EXPECT_EQ(buf[i], 0) << "byte " << i;
}

// FC 0x02 must dispatch to on_read_discrete_inputs, not on_read_coils: the two handlers fill different
// patterns, so a swapped dispatch would fail on both the counters and the wire bytes.
TEST(ModbusServerCoils, ReadDiscreteInputsDispatchesToItsOwnHandler) {
  TestServerHub hub;
  RecordingUART uart;
  hub.set_uart_parent(&uart);
  hub.prime_send_timestamps_for_test();
  DualReadDevice device(0x02);
  hub.register_device(&device);

  const uint8_t pdu_data[] = {0x00, 0x00, 0x00, 0x08};
  ASSERT_TRUE(hub.process_full_client_frame_for_test(0x02, static_cast<uint8_t>(FunctionCode::READ_DISCRETE_INPUTS),
                                                     pdu_data, sizeof(pdu_data)));

  EXPECT_EQ(device.discrete_reads, 1);
  EXPECT_EQ(device.coil_reads, 0);
  ASSERT_GE(uart.written.size(), 4u);
  EXPECT_EQ(uart.written[1], static_cast<uint8_t>(FunctionCode::READ_DISCRETE_INPUTS));
  EXPECT_EQ(uart.written[3], 0x02);  // the discrete handler's pattern, not the coil handler's

  uart.written.clear();
  ASSERT_TRUE(hub.process_full_client_frame_for_test(0x02, static_cast<uint8_t>(FunctionCode::READ_COILS), pdu_data,
                                                     sizeof(pdu_data)));
  EXPECT_EQ(device.coil_reads, 1);
  EXPECT_EQ(device.discrete_reads, 1);
  ASSERT_GE(uart.written.size(), 4u);
  EXPECT_EQ(uart.written[3], 0x01);
}

// The write-side ILLEGAL_FUNCTION defaults: a device without bit handlers rejects coil writes too
// (single and multiple), mirroring the read-side default already covered above.
TEST(ModbusServerCoils, UnhandledCoilWriteIsIllegalFunction) {
  TestServerHub hub;
  RecordingUART uart;
  hub.set_uart_parent(&uart);
  hub.prime_send_timestamps_for_test();
  NoBitsDevice device(0x02);
  hub.register_device(&device);

  const uint8_t single[] = {0x00, 0x03, 0xFF, 0x00};
  ASSERT_TRUE(hub.process_full_client_frame_for_test(0x02, static_cast<uint8_t>(FunctionCode::WRITE_SINGLE_COIL),
                                                     single, sizeof(single)));
  ASSERT_EQ(uart.written.size(), 5u);
  EXPECT_EQ(uart.written[1], static_cast<uint8_t>(FunctionCode::WRITE_SINGLE_COIL) | 0x80);
  EXPECT_EQ(uart.written[2], static_cast<uint8_t>(ExceptionCode::ILLEGAL_FUNCTION));

  uart.written.clear();
  const uint8_t multiple[] = {0x00, 0x00, 0x00, 0x08, 0x01, 0xAA};
  ASSERT_TRUE(hub.process_full_client_frame_for_test(0x02, static_cast<uint8_t>(FunctionCode::WRITE_MULTIPLE_COILS),
                                                     multiple, sizeof(multiple)));
  ASSERT_EQ(uart.written.size(), 5u);
  EXPECT_EQ(uart.written[1], static_cast<uint8_t>(FunctionCode::WRITE_MULTIPLE_COILS) | 0x80);
  EXPECT_EQ(uart.written[2], static_cast<uint8_t>(ExceptionCode::ILLEGAL_FUNCTION));
}

// FC 0x0F with a byte count that does not match ceil(quantity / 8) is ILLEGAL_DATA_VALUE and never
// reaches the handler.
TEST(ModbusServerCoils, WriteCoilsByteCountMismatchRejected) {
  CoilFixture f;

  // quantity 10 needs 2 bytes; claim 1
  const uint8_t pdu_data[] = {0x00, 0x00, 0x00, 0x0A, 0x01, 0xFF};
  ASSERT_TRUE(f.hub.process_full_client_frame_for_test(0x02, static_cast<uint8_t>(FunctionCode::WRITE_MULTIPLE_COILS),
                                                       pdu_data, sizeof(pdu_data)));

  ASSERT_EQ(f.uart.written.size(), 5u);
  EXPECT_EQ(f.uart.written[1], static_cast<uint8_t>(FunctionCode::WRITE_MULTIPLE_COILS) | 0x80);
  EXPECT_EQ(f.uart.written[2], static_cast<uint8_t>(ExceptionCode::ILLEGAL_DATA_VALUE));
  EXPECT_EQ(f.device.write_count, 0);
}

// A coil range that runs past address 0xFFFF is ILLEGAL_DATA_ADDRESS and never reaches the handler.
TEST(ModbusServerCoils, CoilAddressRangeOverflowRejected) {
  CoilFixture f;

  // start 0xFFF8, quantity 16 -> 0x10008 > 0x10000
  const uint8_t pdu_data[] = {0xFF, 0xF8, 0x00, 0x10};
  ASSERT_TRUE(f.hub.process_full_client_frame_for_test(0x02, static_cast<uint8_t>(FunctionCode::READ_COILS), pdu_data,
                                                       sizeof(pdu_data)));

  ASSERT_EQ(f.uart.written.size(), 5u);
  EXPECT_EQ(f.uart.written[1], static_cast<uint8_t>(FunctionCode::READ_COILS) | 0x80);
  EXPECT_EQ(f.uart.written[2], static_cast<uint8_t>(ExceptionCode::ILLEGAL_DATA_ADDRESS));
  EXPECT_EQ(f.device.read_count, 0);
}

}  // namespace esphome::modbus
