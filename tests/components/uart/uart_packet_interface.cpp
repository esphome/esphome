#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <vector>
#include <cstdint>
#include "esphome/components/uart/packet_interface/uart_packet_interface.h"
#include "esphome/components/packet_interface/packet_buffer.h"
#include "esphome/components/uart/uart_component.h"
#include "common.h"

namespace esphome {
namespace uart {
namespace testing {

// Test fixture
class UartPacketInterfaceTest : public ::testing::Test {
 protected:
  MockUARTComponent uart_{};
  UartPacketInterface interface_{};
  std::vector<std::vector<uint8_t>> received_packets_;

  void SetUp() override {
    received_packets_.clear();
    interface_.set_uart_parent(&uart_);

    interface_.add_packet_interface_listener(
        [this](const PacketBuffer &data, PacketMetaData meta) { received_packets_.push_back(data.to_vector()); });
  }
};

// Test basic frame sending
TEST_F(UartPacketInterfaceTest, SendSimpleFrame) {
  std::vector<uint8_t> data = {0x01, 0x02, 0x03};
  PacketBuffer buffer(data);

  // Set up the mock to accumulate bytes
  ASSERT_EQ(uart_.written_data.size(), 0);
  interface_.send_to_interface(buffer, {});
  // Verify the accumulated bytes
  ASSERT_EQ(uart_.written_data.size(), 5);

  std::vector<uint8_t> expected = {0x7E, 0x01, 0x02, 0x03, 0x7E};
  EXPECT_EQ(uart_.written_data, expected);
}
// Test byte stuffing for FLAG_BYTE
TEST_F(UartPacketInterfaceTest, SendWithFlagByteStuffing) {
  std::vector<uint8_t> data = {0x01, 0x7E, 0x03};
  PacketBuffer buffer(data);

  uart_.clear();
  interface_.send_to_interface(buffer, {});

  // Should escape 0x7E as 0x7D 0x5E
  std::vector<uint8_t> expected = {0x7E, 0x01, 0x7D, 0x5E, 0x03, 0x7E};
  EXPECT_EQ(uart_.written_data, expected);
}

// Test byte stuffing for CONTROL_BYTE
TEST_F(UartPacketInterfaceTest, SendWithControlByteStuffing) {
  std::vector<uint8_t> data = {0x01, 0x7D, 0x03};
  PacketBuffer buffer(data);

  uart_.clear();
  interface_.send_to_interface(buffer, {});

  // Should escape 0x7D as 0x7D 0x5D
  std::vector<uint8_t> expected = {0x7E, 0x01, 0x7D, 0x5D, 0x03, 0x7E};
  EXPECT_EQ(uart_.written_data, expected);
}

// Test sending empty frame
TEST_F(UartPacketInterfaceTest, SendEmptyFrame) {
  std::vector<uint8_t> data = {};
  PacketBuffer buffer(data);

  interface_.send_to_interface(buffer, {});

  // Empty data should return without sending
  EXPECT_EQ(uart_.written_data.size(), 0);
}

// Test receiving simple frame
TEST_F(UartPacketInterfaceTest, ReceiveSimpleFrame) {
  std::vector<uint8_t> frame_data = {0x7E, 0x01, 0x02, 0x03, 0x7E};
  size_t read_pos = 0;

  EXPECT_CALL(uart_, available()).WillRepeatedly(Invoke([&frame_data, &read_pos]() {
    return frame_data.size() - read_pos;
  }));

  EXPECT_CALL(uart_, read_array(_, 1)).WillRepeatedly(Invoke([&frame_data, &read_pos](uint8_t *data, size_t) {
    if (read_pos >= frame_data.size())
      return false;
    *data = frame_data[read_pos++];
    return true;
  }));

  interface_.loop();

  ASSERT_EQ(received_packets_.size(), 1);
  std::vector<uint8_t> expected = {0x01, 0x02, 0x03};
  EXPECT_EQ(received_packets_[0], expected);
}

// Test receiving frame with FLAG_BYTE escape
TEST_F(UartPacketInterfaceTest, ReceiveFrameWithFlagEscape) {
  std::vector<uint8_t> frame_data = {0x7E, 0x01, 0x7D, 0x5E, 0x03, 0x7E};
  size_t read_pos = 0;

  EXPECT_CALL(uart_, available()).WillRepeatedly(Invoke([&frame_data, &read_pos]() {
    return frame_data.size() - read_pos;
  }));

  EXPECT_CALL(uart_, read_array(_, 1)).WillRepeatedly(Invoke([&frame_data, &read_pos](uint8_t *data, size_t) {
    if (read_pos >= frame_data.size())
      return false;
    *data = frame_data[read_pos++];
    return true;
  }));

  interface_.loop();

  ASSERT_EQ(received_packets_.size(), 1);
  std::vector<uint8_t> expected = {0x01, 0x7E, 0x03};  // 0x7E unescaped
  EXPECT_EQ(received_packets_[0], expected);
}

// Test receiving frame with CONTROL_BYTE escape
TEST_F(UartPacketInterfaceTest, ReceiveFrameWithControlEscape) {
  std::vector<uint8_t> frame_data = {0x7E, 0x01, 0x7D, 0x5D, 0x03, 0x7E};
  size_t read_pos = 0;

  EXPECT_CALL(uart_, available()).WillRepeatedly(Invoke([&frame_data, &read_pos]() {
    return frame_data.size() - read_pos;
  }));

  EXPECT_CALL(uart_, read_array(_, 1)).WillRepeatedly(Invoke([&frame_data, &read_pos](uint8_t *data, size_t) {
    if (read_pos >= frame_data.size())
      return false;
    *data = frame_data[read_pos++];
    return true;
  }));

  interface_.loop();

  ASSERT_EQ(received_packets_.size(), 1);
  std::vector<uint8_t> expected = {0x01, 0x7D, 0x03};  // 0x7D unescaped
  EXPECT_EQ(received_packets_[0], expected);
}

// Test receiving multiple frames
TEST_F(UartPacketInterfaceTest, ReceiveMultipleFrames) {
  std::vector<uint8_t> frame_data = {0x7E, 0x01, 0x02, 0x7E, 0x7E, 0x03, 0x04, 0x7E};
  size_t read_pos = 0;

  EXPECT_CALL(uart_, available()).WillRepeatedly(Invoke([&frame_data, &read_pos]() {
    return frame_data.size() - read_pos;
  }));

  EXPECT_CALL(uart_, read_array(_, 1)).WillRepeatedly(Invoke([&frame_data, &read_pos](uint8_t *data, size_t) {
    if (read_pos >= frame_data.size())
      return false;
    *data = frame_data[read_pos++];
    return true;
  }));

  interface_.loop();

  ASSERT_EQ(received_packets_.size(), 2) << "Expected 2 packets, got " << received_packets_.size();
  std::vector<uint8_t> expected1 = {0x01, 0x02};
  std::vector<uint8_t> expected2 = {0x03, 0x04};
  auto pkt0 = received_packets_[0];
  auto pkt1 = received_packets_[1];
  EXPECT_EQ(pkt0, expected1) << "Packet 0 size: " << pkt0.size();
  EXPECT_EQ(pkt1, expected2) << "Packet 1 size: " << pkt1.size();
}

// Test receiving empty frame (back-to-back flags)
TEST_F(UartPacketInterfaceTest, ReceiveEmptyFrame) {
  std::vector<uint8_t> frame_data = {0x7E, 0x7E};
  size_t read_pos = 0;

  EXPECT_CALL(uart_, available()).WillRepeatedly(Invoke([&frame_data, &read_pos]() {
    return frame_data.size() - read_pos;
  }));

  EXPECT_CALL(uart_, read_array(_, 1)).WillRepeatedly(Invoke([&frame_data, &read_pos](uint8_t *data, size_t) {
    if (read_pos >= frame_data.size())
      return false;
    *data = frame_data[read_pos++];
    return true;
  }));

  interface_.loop();

  // Empty frames should not be delivered
  EXPECT_EQ(received_packets_.size(), 0);
}

// Test data before first flag is ignored
TEST_F(UartPacketInterfaceTest, IgnoreDataBeforeFirstFlag) {
  std::vector<uint8_t> frame_data = {0xFF, 0xFF, 0x7E, 0x01, 0x02, 0x7E};
  size_t read_pos = 0;

  EXPECT_CALL(uart_, available()).WillRepeatedly(Invoke([&frame_data, &read_pos]() {
    return frame_data.size() - read_pos;
  }));

  EXPECT_CALL(uart_, read_array(_, 1)).WillRepeatedly(Invoke([&frame_data, &read_pos](uint8_t *data, size_t) {
    if (read_pos >= frame_data.size())
      return false;
    *data = frame_data[read_pos++];
    return true;
  }));

  interface_.loop();

  ASSERT_EQ(received_packets_.size(), 1);
  std::vector<uint8_t> expected = {0x01, 0x02};
  EXPECT_EQ(received_packets_[0], expected);
}

// Test oversized packet is discarded
TEST_F(UartPacketInterfaceTest, OversizedPacketDiscarded) {
  interface_.set_rx_buffer_size(10);

  // Create a packet larger than buffer
  std::vector<uint8_t> frame_data = {0x7E};
  for (int i = 0; i < 15; i++) {
    frame_data.push_back(0xAA);
  }
  frame_data.push_back(0x7E);
  size_t read_pos = 0;

  EXPECT_CALL(uart_, available()).WillRepeatedly(Invoke([&frame_data, &read_pos]() {
    return frame_data.size() - read_pos;
  }));

  EXPECT_CALL(uart_, read_array(_, 1)).WillRepeatedly(Invoke([&frame_data, &read_pos](uint8_t *data, size_t) {
    if (read_pos >= frame_data.size())
      return false;
    *data = frame_data[read_pos++];
    return true;
  }));

  interface_.loop();

  // Oversized packet should be discarded
  EXPECT_EQ(received_packets_.size(), 0);
}

// Test round-trip (send and receive)
TEST_F(UartPacketInterfaceTest, RoundTrip) {
  std::vector<uint8_t> original = {0x01, 0x7E, 0x7D, 0x03};
  PacketBuffer send_buffer(original);

  // Send
  interface_.send_to_interface(send_buffer, {});

  // Prepare to receive - use the written data as frame data
  std::vector<uint8_t> frame_data = uart_.written_data;
  size_t read_pos = 0;

  EXPECT_CALL(uart_, available()).WillRepeatedly(Invoke([&frame_data, &read_pos]() {
    return frame_data.size() - read_pos;
  }));

  EXPECT_CALL(uart_, read_array(_, 1)).WillRepeatedly(Invoke([&frame_data, &read_pos](uint8_t *data, size_t) {
    if (read_pos >= frame_data.size())
      return false;
    *data = frame_data[read_pos++];
    return true;
  }));

  // Receive
  interface_.loop();

  ASSERT_EQ(received_packets_.size(), 1);
  EXPECT_EQ(received_packets_[0], original);
}

// Test all bytes can be transmitted (0x00 to 0xFF)
TEST_F(UartPacketInterfaceTest, AllByteValues) {
  std::vector<uint8_t> data;
  data.reserve(256);
  for (int i = 0; i != 256; i++) {
    data.push_back(static_cast<uint8_t>(i));
  }
  PacketBuffer send_buffer(data);

  // Send
  interface_.send_to_interface(send_buffer, {});

  // Prepare to receive - use the written data as frame data
  std::vector<uint8_t> frame_data = uart_.written_data;
  size_t read_pos = 0;

  EXPECT_CALL(uart_, available()).WillRepeatedly(Invoke([&frame_data, &read_pos]() {
    return frame_data.size() - read_pos;
  }));

  EXPECT_CALL(uart_, read_array(_, 1)).WillRepeatedly(Invoke([&frame_data, &read_pos](uint8_t *data, size_t) {
    if (read_pos >= frame_data.size())
      return false;
    *data = frame_data[read_pos++];
    return true;
  }));

  // Receive
  interface_.loop();

  ASSERT_EQ(received_packets_.size(), 1);
  EXPECT_EQ(received_packets_[0], data);
}

}  // namespace testing
}  // namespace uart
}  // namespace esphome
