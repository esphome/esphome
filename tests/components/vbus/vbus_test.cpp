#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <vector>

#include "esphome/components/vbus/vbus.h"

namespace esphome::vbus::testing {
namespace {

class MockUART : public uart::UARTComponent {
 public:
  void inject(const std::vector<uint8_t> &bytes) { this->rx_.insert(this->rx_.end(), bytes.begin(), bytes.end()); }
  size_t available() override { return this->rx_.size() - this->position_; }
  bool read_array(uint8_t *data, size_t size) override {
    if (size > this->available())
      return false;
    std::copy_n(this->rx_.data() + this->position_, size, data);
    this->position_ += size;
    return true;
  }
  bool peek_byte(uint8_t *data) override {
    if (!this->available())
      return false;
    *data = this->rx_[this->position_];
    return true;
  }
  MOCK_METHOD(void, write_array, (const uint8_t *data, size_t size), (override));
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS; }
  void check_logger_conflict() override {}
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif

 protected:
  std::vector<uint8_t> rx_;
  size_t position_{0};
};

class RecordingListener : public VBusListener {
 public:
  std::vector<std::vector<uint8_t>> messages;

 protected:
  void handle_message(std::vector<uint8_t> &message) override { this->messages.push_back(message); }
};

// P1 command 0x0100, source 0x2211, destination 0x0010, two frames.
const std::vector<uint8_t> HEADER = {0xaa, 0x10, 0x00, 0x11, 0x22, 0x10, 0x00, 0x01, 0x02, 0x29};
const std::array<std::vector<uint8_t>, 3> FRAMES = {{
    {0x01, 0x02, 0x03, 0x04, 0x00, 0x75},
    {0x10, 0x20, 0x30, 0x40, 0x0f, 0x50},
    {0x05, 0x06, 0x07, 0x08, 0x00, 0x65},
}};
const std::vector<uint8_t> PAYLOAD = {0x01, 0x02, 0x03, 0x04, 0x90, 0xa0, 0xb0, 0xc0};

class VBusTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->bus_.set_uart_parent(&this->uart_);
    this->listener_.set_command(0x0100);
    this->listener_.set_source(0x2211);
    this->listener_.set_dest(0x0010);
    this->bus_.register_listener(&this->listener_);
    EXPECT_CALL(this->uart_, write_array(::testing::_, ::testing::_)).Times(0);
  }
  void feed_(const std::vector<uint8_t> &bytes) {
    this->uart_.inject(bytes);
    this->bus_.loop();
  }
  void valid_packet_() {
    this->feed_(HEADER);
    this->feed_(FRAMES[0]);
    this->feed_(FRAMES[1]);
  }

  MockUART uart_;
  VBus bus_{};
  RecordingListener listener_;
};

TEST_F(VBusTest, DecodesValidPacketAndSeptets) {
  this->valid_packet_();
  ASSERT_EQ(this->listener_.messages.size(), 1u);
  EXPECT_EQ(this->listener_.messages[0], PAYLOAD);
}

TEST_F(VBusTest, PreservesPartialHeaderAndFrameAcrossLoopCalls) {
  this->feed_({HEADER.begin(), HEADER.begin() + 5});
  this->feed_({HEADER.begin() + 5, HEADER.end()});
  this->feed_({FRAMES[0].begin(), FRAMES[0].begin() + 3});
  this->feed_({FRAMES[0].begin() + 3, FRAMES[0].end()});
  EXPECT_TRUE(this->listener_.messages.empty());
  this->feed_(FRAMES[1]);
  ASSERT_EQ(this->listener_.messages.size(), 1u);
  EXPECT_EQ(this->listener_.messages[0], PAYLOAD);
}

TEST_F(VBusTest, RejectsBadHeaderChecksumAndRecoversAtNextSync) {
  auto bad_header = HEADER;
  bad_header.back() ^= 0x01;
  this->feed_(bad_header);
  this->feed_(FRAMES[0]);
  this->feed_(FRAMES[1]);
  EXPECT_TRUE(this->listener_.messages.empty());
  this->valid_packet_();
  ASSERT_EQ(this->listener_.messages.size(), 1u);
  EXPECT_EQ(this->listener_.messages[0], PAYLOAD);
}

class VBusBadFrameTest : public VBusTest, public ::testing::WithParamInterface<size_t> {};

TEST_P(VBusBadFrameTest, DiscardsWholePacketUntilNextSync) {
  auto header = HEADER;
  header[8] = 0x03;
  header[9] = 0x28;
  this->feed_(header);
  for (size_t i = 0; i < FRAMES.size(); ++i) {
    auto frame = FRAMES[i];
    if (i == GetParam())
      frame.back() ^= 0x01;
    this->feed_(frame);
  }
  // Checksum-valid trailing data must not replace the rejected frame.
  this->feed_(FRAMES[0]);
  EXPECT_TRUE(this->listener_.messages.empty());

  this->valid_packet_();
  ASSERT_EQ(this->listener_.messages.size(), 1u);
  EXPECT_EQ(this->listener_.messages[0], PAYLOAD);
}

INSTANTIATE_TEST_SUITE_P(FirstMiddleLast, VBusBadFrameTest, ::testing::Values(0u, 1u, 2u));

TEST_F(VBusTest, RecoversWithinTheSameUartBatch) {
  auto bad_frame = FRAMES[0];
  bad_frame.back() ^= 0x01;
  for (const auto &bytes : {HEADER, bad_frame, FRAMES[0], FRAMES[1], HEADER, FRAMES[0], FRAMES[1]})
    this->uart_.inject(bytes);
  this->bus_.loop();
  ASSERT_EQ(this->listener_.messages.size(), 1u);
  EXPECT_EQ(this->listener_.messages[0], PAYLOAD);
}

TEST_F(VBusTest, AcceptsMaximumFrameCount) {
  this->feed_({0xaa, 0x10, 0x00, 0x11, 0x22, 0x10, 0x00, 0x01, 0x7f, 0x2c});
  std::vector<uint8_t> expected;
  for (size_t i = 0; i < 127; ++i) {
    this->feed_(FRAMES[0]);
    expected.insert(expected.end(), {0x01, 0x02, 0x03, 0x04});
  }
  ASSERT_EQ(this->listener_.messages.size(), 1u);
  EXPECT_EQ(this->listener_.messages[0], expected);
}

}  // namespace
}  // namespace esphome::vbus::testing
