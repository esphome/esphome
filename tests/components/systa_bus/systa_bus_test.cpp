#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <vector>

#include "esphome/components/systa_bus/systa_bus.h"
#include "esphome/components/uart/uart_component.h"

namespace esphome::systa_bus::testing {

namespace {

class MockUARTComponent : public uart::UARTComponent {
 public:
  std::vector<uint8_t> rx;

  void push_rx(const std::vector<uint8_t> &data) { this->rx.insert(this->rx.end(), data.begin(), data.end()); }

  void write_array(const uint8_t *data, size_t len) override {}
  bool read_array(uint8_t *data, size_t len) override {
    if (this->rx.size() < len)
      return false;
    std::copy(this->rx.begin(), this->rx.begin() + len, data);
    this->rx.erase(this->rx.begin(), this->rx.begin() + len);
    return true;
  }
  size_t available() override { return this->rx.size(); }

  MOCK_METHOD(bool, peek_byte, (uint8_t * data), (override));
  MOCK_METHOD(uart::UARTFlushResult, flush, (), (override));
  MOCK_METHOD(void, check_logger_conflict, (), (override));
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif
};

class TestListener final : public SystaBusListener {
 public:
  void handle_message(std::span<const uint8_t> message) override {
    this->messages.emplace_back(message.begin(), message.end());
  }
  std::vector<std::vector<uint8_t>> messages;
};

void put_i16be(std::vector<uint8_t> &frame, size_t offset, int16_t value) {
  frame[offset] = static_cast<uint8_t>(static_cast<uint16_t>(value) >> 8);
  frame[offset + 1] = static_cast<uint8_t>(value);
}

// A 25-byte Aqua sensor frame with a valid checksum
std::vector<uint8_t> aqua_frame(int16_t tsa, int16_t tse, int16_t twu, int16_t tw2, uint8_t pump) {
  std::vector<uint8_t> frame(MAX_MESSAGE_SIZE, 0);
  frame[0] = START_BYTE;
  frame[1] = MESSAGE_TYPE_AQUA_SENSOR_DATA & 0xff;
  put_i16be(frame, 4, tsa);
  put_i16be(frame, 6, tse);
  put_i16be(frame, 8, twu);
  put_i16be(frame, 10, tw2);
  frame[12] = pump;
  uint8_t sum = 0;
  for (size_t i = 0; i + 1 < frame.size(); i++)
    sum += frame[i];
  frame.back() = static_cast<uint8_t>(0 - sum);
  return frame;
}

class SystaBusTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->bus_.set_uart_parent(&this->uart_);
    this->bus_.register_listener(&this->listener_);
  }
  void feed_(const std::vector<uint8_t> &bytes) {
    this->uart_.push_rx(bytes);
    this->bus_.loop();
  }

  MockUARTComponent uart_;
  SystaBus bus_;
  TestListener listener_;
};

const std::vector<uint8_t> FRAME_A = aqua_frame(512, 301, -15, 700, 60);
const std::vector<uint8_t> FRAME_B = aqua_frame(513, 302, -14, 701, 61);

}  // namespace

TEST_F(SystaBusTest, DecodesAValidFrame) {
  this->feed_(FRAME_A);
  ASSERT_EQ(this->listener_.messages.size(), 1u);
  EXPECT_EQ(this->listener_.messages[0], FRAME_A);
}

TEST_F(SystaBusTest, IgnoresGarbageBeforeAFrame) {
  std::vector<uint8_t> bytes = {0x00, 0x16, 0x42, 0xfc};  // includes a stray start byte
  bytes.insert(bytes.end(), FRAME_A.begin(), FRAME_A.end());
  this->feed_(bytes);
  ASSERT_EQ(this->listener_.messages.size(), 1u);
  EXPECT_EQ(this->listener_.messages[0], FRAME_A);
}

TEST_F(SystaBusTest, SkipsAnUnknownMessageType) {
  std::vector<uint8_t> bytes = {START_BYTE, 0x1a, 0x01, 0x02, 0x03};
  bytes.insert(bytes.end(), FRAME_A.begin(), FRAME_A.end());
  this->feed_(bytes);
  ASSERT_EQ(this->listener_.messages.size(), 1u);
  EXPECT_EQ(this->listener_.messages[0], FRAME_A);
}

TEST_F(SystaBusTest, RejectsACorruptedByte) {
  auto bad = FRAME_A;
  bad[6] ^= 0x10;
  std::vector<uint8_t> bytes = bad;
  bytes.insert(bytes.end(), FRAME_B.begin(), FRAME_B.end());
  this->feed_(bytes);
  ASSERT_EQ(this->listener_.messages.size(), 1u);
  EXPECT_EQ(this->listener_.messages[0], FRAME_B);
}

// A frame that lost a byte swallows the start byte of the next frame; the parser must still find that frame.
TEST_F(SystaBusTest, RecoversTheFrameAfterADroppedByte) {
  auto truncated = FRAME_A;
  truncated.erase(truncated.begin() + 9);
  std::vector<uint8_t> bytes = truncated;
  bytes.insert(bytes.end(), FRAME_B.begin(), FRAME_B.end());
  this->feed_(bytes);
  ASSERT_EQ(this->listener_.messages.size(), 1u);
  EXPECT_EQ(this->listener_.messages[0], FRAME_B);
}

TEST_F(SystaBusTest, DecodesFramesSplitAcrossLoops) {
  std::vector<uint8_t> first(FRAME_A.begin(), FRAME_A.begin() + 10);
  std::vector<uint8_t> second(FRAME_A.begin() + 10, FRAME_A.end());
  this->feed_(first);
  EXPECT_TRUE(this->listener_.messages.empty());
  this->feed_(second);
  ASSERT_EQ(this->listener_.messages.size(), 1u);
  EXPECT_EQ(this->listener_.messages[0], FRAME_A);
}

TEST_F(SystaBusTest, DecodesBackToBackFrames) {
  std::vector<uint8_t> bytes = FRAME_A;
  bytes.insert(bytes.end(), FRAME_B.begin(), FRAME_B.end());
  this->feed_(bytes);
  ASSERT_EQ(this->listener_.messages.size(), 2u);
  EXPECT_EQ(this->listener_.messages[0], FRAME_A);
  EXPECT_EQ(this->listener_.messages[1], FRAME_B);
}

}  // namespace esphome::systa_bus::testing
