#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "esphome/components/vbus/vbus.h"
#include "esphome/core/application.h"

namespace esphome::vbus::testing {
namespace {

class IncompleteTestUART : public uart::UARTComponent {
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
  void write_array(const uint8_t *data, size_t size) override { ADD_FAILURE() << "RX must not transmit"; }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS; }
  void check_logger_conflict() override {}
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif

 protected:
  std::vector<uint8_t> rx_;
  size_t position_{0};
};

class IncompleteTestListener : public VBusListener {
 public:
  std::vector<std::vector<uint8_t>> messages;

 protected:
  void handle_message(std::vector<uint8_t> &message) override { this->messages.push_back(message); }
};

const std::vector<uint8_t> INCOMPLETE_TEST_PACKET = {0xaa, 0x10, 0x00, 0x11, 0x22, 0x10, 0x00, 0x01, 0x02, 0x29, 0x01,
                                                     0x02, 0x03, 0x04, 0x00, 0x75, 0x10, 0x20, 0x30, 0x40, 0x0f, 0x50};
const std::vector<uint8_t> INCOMPLETE_TEST_PAYLOAD = {0x01, 0x02, 0x03, 0x04, 0x90, 0xa0, 0xb0, 0xc0};

class VBusIncompleteTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->bus_.set_uart_parent(&this->uart_);
    this->listener_.set_command(0x0100);
    this->listener_.set_source(0x2211);
    this->listener_.set_dest(0x0010);
    this->bus_.register_listener(&this->listener_);
  }
  void loop_at_(uint32_t now) {
    LoopBlockingGuard{nullptr, nullptr, now};
    this->bus_.loop();
  }
  void feed_at_(const std::vector<uint8_t> &bytes, uint32_t now) {
    this->uart_.inject(bytes);
    this->loop_at_(now);
  }

  IncompleteTestUART uart_;
  VBus bus_;
  IncompleteTestListener listener_;
};

class VBusTruncationTest : public VBusIncompleteTest, public ::testing::WithParamInterface<size_t> {};

TEST_P(VBusTruncationTest, CompletesBeforeTheIdleTimeout) {
  const auto split = INCOMPLETE_TEST_PACKET.begin() + GetParam();
  this->feed_at_({INCOMPLETE_TEST_PACKET.begin(), split}, 1000);
  ASSERT_TRUE(this->listener_.messages.empty());
  this->loop_at_(2499);
  this->feed_at_({split, INCOMPLETE_TEST_PACKET.end()}, 2499);
  ASSERT_EQ(this->listener_.messages.size(), 1u);
  EXPECT_EQ(this->listener_.messages[0], INCOMPLETE_TEST_PAYLOAD);
}

TEST_P(VBusTruncationTest, DiscardsThePartialMessageAtTheIdleTimeout) {
  const auto split = INCOMPLETE_TEST_PACKET.begin() + GetParam();
  this->feed_at_({INCOMPLETE_TEST_PACKET.begin(), split}, 1000);
  ASSERT_TRUE(this->listener_.messages.empty());
  this->loop_at_(2500);
  this->feed_at_({split, INCOMPLETE_TEST_PACKET.end()}, 2500);
  ASSERT_TRUE(this->listener_.messages.empty());
  this->feed_at_(INCOMPLETE_TEST_PACKET, 2501);
  ASSERT_EQ(this->listener_.messages.size(), 1u);
  EXPECT_EQ(this->listener_.messages[0], INCOMPLETE_TEST_PAYLOAD);
}

// Every truncation point, including sync alone, partial headers, and partial frames.
INSTANTIATE_TEST_SUITE_P(EveryByte, VBusTruncationTest, ::testing::Range(size_t{1}, INCOMPLETE_TEST_PACKET.size()));

TEST_F(VBusIncompleteTest, MeasuresInactivityFromTheLatestChunk) {
  this->feed_at_({INCOMPLETE_TEST_PACKET.begin(), INCOMPLETE_TEST_PACKET.begin() + 5}, 1000);
  this->feed_at_({INCOMPLETE_TEST_PACKET.begin() + 5, INCOMPLETE_TEST_PACKET.begin() + 13}, 2000);
  this->loop_at_(3499);
  this->feed_at_({INCOMPLETE_TEST_PACKET.begin() + 13, INCOMPLETE_TEST_PACKET.end()}, 3499);
  ASSERT_EQ(this->listener_.messages.size(), 1u);
  EXPECT_EQ(this->listener_.messages[0], INCOMPLETE_TEST_PAYLOAD);
}

TEST_F(VBusIncompleteTest, PreservesQueuedBytesAfterALongLoopDelay) {
  this->feed_at_({INCOMPLETE_TEST_PACKET.begin(), INCOMPLETE_TEST_PACKET.begin() + 13}, 1000);
  this->uart_.inject({INCOMPLETE_TEST_PACKET.begin() + 13, INCOMPLETE_TEST_PACKET.end()});
  this->loop_at_(10000);
  ASSERT_EQ(this->listener_.messages.size(), 1u);
  EXPECT_EQ(this->listener_.messages[0], INCOMPLETE_TEST_PAYLOAD);
}

TEST_F(VBusIncompleteTest, PreservesAPartialMessageBeforeTheTimeoutAcrossClockRollover) {
  const uint32_t started = UINT32_MAX - 500;
  this->feed_at_({INCOMPLETE_TEST_PACKET.begin(), INCOMPLETE_TEST_PACKET.begin() + 13}, started);
  this->loop_at_(started + 1499);
  this->feed_at_({INCOMPLETE_TEST_PACKET.begin() + 13, INCOMPLETE_TEST_PACKET.end()}, started + 1499);
  ASSERT_EQ(this->listener_.messages.size(), 1u);
  EXPECT_EQ(this->listener_.messages[0], INCOMPLETE_TEST_PAYLOAD);
}

TEST_F(VBusIncompleteTest, ExpiresAPartialMessageAcrossClockRollover) {
  const uint32_t started = UINT32_MAX - 500;
  this->feed_at_({INCOMPLETE_TEST_PACKET.begin(), INCOMPLETE_TEST_PACKET.begin() + 13}, started);
  this->loop_at_(started + 1500);
  this->feed_at_({INCOMPLETE_TEST_PACKET.begin() + 13, INCOMPLETE_TEST_PACKET.end()}, started + 1500);
  ASSERT_TRUE(this->listener_.messages.empty());
  this->feed_at_(INCOMPLETE_TEST_PACKET, started + 1501);
  ASSERT_EQ(this->listener_.messages.size(), 1u);
  EXPECT_EQ(this->listener_.messages[0], INCOMPLETE_TEST_PAYLOAD);
}

}  // namespace
}  // namespace esphome::vbus::testing
