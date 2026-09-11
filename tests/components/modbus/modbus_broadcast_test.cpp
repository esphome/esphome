#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "common.h"
#include "esphome/components/modbus/modbus.h"

namespace esphome::modbus {

namespace {

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

// A server device that records the coil writes the hub routes to it. Coils arrive as a PackedBits view
// over the hub's buffers, so the bits are copied out here rather than the view retained.
class RecordingCoilDevice : public ModbusServerDevice {
 public:
  explicit RecordingCoilDevice(uint8_t address) { this->set_address(address); }

  ResponseStatus on_write_coils(uint16_t start_address, PackedBits bits) override {
    this->write_count++;
    this->last_start_address = start_address;
    this->last_bits.clear();
    for (uint16_t i = 0; i != bits.size(); i++) {
      this->last_bits.push_back(bits[i]);
    }
    return std::nullopt;  // return value is ignored for broadcasts, which are never answered
  }

  int write_count{0};
  uint16_t last_start_address{0};
  std::vector<bool> last_bits;
};

// A server device that rejects every write, to exercise the broadcast dispatch loop's rejection branch.
class RejectingDevice : public ModbusServerDevice {
 public:
  explicit RejectingDevice(uint8_t address) { this->set_address(address); }

  ResponseStatus on_write_registers(uint16_t start_address, const RegisterValues &registers) override {
    this->write_count++;
    return ExceptionCode::ILLEGAL_DATA_ADDRESS;
  }

  int write_count{0};
};

// Drives full frames through the server hub's receive path in tests.
class TestServerHub : public ModbusServerHub {
 public:
  bool tx_blocked() override { return false; }

  // Builds a complete client frame (address + FC + pdu + CRC) and runs the full receive-side parser
  // (parse_modbus_frames), so the expecting-peer-response routing is exercised, not just the frame parser
  // below it. Returns true once the buffer has fully drained.
  bool run_receive_parser_for_test(uint8_t address, uint8_t function_code, const uint8_t *pdu_data,
                                   size_t pdu_data_len) {
    this->rx_buffer_.clear();
    this->rx_buffer_.reserve(pdu_data_len + 4);
    this->rx_buffer_.push_back(address);
    this->rx_buffer_.push_back(function_code);
    this->rx_buffer_.insert(this->rx_buffer_.end(), pdu_data, pdu_data + pdu_data_len);
    uint16_t crc = crc16(this->rx_buffer_.data(), this->rx_buffer_.size());
    this->rx_buffer_.push_back(crc & 0xFF);
    this->rx_buffer_.push_back(crc >> 8);
    this->parse_modbus_frames();
    return this->rx_buffer_.empty();
  }
};

}  // namespace

using testing::RecordingUART;

// A broadcast (address 0) single-register write reaches every registered device and is not answered.
// Driven through the full receive parser (parse_modbus_frames) so the address-0 routing -- frame length,
// CRC, and client-vs-broadcast dispatch -- is exercised, not just the handler below it.
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
  ASSERT_TRUE(hub.run_receive_parser_for_test(
      BROADCAST_ADDRESS, static_cast<uint8_t>(FunctionCode::WRITE_SINGLE_REGISTER), pdu_data, sizeof(pdu_data)));

  for (RecordingDevice *device : {&device_a, &device_b}) {
    EXPECT_EQ(device->write_count, 1);
    EXPECT_EQ(device->last_start_address, 0x9D31);
    ASSERT_EQ(device->last_values.size(), 1u);
    EXPECT_EQ(device->last_values[0], 0x00A5);
  }
  EXPECT_TRUE(uart.written.empty());  // broadcasts are never answered
}

// A single-register broadcast (FC 0x06) must still reach every device when the hub is mid-way through
// waiting for a peer's response. Its frame length matches a response frame, so without the address-0 guard
// in parse_modbus_frames() it would be swallowed by the response parser instead of being dispatched.
TEST(ModbusBroadcast, SingleRegisterBroadcastDispatchedWhileExpectingPeerResponse) {
  TestServerHub hub;
  RecordingUART uart;
  hub.set_uart_parent(&uart);

  RecordingDevice device_a(0x02);
  RecordingDevice device_b(0x03);
  hub.register_device(&device_a);
  hub.register_device(&device_b);

  // A unicast write addressed to an unregistered peer (0x09) leaves the hub expecting that peer's response.
  const uint8_t peer_pdu[] = {0x00, 0x10, 0x00, 0x2A};
  ASSERT_TRUE(hub.run_receive_parser_for_test(0x09, static_cast<uint8_t>(FunctionCode::WRITE_SINGLE_REGISTER), peer_pdu,
                                              sizeof(peer_pdu)));
  ASSERT_EQ(device_a.write_count, 0);  // the peer request is not for our devices
  ASSERT_EQ(device_b.write_count, 0);

  // The broadcast that follows must still be delivered to every device, and still without a reply.
  const uint8_t pdu_data[] = {0x9D, 0x31, 0x00, 0xA5};
  ASSERT_TRUE(hub.run_receive_parser_for_test(
      BROADCAST_ADDRESS, static_cast<uint8_t>(FunctionCode::WRITE_SINGLE_REGISTER), pdu_data, sizeof(pdu_data)));

  for (RecordingDevice *device : {&device_a, &device_b}) {
    EXPECT_EQ(device->write_count, 1);
    EXPECT_EQ(device->last_start_address, 0x9D31);
    ASSERT_EQ(device->last_values.size(), 1u);
    EXPECT_EQ(device->last_values[0], 0x00A5);
  }
  EXPECT_TRUE(uart.written.empty());  // broadcasts are never answered
}

// After dispatching a broadcast, the hub must not still expect a peer response: a following unicast FC 0x06
// to one of our own devices must be handled, not misparsed as that peer's response and dropped.
TEST(ModbusBroadcast, BroadcastClearsStalePeerExpectation) {
  TestServerHub hub;
  RecordingUART uart;
  hub.set_uart_parent(&uart);

  RecordingDevice device(0x02);
  hub.register_device(&device);

  // A unicast write to an unregistered peer (0x09) leaves the hub expecting that peer's response.
  const uint8_t pdu_data[] = {0x00, 0x10, 0x00, 0x2A};
  ASSERT_TRUE(hub.run_receive_parser_for_test(0x09, static_cast<uint8_t>(FunctionCode::WRITE_SINGLE_REGISTER), pdu_data,
                                              sizeof(pdu_data)));

  // The broadcast that follows clears that expectation as it is dispatched.
  ASSERT_TRUE(hub.run_receive_parser_for_test(
      BROADCAST_ADDRESS, static_cast<uint8_t>(FunctionCode::WRITE_SINGLE_REGISTER), pdu_data, sizeof(pdu_data)));
  ASSERT_EQ(device.write_count, 1);

  // The next unicast FC 0x06 to our own device is handled, not swallowed by the stale expectation.
  ASSERT_TRUE(hub.run_receive_parser_for_test(0x02, static_cast<uint8_t>(FunctionCode::WRITE_SINGLE_REGISTER), pdu_data,
                                              sizeof(pdu_data)));
  EXPECT_EQ(device.write_count, 2);
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
  ASSERT_TRUE(hub.run_receive_parser_for_test(
      BROADCAST_ADDRESS, static_cast<uint8_t>(FunctionCode::WRITE_MULTIPLE_REGISTERS), pdu_data, sizeof(pdu_data)));

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
  ASSERT_TRUE(hub.run_receive_parser_for_test(
      BROADCAST_ADDRESS, static_cast<uint8_t>(FunctionCode::READ_HOLDING_REGISTERS), pdu_data, sizeof(pdu_data)));

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
  ASSERT_TRUE(hub.run_receive_parser_for_test(
      BROADCAST_ADDRESS, static_cast<uint8_t>(FunctionCode::WRITE_MULTIPLE_REGISTERS), pdu_data, sizeof(pdu_data)));

  EXPECT_EQ(device_a.write_count, 0);
  EXPECT_EQ(device_b.write_count, 0);
  EXPECT_TRUE(uart.written.empty());
}

// A device that rejects a broadcast write must not stop dispatch to devices registered after it, and the
// broadcast is still never answered.
TEST(ModbusBroadcast, RejectingDeviceDoesNotStopBroadcastDispatch) {
  TestServerHub hub;
  RecordingUART uart;
  hub.set_uart_parent(&uart);

  RejectingDevice rejecter(0x02);
  RecordingDevice device(0x03);
  hub.register_device(&rejecter);  // registered first, so a rejection happens before the normal device
  hub.register_device(&device);

  // FC 0x06 payload: start address 0x9D31, value 0x00A5 (big-endian, no address/CRC).
  const uint8_t pdu_data[] = {0x9D, 0x31, 0x00, 0xA5};
  ASSERT_TRUE(hub.run_receive_parser_for_test(
      BROADCAST_ADDRESS, static_cast<uint8_t>(FunctionCode::WRITE_SINGLE_REGISTER), pdu_data, sizeof(pdu_data)));

  EXPECT_EQ(rejecter.write_count, 1);  // the rejecting device was still invoked
  EXPECT_EQ(device.write_count, 1);    // and dispatch continued to the device registered after it
  EXPECT_EQ(device.last_start_address, 0x9D31);
  ASSERT_EQ(device.last_values.size(), 1u);
  EXPECT_EQ(device.last_values[0], 0x00A5);
  EXPECT_TRUE(uart.written.empty());  // a broadcast is never answered, even when a device rejects
}

// A unicast out-of-range write sends exactly one exception frame on the wire.
TEST(ModbusBroadcast, UnicastOutOfRangeWriteSendsSingleExceptionFrame) {
  TestServerHub hub;
  RecordingUART uart;
  hub.set_uart_parent(&uart);

  RecordingDevice device(0x02);
  hub.register_device(&device);

  // FC 0x10 payload: start 0xFFFF, quantity 2, byte count 4, values valid but address range overflows.
  const uint8_t pdu_data[] = {0xFF, 0xFF, 0x00, 0x02, 0x04, 0x01, 0x02, 0x03, 0x04};
  ASSERT_TRUE(hub.run_receive_parser_for_test(0x02, static_cast<uint8_t>(FunctionCode::WRITE_MULTIPLE_REGISTERS),
                                              pdu_data, sizeof(pdu_data)));

  EXPECT_EQ(device.write_count, 0);
  ASSERT_EQ(uart.written.size(), 5u);
  EXPECT_EQ(uart.written[0], 0x02);  // server address
  EXPECT_EQ(uart.written[1], static_cast<uint8_t>(FunctionCode::WRITE_MULTIPLE_REGISTERS) | 0x80);
  EXPECT_EQ(uart.written[2], static_cast<uint8_t>(ExceptionCode::ILLEGAL_DATA_ADDRESS));
}

// A broadcast single-coil write (FC 0x05) reaches every device and is not answered. The 2-byte ON value
// is normalized to a one-bit view, so the handler sees the same shape as a multiple-coil write of one.
TEST(ModbusBroadcast, SingleCoilWriteReachesAllDevicesWithoutReply) {
  TestServerHub hub;
  RecordingUART uart;
  hub.set_uart_parent(&uart);

  RecordingCoilDevice device_a(0x02);
  RecordingCoilDevice device_b(0x03);
  hub.register_device(&device_a);
  hub.register_device(&device_b);

  // FC 0x05 payload: coil 0x00AC, value 0xFF00 (ON).
  const uint8_t pdu_data[] = {0x00, 0xAC, 0xFF, 0x00};
  ASSERT_TRUE(hub.run_receive_parser_for_test(BROADCAST_ADDRESS, static_cast<uint8_t>(FunctionCode::WRITE_SINGLE_COIL),
                                              pdu_data, sizeof(pdu_data)));

  for (RecordingCoilDevice *device : {&device_a, &device_b}) {
    EXPECT_EQ(device->write_count, 1);
    EXPECT_EQ(device->last_start_address, 0x00AC);
    ASSERT_EQ(device->last_bits.size(), 1u);
    EXPECT_TRUE(device->last_bits[0]);
  }
  EXPECT_TRUE(uart.written.empty());  // broadcasts are never answered
}

// A broadcast multiple-coil write (FC 0x0F) delivers the packed bits to every device, LSB first.
TEST(ModbusBroadcast, MultipleCoilWriteReachesAllDevicesWithoutReply) {
  TestServerHub hub;
  RecordingUART uart;
  hub.set_uart_parent(&uart);

  RecordingCoilDevice device_a(0x02);
  RecordingCoilDevice device_b(0x03);
  hub.register_device(&device_a);
  hub.register_device(&device_b);

  // FC 0x0F payload: start 0x0013, 10 coils, 2 bytes, 0xCD 0x01 -> bit 0 set, bit 8 set.
  const uint8_t pdu_data[] = {0x00, 0x13, 0x00, 0x0A, 0x02, 0xCD, 0x01};
  ASSERT_TRUE(hub.run_receive_parser_for_test(
      BROADCAST_ADDRESS, static_cast<uint8_t>(FunctionCode::WRITE_MULTIPLE_COILS), pdu_data, sizeof(pdu_data)));

  for (RecordingCoilDevice *device : {&device_a, &device_b}) {
    EXPECT_EQ(device->write_count, 1);
    EXPECT_EQ(device->last_start_address, 0x0013);
    ASSERT_EQ(device->last_bits.size(), 10u);
    EXPECT_TRUE(device->last_bits[0]);   // 0xCD bit 0
    EXPECT_FALSE(device->last_bits[1]);  // 0xCD bit 1
    EXPECT_TRUE(device->last_bits[8]);   // 0x01 bit 0
    EXPECT_FALSE(device->last_bits[9]);  // padding bit
  }
  EXPECT_TRUE(uart.written.empty());
}

// A coil broadcast that fails validation is dropped exactly like a bad register broadcast: no handler
// call and, because broadcasts are never answered, no exception frame either.
TEST(ModbusBroadcast, InvalidCoilBroadcastProducesNoWriteAndNoReply) {
  TestServerHub hub;
  RecordingUART uart;
  hub.set_uart_parent(&uart);

  RecordingCoilDevice device(0x02);
  hub.register_device(&device);

  // Byte count disagrees with the coil quantity: 10 coils need 2 bytes, not 1.
  const uint8_t bad_count[] = {0x00, 0x13, 0x00, 0x0A, 0x01, 0xCD};
  ASSERT_TRUE(hub.run_receive_parser_for_test(
      BROADCAST_ADDRESS, static_cast<uint8_t>(FunctionCode::WRITE_MULTIPLE_COILS), bad_count, sizeof(bad_count)));
  EXPECT_EQ(device.write_count, 0);

  // A single-coil value must be 0x0000 or 0xFF00; anything else is out of spec.
  const uint8_t bad_value[] = {0x00, 0xAC, 0x12, 0x34};
  ASSERT_TRUE(hub.run_receive_parser_for_test(BROADCAST_ADDRESS, static_cast<uint8_t>(FunctionCode::WRITE_SINGLE_COIL),
                                              bad_value, sizeof(bad_value)));
  EXPECT_EQ(device.write_count, 0);

  EXPECT_TRUE(uart.written.empty());
}

}  // namespace esphome::modbus
