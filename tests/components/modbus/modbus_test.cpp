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
  void on_modbus_data(const std::vector<uint8_t> &data) override { this->data_received = true; }
  bool data_received{false};
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

}  // namespace esphome::modbus
