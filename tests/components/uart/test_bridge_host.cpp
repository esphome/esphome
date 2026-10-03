#include "esphome/components/uart/bridge/uart_bridge.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace esphome::uart::testing {

class BridgeFakeUart : public UARTComponent {
 public:
  void write_array(const uint8_t *data, size_t len) override {
    for (size_t i = 0; i < len && this->tx_len_ < sizeof(this->tx_); i++) {
      this->tx_[this->tx_len_++] = data[i];
    }
  }
  bool peek_byte(uint8_t *data) override {
    if (this->rx_pos_ >= this->rx_len_) {
      return false;
    }
    *data = this->rx_[this->rx_pos_];
    return true;
  }
  bool read_array(uint8_t *data, size_t len) override {
    if (this->rx_len_ - this->rx_pos_ < len) {
      return false;
    }
    std::memcpy(data, this->rx_ + this->rx_pos_, len);
    this->rx_pos_ += len;
    return true;
  }
  size_t available() override { return this->rx_len_ - this->rx_pos_; }
  size_t available_for_write() override { return this->block_tx_ ? 0 : this->room_; }
  UARTFlushResult flush() override { return UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }
  void load_settings(bool /*dump_config*/) override {}

  void rx(const uint8_t *data, size_t len) {
    this->rx_pos_ = 0;
    this->rx_len_ = 0;
    for (size_t i = 0; i < len && this->rx_len_ < sizeof(this->rx_); i++) {
      this->rx_[this->rx_len_++] = data[i];
    }
  }

  bool block_tx_{false};
  size_t room_{SIZE_MAX};
  uint8_t tx_[32]{};
  size_t tx_len_{0};

 protected:
  void check_logger_conflict() override {}

  uint8_t rx_[32]{};
  size_t rx_len_{0};
  size_t rx_pos_{0};
};

class UARTBridgeCopy : public ::testing::Test {
 protected:
  void SetUp() override {
    this->a_.set_baud_rate(9600);
    this->b_.set_baud_rate(9600);
    this->bridge_.set_a(&this->a_);
    this->bridge_.set_b(&this->b_);
  }

  BridgeFakeUart a_;
  BridgeFakeUart b_;
  UARTBridge bridge_;
};

TEST_F(UARTBridgeCopy, ByteOnAReachesB) {
  const uint8_t byte = 0x11;
  this->a_.rx(&byte, 1);
  this->bridge_.loop();
  EXPECT_EQ(this->b_.tx_len_, 1u);
  EXPECT_EQ(this->b_.tx_[0], 0x11);
  EXPECT_EQ(this->a_.available(), 0u);
}

TEST_F(UARTBridgeCopy, ByteOnBReachesA) {
  const uint8_t byte = 0x22;
  this->b_.rx(&byte, 1);
  this->bridge_.loop();
  EXPECT_EQ(this->a_.tx_len_, 1u);
  EXPECT_EQ(this->a_.tx_[0], 0x22);
}

TEST_F(UARTBridgeCopy, FullSideDoesNotConsumeTheByte) {
  this->b_.block_tx_ = true;
  const uint8_t byte = 0x33;
  this->a_.rx(&byte, 1);
  this->bridge_.loop();
  EXPECT_EQ(this->b_.tx_len_, 0u);
  EXPECT_EQ(this->a_.available(), 1u);
  this->b_.block_tx_ = false;
  this->bridge_.loop();
  EXPECT_EQ(this->b_.tx_len_, 1u);
  EXPECT_EQ(this->b_.tx_[0], 0x33);
}

TEST_F(UARTBridgeCopy, UnknownRoomIsPacedToTheBaudRate) {
  uint8_t bytes[30];
  for (size_t i = 0; i < sizeof(bytes); i++) {
    bytes[i] = static_cast<uint8_t>(i);
  }
  this->a_.rx(bytes, sizeof(bytes));
  this->bridge_.loop();
  // 9600 baud, 10 bits per byte, one 16 ms pass: 9600 / 625 = 15.
  EXPECT_EQ(this->b_.tx_len_, 15u);
  EXPECT_EQ(this->a_.available(), 15u);
}

TEST_F(UARTBridgeCopy, KnownRoomLimitsTheCopy) {
  this->b_.room_ = 4;
  uint8_t bytes[10];
  for (size_t i = 0; i < sizeof(bytes); i++) {
    bytes[i] = static_cast<uint8_t>(i);
  }
  this->a_.rx(bytes, sizeof(bytes));
  this->bridge_.loop();
  EXPECT_EQ(this->b_.tx_len_, 4u);
  EXPECT_EQ(this->a_.available(), 6u);
}

}  // namespace esphome::uart::testing
