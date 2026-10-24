#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "esphome/components/vbus/vbus.h"

namespace esphome::vbus::testing {
namespace {

class LoopTestUART : public uart::UARTComponent {
 public:
  void inject(const std::vector<uint8_t> &bytes) { this->rx_.insert(this->rx_.end(), bytes.begin(), bytes.end()); }
  void refill_reads(size_t count) { this->refill_reads_ = count; }
  size_t read_count() const { return this->position_; }
  size_t available() override { return this->rx_.size() - this->position_; }
  bool read_array(uint8_t *data, size_t size) override {
    if (size > this->available())
      return false;
    std::copy_n(this->rx_.data() + this->position_, size, data);
    this->position_ += size;
    if (this->refill_reads_ > 0) {
      --this->refill_reads_;
      this->rx_.push_back(0x00);
    }
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
  size_t refill_reads_{0};
};

class LoopTestListener : public VBusListener {
 public:
  std::vector<std::vector<uint8_t>> messages;

 protected:
  void handle_message(std::vector<uint8_t> &message) override { this->messages.push_back(message); }
};

const std::vector<uint8_t> LOOP_TEST_PACKET = {0xaa, 0x10, 0x00, 0x11, 0x22, 0x10, 0x00, 0x01, 0x02, 0x29, 0x01,
                                               0x02, 0x03, 0x04, 0x00, 0x75, 0x10, 0x20, 0x30, 0x40, 0x0f, 0x50};
const std::vector<uint8_t> LOOP_TEST_PAYLOAD = {0x01, 0x02, 0x03, 0x04, 0x90, 0xa0, 0xb0, 0xc0};

class VBusLoopTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->bus_.set_uart_parent(&this->uart_);
    this->listener_.set_command(0x0100);
    this->listener_.set_source(0x2211);
    this->listener_.set_dest(0x0010);
    this->bus_.register_listener(&this->listener_);
  }

  LoopTestUART uart_;
  VBus bus_{};
  LoopTestListener listener_;
};

TEST_F(VBusLoopTest, YieldsWithNoiseStillQueuedAndThenRecovers) {
  this->uart_.inject(std::vector<uint8_t>(512, 0x00));
  this->uart_.inject(LOOP_TEST_PACKET);
  this->bus_.loop();
  ASSERT_EQ(this->uart_.read_count(), 64u);
  ASSERT_TRUE(this->listener_.messages.empty());
  for (size_t pass = 0; pass < 8; ++pass)
    this->bus_.loop();
  EXPECT_EQ(this->uart_.available(), 0u);
  ASSERT_EQ(this->listener_.messages.size(), 1u);
  EXPECT_EQ(this->listener_.messages[0], LOOP_TEST_PAYLOAD);
}

TEST_F(VBusLoopTest, DefersBytesArrivingAfterTheReadSnapshot) {
  this->uart_.inject({0x00});
  // Refill finitely so a regressed parser fails instead of hanging the test runner.
  this->uart_.refill_reads(256);
  this->bus_.loop();
  // The shared helper snapshots availability: bytes arriving during reads wait for the next loop.
  EXPECT_EQ(this->uart_.read_count(), 1u);
  EXPECT_EQ(this->uart_.available(), 1u);
  EXPECT_TRUE(this->listener_.messages.empty());
}

TEST_F(VBusLoopTest, ReassemblesTheMaximumFrameCountAcrossBoundedPasses) {
  this->uart_.inject({0xaa, 0x10, 0x00, 0x11, 0x22, 0x10, 0x00, 0x01, 0x7f, 0x2c});
  std::vector<uint8_t> expected;
  for (size_t frame = 0; frame < 127; ++frame) {
    this->uart_.inject({0x01, 0x02, 0x03, 0x04, 0x00, 0x75});
    expected.insert(expected.end(), {0x01, 0x02, 0x03, 0x04});
  }
  size_t passes = 0;
  while (this->uart_.available()) {
    ASSERT_LT(passes++, 13u);
    const size_t before = this->uart_.read_count();
    this->bus_.loop();
    ASSERT_GT(this->uart_.read_count(), before);
    ASSERT_LE(this->uart_.read_count() - before, 64u);
    if (this->uart_.available())
      EXPECT_TRUE(this->listener_.messages.empty());
  }
  ASSERT_EQ(this->listener_.messages.size(), 1u);
  EXPECT_EQ(this->listener_.messages[0], expected);
}

class VBusLoopBoundaryTest : public VBusLoopTest, public ::testing::WithParamInterface<size_t> {};

TEST_P(VBusLoopBoundaryTest, PreservesPacketsAcrossLoopBoundaries) {
  this->uart_.inject(std::vector<uint8_t>(GetParam(), 0x00));
  this->uart_.inject(LOOP_TEST_PACKET);
  this->uart_.inject(LOOP_TEST_PACKET);
  this->bus_.loop();
  this->bus_.loop();
  EXPECT_EQ(this->uart_.available(), 0u);
  ASSERT_EQ(this->listener_.messages.size(), 2u);
  EXPECT_EQ(this->listener_.messages[0], LOOP_TEST_PAYLOAD);
  EXPECT_EQ(this->listener_.messages[1], LOOP_TEST_PAYLOAD);
}

// Complete packets in one pass, or a loop boundary in the header, data, or checksum.
INSTANTIATE_TEST_SUITE_P(PacketPositions, VBusLoopBoundaryTest, ::testing::Values(0u, 59u, 51u, 43u));

}  // namespace
}  // namespace esphome::vbus::testing
