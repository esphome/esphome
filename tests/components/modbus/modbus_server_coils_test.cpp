#include <gtest/gtest.h>

#include <cstdint>
#include <new>
#include <span>
#include <vector>

#include "common.h"
#include "esphome/components/modbus/modbus.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::modbus {

namespace {

void ensure_test_app_constructed() {
  static bool app_constructed = false;
  if (!app_constructed) {
    new (&App) Application();
    app_constructed = true;
  }
}

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
    ensure_test_app_constructed();
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
  ASSERT_TRUE(f.hub.process_full_client_frame_for_test(0x02, static_cast<uint8_t>(ModbusFunctionCode::READ_COILS),
                                                       pdu_data, sizeof(pdu_data)));

  EXPECT_EQ(f.device.read_count, 1);
  // Response: address(1) + fc(1) + byte count(1) + packed(2) + CRC(2)
  ASSERT_EQ(f.uart.written.size(), 7u);
  EXPECT_EQ(f.uart.written[0], 0x02);
  EXPECT_EQ(f.uart.written[1], static_cast<uint8_t>(ModbusFunctionCode::READ_COILS));
  EXPECT_EQ(f.uart.written[2], 2u);    // byte count
  EXPECT_EQ(f.uart.written[3], 0x0D);  // coils 0,2,3
  EXPECT_EQ(f.uart.written[4], 0x02);  // coil 9 -> bit 1 of byte 1
}

// A multiple-coil write hands the handler the packed wire bytes and echoes the request header.
TEST(ModbusServerCoils, WriteMultipleCoilsAppliesPackedBits) {
  CoilFixture f;

  // FC 0x0F: start 0x0000, quantity 10, byte count 2, packed values 0x0D 0x02
  const uint8_t pdu_data[] = {0x00, 0x00, 0x00, 0x0A, 0x02, 0x0D, 0x02};
  ASSERT_TRUE(f.hub.process_full_client_frame_for_test(
      0x02, static_cast<uint8_t>(ModbusFunctionCode::WRITE_MULTIPLE_COILS), pdu_data, sizeof(pdu_data)));

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
  EXPECT_EQ(f.uart.written[1], static_cast<uint8_t>(ModbusFunctionCode::WRITE_MULTIPLE_COILS));
}

// A single-coil write (FC 0x05) is normalized to a one-bit packed buffer.
TEST(ModbusServerCoils, WriteSingleCoilNormalizedToOneBit) {
  CoilFixture f;

  const uint8_t pdu_on[] = {0x00, 0x03, 0xFF, 0x00};  // coil 3 ON
  ASSERT_TRUE(f.hub.process_full_client_frame_for_test(
      0x02, static_cast<uint8_t>(ModbusFunctionCode::WRITE_SINGLE_COIL), pdu_on, sizeof(pdu_on)));
  EXPECT_EQ(f.device.last_write_count, 1u);
  EXPECT_TRUE(f.device.coils[3]);

  f.uart.written.clear();
  f.hub.prime_send_timestamps_for_test();
  const uint8_t pdu_off[] = {0x00, 0x03, 0x00, 0x00};  // coil 3 OFF
  ASSERT_TRUE(f.hub.process_full_client_frame_for_test(
      0x02, static_cast<uint8_t>(ModbusFunctionCode::WRITE_SINGLE_COIL), pdu_off, sizeof(pdu_off)));
  EXPECT_FALSE(f.device.coils[3]);
  EXPECT_EQ(f.device.write_count, 2);
}

// An invalid single-coil value (not 0xFF00/0x0000) is rejected with ILLEGAL_DATA_VALUE, no write.
TEST(ModbusServerCoils, InvalidSingleCoilValueRejected) {
  CoilFixture f;

  const uint8_t pdu_data[] = {0x00, 0x03, 0x12, 0x34};
  ASSERT_TRUE(f.hub.process_full_client_frame_for_test(
      0x02, static_cast<uint8_t>(ModbusFunctionCode::WRITE_SINGLE_COIL), pdu_data, sizeof(pdu_data)));

  EXPECT_EQ(f.device.write_count, 0);
  ASSERT_EQ(f.uart.written.size(), 5u);  // one exception frame
  EXPECT_EQ(f.uart.written[1], static_cast<uint8_t>(ModbusFunctionCode::WRITE_SINGLE_COIL) | 0x80);
  EXPECT_EQ(f.uart.written[2], static_cast<uint8_t>(ModbusExceptionCode::ILLEGAL_DATA_VALUE));
}

// A device without bit handlers rejects coil requests with ILLEGAL_FUNCTION via the defaults.
TEST(ModbusServerCoils, UnhandledCoilReadIsIllegalFunction) {
  ensure_test_app_constructed();
  TestServerHub hub;
  RecordingUART uart;
  hub.set_uart_parent(&uart);
  hub.prime_send_timestamps_for_test();
  NoBitsDevice device(0x02);
  hub.register_device(&device);

  const uint8_t pdu_data[] = {0x00, 0x00, 0x00, 0x08};
  ASSERT_TRUE(hub.process_full_client_frame_for_test(0x02, static_cast<uint8_t>(ModbusFunctionCode::READ_COILS),
                                                     pdu_data, sizeof(pdu_data)));

  ASSERT_EQ(uart.written.size(), 5u);
  EXPECT_EQ(uart.written[1], static_cast<uint8_t>(ModbusFunctionCode::READ_COILS) | 0x80);
  EXPECT_EQ(uart.written[2], static_cast<uint8_t>(ModbusExceptionCode::ILLEGAL_FUNCTION));
}

}  // namespace esphome::modbus
