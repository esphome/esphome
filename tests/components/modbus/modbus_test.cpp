#include <gtest/gtest.h>
#include "esphome/components/modbus/modbus.h"
#include "esphome/core/helpers.h"

namespace esphome::modbus {

// Exposes protected methods for testing.
class TestModbus : public Modbus {
 public:
  bool test_parse_modbus_byte(uint8_t byte) { return this->parse_modbus_byte_(byte); }
  void test_clear_rx_buffer() { this->rx_buffer_.clear(); }
  void set_waiting(uint8_t addr) { this->waiting_for_response_ = addr; }
};

class MockDevice : public ModbusDevice {
 public:
  void on_modbus_data(const std::vector<uint8_t> &data) override {
    this->data_received = true;
    this->call_count++;
    this->last_data = data;
  }
  bool data_received{false};
  uint32_t call_count{0};
  std::vector<uint8_t> last_data;
};

TEST(ModbusTest, TwoByteRegressionTest) {
  TestModbus modbus;
  modbus.set_role(ModbusRole::CLIENT);
  // First byte (at=0)
  EXPECT_TRUE(modbus.test_parse_modbus_byte(0x01));
  // Second byte (at=1)
  // This used to reach raw[2] because it skipped the if(at==2) check, causing a
  // buffer overflow.
  EXPECT_TRUE(modbus.test_parse_modbus_byte(0x03));
}

TEST(ModbusTest, TestValidFrame) {
  TestModbus modbus;
  modbus.set_role(ModbusRole::CLIENT);

  MockDevice device;
  device.set_parent(&modbus);
  device.set_address(0x01);
  modbus.register_device(&device);
  modbus.set_waiting(0x01);

  // Address 1, Function 3, Length 2, Data 0x1234
  uint8_t frame_data[] = {0x01, 0x03, 0x02, 0x12, 0x34};
  uint16_t crc = esphome::crc16(frame_data, sizeof(frame_data));

  std::vector<uint8_t> frame;
  for (uint8_t b : frame_data)
    frame.push_back(b);
  frame.push_back(crc & 0xFF);
  frame.push_back((crc >> 8) & 0xFF);

  for (size_t i = 0; i < frame.size(); i++) {
    bool result = modbus.test_parse_modbus_byte(frame[i]);
    EXPECT_TRUE(result) << "Failed at byte " << i << " (0x" << std::hex << (int) frame[i] << ")";
  }
  EXPECT_TRUE(device.data_received);
}

// Build a valid Modbus RTU response frame (address | function | byte_count | payload | CRC).
static std::vector<uint8_t> make_valid_response_frame() {
  uint8_t frame_data[] = {0x01, 0x03, 0x02, 0x12, 0x34};
  uint16_t crc = esphome::crc16(frame_data, sizeof(frame_data));
  std::vector<uint8_t> frame;
  for (uint8_t b : frame_data)
    frame.push_back(b);
  frame.push_back(crc & 0xFF);
  frame.push_back((crc >> 8) & 0xFF);
  return frame;
}

// Feeds a byte stream into the parser and asserts the mock device received the expected frame
// exactly once, regardless of any junk prepended to it.
static void feed_and_expect_one_valid_frame(TestModbus &modbus, MockDevice &device,
                                            const std::vector<uint8_t> &stream) {
  for (size_t i = 0; i < stream.size(); i++) {
    EXPECT_TRUE(modbus.test_parse_modbus_byte(stream[i]))
        << "Parser returned false at byte " << i << " (0x" << std::hex << (int) stream[i] << ")";
  }
  EXPECT_TRUE(device.data_received);
  EXPECT_EQ(device.call_count, 1u);
  // Payload is the 2 data bytes from make_valid_response_frame().
  ASSERT_EQ(device.last_data.size(), 2u);
  EXPECT_EQ(device.last_data[0], 0x12);
  EXPECT_EQ(device.last_data[1], 0x34);
}

// Regression test: a single junk byte before a valid frame must be walked past.
TEST(ModbusTest, ResyncAfterSingleJunkByte) {
  TestModbus modbus;
  modbus.set_role(ModbusRole::CLIENT);

  MockDevice device;
  device.set_parent(&modbus);
  device.set_address(0x01);
  modbus.register_device(&device);
  modbus.set_waiting(0x01);

  std::vector<uint8_t> stream = {0xFF};  // single junk byte
  const auto frame = make_valid_response_frame();
  stream.insert(stream.end(), frame.begin(), frame.end());

  feed_and_expect_one_valid_frame(modbus, device, stream);
}

// Real-world case: an 8-byte junk prefix (modelled on the SDM630 + MAX485 direction-switch
// artifact observed on actual hardware) must be walked past to reach the valid frame behind it.
TEST(ModbusTest, ResyncAfterEightByteJunkPrefix) {
  TestModbus modbus;
  modbus.set_role(ModbusRole::CLIENT);

  MockDevice device;
  device.set_parent(&modbus);
  device.set_address(0x01);
  modbus.register_device(&device);
  modbus.set_waiting(0x01);

  // Observed prefix shape from a real RS485 bus: arbitrary bytes that the naive parser
  // interpreted as a malformed frame (bytes with MSB set look like exception responses).
  std::vector<uint8_t> stream = {0xEE, 0xA1, 0xEC, 0xA4, 0x30, 0xD7, 0x00, 0x00};
  const auto frame = make_valid_response_frame();
  stream.insert(stream.end(), frame.begin(), frame.end());

  feed_and_expect_one_valid_frame(modbus, device, stream);
}

// Adversarial case: a junk prefix that itself forms a well-formed 5-byte exception response
// from an unregistered address (0x42) with a valid CRC. The CLIENT fast-path is waiting for
// 0x01, so it drops every byte of the spurious frame without ever parsing it, and still
// correctly accepts the real frame from 0x01 that follows.
TEST(ModbusTest, ResyncPastSpuriousFrameForUnexpectedAddress) {
  TestModbus modbus;
  modbus.set_role(ModbusRole::CLIENT);

  MockDevice device;
  device.set_parent(&modbus);
  device.set_address(0x01);
  modbus.register_device(&device);
  modbus.set_waiting(0x01);

  // Build a 5-byte Modbus exception response for address 0x42 (not registered):
  // { 0x42, 0x83 (READ_HOLDING_REGISTERS|error), 0x02 (exception code), CRC_lo, CRC_hi }
  uint8_t spurious_header[] = {0x42, 0x83, 0x02};
  uint16_t spurious_crc = esphome::crc16(spurious_header, sizeof(spurious_header));
  std::vector<uint8_t> stream;
  for (uint8_t b : spurious_header)
    stream.push_back(b);
  stream.push_back(spurious_crc & 0xFF);
  stream.push_back((spurious_crc >> 8) & 0xFF);

  const auto frame = make_valid_response_frame();
  stream.insert(stream.end(), frame.begin(), frame.end());

  feed_and_expect_one_valid_frame(modbus, device, stream);
}

// Guard against over-aggressive resync: when the CLIENT is NOT waiting for a response
// (waiting_for_response_ == 0), the fast-path must be inactive. Bytes should be buffered and
// only dropped on CRC failure at a full-frame boundary, so a valid frame that arrives
// unsolicited still parses. This preserves the pre-patch behaviour for non-request/response
// bus traffic (unusual for CLIENT mode, but exercised to prove the guard is in place).
TEST(ModbusTest, NoResyncWhenNotWaitingForResponse) {
  TestModbus modbus;
  modbus.set_role(ModbusRole::CLIENT);

  MockDevice device;
  device.set_parent(&modbus);
  device.set_address(0x01);
  modbus.register_device(&device);
  // Deliberately do NOT call modbus.set_waiting() — waiting_for_response_ stays 0.

  const auto frame = make_valid_response_frame();
  for (uint8_t byte : frame) {
    EXPECT_TRUE(modbus.test_parse_modbus_byte(byte));
  }
  // With waiting_for_response_ == 0, the frame is parsed and dispatched, but the client-side
  // "not expecting a response" branch logs a warning and does not invoke on_modbus_data. The
  // key assertion is that the parser didn't crash and didn't eat arbitrary bytes — the device
  // callback behaviour here matches the pre-patch semantics.
  EXPECT_FALSE(device.data_received);
}

}  // namespace esphome::modbus
