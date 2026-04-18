#include <gtest/gtest.h>

#include "esphome/components/modbus/modbus.h"
#include "esphome/core/helpers.h"

namespace esphome::modbus {

class TestModbus : public Modbus {
 public:
  void append_rx_bytes(const std::vector<uint8_t> &bytes) {
    this->rx_buffer_.insert(this->rx_buffer_.end(), bytes.begin(), bytes.end());
  }
  void run_drop_impossible() { this->drop_impossible_leading_bytes_(); }
  void run_extract() { this->try_extract_frame_(); }
  void set_waiting(uint8_t addr) { this->waiting_for_response_ = addr; }
  size_t rx_buffer_size() const { return this->rx_buffer_.size(); }
};

class MockDevice : public ModbusDevice {
 public:
  void on_modbus_data(const std::vector<uint8_t> &data) override { this->payloads.push_back(data); }

  std::vector<std::vector<uint8_t>> payloads;
};

static std::vector<uint8_t> make_rtu_frame(const std::vector<uint8_t> &frame_data) {
  std::vector<uint8_t> frame = frame_data;
  uint16_t crc = esphome::crc16(frame.data(), frame.size());
  frame.push_back(crc & 0xFF);
  frame.push_back((crc >> 8) & 0xFF);
  return frame;
}

TEST(ModbusTest, TruncatedHeaderRemainsBuffered) {
  TestModbus modbus;
  modbus.set_role(ModbusRole::CLIENT);

  modbus.append_rx_bytes({0x01, 0x03});
  modbus.run_extract();

  // Incomplete frame must stay buffered so subsequent bytes can complete it.
  EXPECT_EQ(modbus.rx_buffer_size(), 2);
}

TEST(ModbusTest, StandardFrameDispatchesFromBufferedParser) {
  TestModbus modbus;
  modbus.set_role(ModbusRole::CLIENT);

  MockDevice device;
  device.set_parent(&modbus);
  device.set_address(0x01);
  modbus.register_device(&device);
  modbus.set_waiting(0x01);

  modbus.append_rx_bytes(make_rtu_frame({0x01, 0x03, 0x02, 0x12, 0x34}));
  modbus.run_extract();

  ASSERT_EQ(device.payloads.size(), 1);
  EXPECT_EQ(device.payloads[0], (std::vector<uint8_t>{0x12, 0x34}));
  EXPECT_EQ(modbus.rx_buffer_size(), 0);
}

TEST(ModbusTest, UserDefinedFourByteFrameDispatches) {
  TestModbus modbus;
  modbus.set_role(ModbusRole::CLIENT);

  MockDevice device;
  device.set_parent(&modbus);
  device.set_address(0x01);
  modbus.register_device(&device);
  modbus.set_waiting(0x01);

  // User-defined function code 0x44 with no payload (4-byte frame: addr+FC+CRC16).
  modbus.append_rx_bytes(make_rtu_frame({0x01, 0x44}));
  modbus.run_extract();

  ASSERT_EQ(device.payloads.size(), 1);
  EXPECT_EQ(device.payloads[0], (std::vector<uint8_t>{0x44}));
  EXPECT_EQ(modbus.rx_buffer_size(), 0);
}

TEST(ModbusTest, SlidingResyncRecoversValidFrameAfterNoise) {
  TestModbus modbus;
  modbus.set_role(ModbusRole::CLIENT);

  MockDevice device;
  device.set_parent(&modbus);
  device.set_address(0x01);
  modbus.register_device(&device);
  modbus.set_waiting(0x01);

  std::vector<uint8_t> frame = make_rtu_frame({0x01, 0x03, 0x02, 0x12, 0x34});
  // This test exercises the sliding CRC resync in isolation. In production flow
  // receive_and_parse_modbus_bytes_() calls drop_impossible_leading_bytes_() first, which
  // would discard 0xAA/0xBB while waiting for address 0x01. Here we skip that pass and
  // verify that try_extract_frame_() alone still recovers the valid frame.
  std::vector<uint8_t> noisy_frame = {0xAA, 0xBB};
  noisy_frame.insert(noisy_frame.end(), frame.begin(), frame.end());

  modbus.append_rx_bytes(noisy_frame);
  modbus.run_extract();

  ASSERT_EQ(device.payloads.size(), 1);
  EXPECT_EQ(device.payloads[0], (std::vector<uint8_t>{0x12, 0x34}));
  EXPECT_EQ(modbus.rx_buffer_size(), 0);
}

TEST(ModbusTest, FastDropRemovesAddressMismatchedLeadingByteWhenWaiting) {
  TestModbus modbus;
  modbus.set_role(ModbusRole::CLIENT);

  MockDevice device;
  device.set_parent(&modbus);
  device.set_address(0x01);
  modbus.register_device(&device);
  modbus.set_waiting(0x01);

  std::vector<uint8_t> frame = make_rtu_frame({0x01, 0x03, 0x02, 0x12, 0x34});
  std::vector<uint8_t> with_noise = {0x00};
  with_noise.insert(with_noise.end(), frame.begin(), frame.end());

  modbus.append_rx_bytes(with_noise);
  // The fast-drop path must remove the leading 0x00 before any CRC work happens;
  // after drop_impossible the buffer should start with the valid frame.
  modbus.run_drop_impossible();
  EXPECT_EQ(modbus.rx_buffer_size(), frame.size());

  modbus.run_extract();
  ASSERT_EQ(device.payloads.size(), 1);
  EXPECT_EQ(device.payloads[0], (std::vector<uint8_t>{0x12, 0x34}));
  EXPECT_EQ(modbus.rx_buffer_size(), 0);
}

TEST(ModbusTest, FastDropIgnoresNonNulLeadingByteWhenNotWaiting) {
  TestModbus modbus;
  modbus.set_role(ModbusRole::CLIENT);

  MockDevice device;
  device.set_parent(&modbus);
  device.set_address(0x01);
  modbus.register_device(&device);

  std::vector<uint8_t> frame = make_rtu_frame({0x01, 0x03, 0x02, 0x12, 0x34});
  std::vector<uint8_t> with_noise = {0xAA};
  with_noise.insert(with_noise.end(), frame.begin(), frame.end());

  modbus.append_rx_bytes(with_noise);
  // Drop-pass invariant under test: when no response is pending, the fast-drop path must
  // not consume arbitrary non-NUL bytes (unlike the waiting path, which drops any leading
  // non-address byte). Only NUL noise is trimmed here; 0xAA must remain.
  modbus.run_drop_impossible();
  EXPECT_EQ(modbus.rx_buffer_size(), with_noise.size());

  // Recovery still works - try_extract_frame_ uses the sliding CRC resync to drop the
  // 0xAA and extract the valid frame. Not checking device.payloads here because CLIENT
  // mode requires set_waiting() for dispatch, which is a separate concern from this test.
  modbus.run_extract();
  EXPECT_EQ(modbus.rx_buffer_size(), 0);
}

}  // namespace esphome::modbus
