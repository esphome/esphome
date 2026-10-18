#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <vector>
#include <cstdint>
#include "esphome/components/uart/packet_interface/uart_packet_interface.h"
#include "esphome/components/packet_interface/packet_buffer.h"
#include "esphome/components/uart/uart_component.h"
#include "common.h"

namespace esphome::uart::testing {

// Copies a packet into a vector, for comparing in tests
static std::vector<uint8_t> to_vector(const esphome::packet_interface::PacketBuffer &data) {
  std::vector<uint8_t> result(data.size());
  data.copy_to(result.data(), result.size());
  return result;
}

using packet_interface::PacketBuffer;
using packet_interface::PacketMetaData;

// Test fixture
class UartPacketInterfaceTest : public ::testing::Test {
 protected:
  MockUARTComponent uart_{};
  // As large as the default max_packet_size; tests only make it smaller
  uint8_t rx_buffer_[512]{};
  UartPacketInterface interface_{this->rx_buffer_};
  std::vector<std::vector<uint8_t>> received_packets_;

  void SetUp() override {
    received_packets_.clear();
    interface_.set_uart_parent(&uart_);
    interface_.setup();

    interface_.add_packet_interface_listener(
        [this](const PacketBuffer &data, const PacketMetaData &meta) { received_packets_.push_back(to_vector(data)); });
  }
};

// Feed bytes to the interface through the mocked UART and run its loop
static void feed(MockUARTComponent &uart, UartPacketInterface &interface, const std::vector<uint8_t> &bytes) {
  size_t read_pos = 0;
  EXPECT_CALL(uart, available()).WillRepeatedly(Invoke([&bytes, &read_pos]() { return bytes.size() - read_pos; }));
  EXPECT_CALL(uart, read_array(_, 1)).WillRepeatedly(Invoke([&bytes, &read_pos](uint8_t *data, size_t) {
    if (read_pos >= bytes.size())
      return false;
    *data = bytes[read_pos++];
    return true;
  }));
  interface.loop();
}

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
  interface_.set_max_packet_size(10);
  interface_.setup();

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

// Test a packet larger than max_packet_size is not sent
TEST_F(UartPacketInterfaceTest, ReceiveLimitedToBuffer) {
  uint8_t small_buffer[4]{};
  UartPacketInterface interface(small_buffer);
  interface.set_uart_parent(&this->uart_);
  // Raised above the buffer after code generation sized it
  interface.set_max_packet_size(100);
  std::vector<std::vector<uint8_t>> received;
  interface.add_packet_interface_listener(
      [&received](const PacketBuffer &data, const PacketMetaData &) { received.push_back(to_vector(data)); });
  feed(this->uart_, interface, {0x7E, 1, 2, 3, 4, 5, 6, 0x7E, 7, 8, 0x7E});
  EXPECT_EQ(received, (std::vector<std::vector<uint8_t>>{{7, 8}}));
}

TEST_F(UartPacketInterfaceTest, OversizedPacketNotSent) {
  interface_.set_max_packet_size(4);
  std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0x05};
  PacketBuffer buffer(data);

  EXPECT_FALSE(interface_.send_to_interface(buffer, {}));
  EXPECT_EQ(uart_.written_data.size(), 0);
}

// Test a composite buffer longer than one write chunk is framed correctly
TEST_F(UartPacketInterfaceTest, SendCompositeAcrossChunks) {
  std::vector<uint8_t> pre(40, 0x7E);
  std::vector<uint8_t> mid_data = {0x11, 0x22};
  std::vector<uint8_t> post(30, 0x33);
  PacketBuffer mid(mid_data);
  PacketBuffer buffer(pre.data(), pre.size(), &mid, post.data(), post.size());

  ASSERT_TRUE(interface_.send_to_interface(buffer, {}));

  std::vector<uint8_t> expected = {0x7E};
  for (size_t i = 0; i != pre.size(); i++) {
    expected.push_back(0x7D);
    expected.push_back(0x5E);
  }
  expected.insert(expected.end(), mid_data.begin(), mid_data.end());
  expected.insert(expected.end(), post.begin(), post.end());
  expected.push_back(0x7E);
  EXPECT_EQ(uart_.written_data, expected);
}

// Test one flag between frames both ends the first and starts the second
TEST_F(UartPacketInterfaceTest, SharedFlagBetweenFrames) {
  feed(uart_, interface_, {0x7E, 0x01, 0x7E, 0x02, 0x03, 0x7E});

  EXPECT_EQ(received_packets_, (std::vector<std::vector<uint8_t>>{{0x01}, {0x02, 0x03}}));
}

// Test an unfinished frame does not hide the frame after it
TEST_F(UartPacketInterfaceTest, UnfinishedFrameDoesNotHideNext) {
  feed(uart_, interface_, {0x7E, 0xAA, 0x7E, 0x01, 0x02, 0x7E});

  EXPECT_EQ(received_packets_, (std::vector<std::vector<uint8_t>>{{0xAA}, {0x01, 0x02}}));
}

}  // namespace esphome::uart::testing
