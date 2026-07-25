#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "esphome/components/modbus/modbus.h"
#include "esphome/core/hal.h"

namespace esphome::modbus {

namespace {

// Modbus broadcast address: a request every device processes and never answers (Modbus 4.1).
constexpr uint8_t BROADCAST_ADDRESS = 0x00;

// A server device that records the writes the hub routes to it.
class RecordingDevice : public ModbusServerDevice {
 public:
  explicit RecordingDevice(uint8_t address) { this->set_address(address); }

  ResponseStatus on_write_registers(uint16_t start_address, const RegisterValues &registers) override {
    this->write_count++;
    this->last_start_address = start_address;
    this->last_values.assign(registers.begin(), registers.end());
    return std::nullopt;  // return value is ignored for broadcasts, which are never answered
  }

  int write_count{0};
  uint16_t last_start_address{0};
  std::vector<uint16_t> last_values;
};

// A UART that records every byte written so the test can assert the hub sends no reply.
class RecordingUART : public uart::UARTComponent {
 public:
  RecordingUART() { this->set_baud_rate(9600); }

  void write_array(const uint8_t *data, size_t len) override {
    this->written.insert(this->written.end(), data, data + len);
  }
  bool peek_byte(uint8_t *data) override { return false; }
  bool read_array(uint8_t *data, size_t len) override { return false; }
  size_t available() override { return 0; }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS; }
  void check_logger_conflict() override {}
  // ESP32-only: naming the ESP8266 macro would grep this gtest file into that clang-tidy scan.
#ifdef USE_ESP32
  void load_settings(bool dump_config) override {}
#endif

  std::vector<uint8_t> written;
};

// Exposes the protected client-frame entry point so a full broadcast frame can be routed in tests.
class TestServerHub : public ModbusServerHub {
 public:
  using ModbusServerHub::process_modbus_client_frame_;

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

}  // namespace

// A broadcast (address 0) single-register write reaches every registered device and is not answered.
TEST(ModbusBroadcast, SingleRegisterWriteReachesAllDevicesWithoutReply) {
  TestServerHub hub;
  RecordingUART uart;
  hub.set_uart_parent(&uart);

  RecordingDevice device_a(0x02);
  RecordingDevice device_b(0x03);
  hub.register_device(&device_a);
  hub.register_device(&device_b);

  // FC 0x06 payload: start address 0x9D31, value 0x00A5 (big-endian, no address/CRC).
  const uint8_t pdu_data[] = {0x9D, 0x31, 0x00, 0xA5};
  hub.process_modbus_client_frame_(BROADCAST_ADDRESS, static_cast<uint8_t>(FunctionCode::WRITE_SINGLE_REGISTER),
                                   pdu_data);

  for (RecordingDevice *device : {&device_a, &device_b}) {
    EXPECT_EQ(device->write_count, 1);
    EXPECT_EQ(device->last_start_address, 0x9D31);
    ASSERT_EQ(device->last_values.size(), 1u);
    EXPECT_EQ(device->last_values[0], 0x00A5);
  }
  EXPECT_TRUE(uart.written.empty());  // broadcasts are never answered
}

// A broadcast multi-register write is decoded and delivered to every device, still without a reply.
TEST(ModbusBroadcast, MultipleRegisterWriteReachesAllDevicesWithoutReply) {
  TestServerHub hub;
  RecordingUART uart;
  hub.set_uart_parent(&uart);

  RecordingDevice device_a(0x02);
  RecordingDevice device_b(0x03);
  hub.register_device(&device_a);
  hub.register_device(&device_b);

  // FC 0x10 payload: start 0x9D31, quantity 2, byte count 4, values 0x0102 and 0x0304.
  const uint8_t pdu_data[] = {0x9D, 0x31, 0x00, 0x02, 0x04, 0x01, 0x02, 0x03, 0x04};
  hub.process_modbus_client_frame_(BROADCAST_ADDRESS, static_cast<uint8_t>(FunctionCode::WRITE_MULTIPLE_REGISTERS),
                                   pdu_data);

  for (RecordingDevice *device : {&device_a, &device_b}) {
    EXPECT_EQ(device->write_count, 1);
    EXPECT_EQ(device->last_start_address, 0x9D31);
    ASSERT_EQ(device->last_values.size(), 2u);
    EXPECT_EQ(device->last_values[0], 0x0102);
    EXPECT_EQ(device->last_values[1], 0x0304);
  }
  EXPECT_TRUE(uart.written.empty());
}

// A read broadcast is meaningless (it would need a reply), so nothing is dispatched and nothing is sent.
TEST(ModbusBroadcast, ReadFunctionCodeIsIgnoredAndProducesNoReply) {
  TestServerHub hub;
  RecordingUART uart;
  hub.set_uart_parent(&uart);

  RecordingDevice device(0x02);
  hub.register_device(&device);

  // FC 0x03 payload: start 0x0000, quantity 2. Reads cannot be broadcast.
  const uint8_t pdu_data[] = {0x00, 0x00, 0x00, 0x02};
  hub.process_modbus_client_frame_(BROADCAST_ADDRESS, static_cast<uint8_t>(FunctionCode::READ_HOLDING_REGISTERS),
                                   pdu_data);

  EXPECT_EQ(device.write_count, 0);   // no device was written
  EXPECT_TRUE(uart.written.empty());  // and the broadcast address is never answered
}

// An invalid broadcast write is silently dropped: no writes dispatched and no exception reply sent.
TEST(ModbusBroadcast, InvalidMultipleWriteBroadcastProducesNoWriteAndNoReply) {
  TestServerHub hub;
  RecordingUART uart;
  hub.set_uart_parent(&uart);

  RecordingDevice device_a(0x02);
  RecordingDevice device_b(0x03);
  hub.register_device(&device_a);
  hub.register_device(&device_b);

  // FC 0x10 payload: quantity 2 but byte count 2 (should be 4), so parsing fails.
  const uint8_t pdu_data[] = {0x9D, 0x31, 0x00, 0x02, 0x02, 0x01, 0x02};
  hub.process_modbus_client_frame_(BROADCAST_ADDRESS, static_cast<uint8_t>(FunctionCode::WRITE_MULTIPLE_REGISTERS),
                                   pdu_data);

  EXPECT_EQ(device_a.write_count, 0);
  EXPECT_EQ(device_b.write_count, 0);
  EXPECT_TRUE(uart.written.empty());
}

// A unicast out-of-range write sends exactly one exception frame on the wire.
TEST(ModbusBroadcast, UnicastOutOfRangeWriteSendsSingleExceptionFrame) {
  TestServerHub hub;
  RecordingUART uart;
  hub.set_uart_parent(&uart);
  hub.prime_send_timestamps_for_test();

  RecordingDevice device(0x02);
  hub.register_device(&device);

  // FC 0x10 payload: start 0xFFFF, quantity 2, byte count 4, values valid but address range overflows.
  const uint8_t pdu_data[] = {0xFF, 0xFF, 0x00, 0x02, 0x04, 0x01, 0x02, 0x03, 0x04};
  ASSERT_TRUE(hub.process_full_client_frame_for_test(0x02, static_cast<uint8_t>(FunctionCode::WRITE_MULTIPLE_REGISTERS),
                                                     pdu_data, sizeof(pdu_data)));

  EXPECT_EQ(device.write_count, 0);
  ASSERT_EQ(uart.written.size(), 5u);
  EXPECT_EQ(uart.written[0], 0x02);  // server address
  EXPECT_EQ(uart.written[1], static_cast<uint8_t>(FunctionCode::WRITE_MULTIPLE_REGISTERS) | 0x80);
  EXPECT_EQ(uart.written[2], static_cast<uint8_t>(ExceptionCode::ILLEGAL_DATA_ADDRESS));
}

}  // namespace esphome::modbus
