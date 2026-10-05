#include <gtest/gtest.h>

#include <cstring>

#include "esphome/components/uart_split/uart_split.h"

#ifdef USE_HOST

namespace esphome::uart_split::testing {

class FakeUart : public uart::UARTComponent {
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
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }

  void rx(const uint8_t *data, size_t len) {
    this->rx_pos_ = 0;
    this->rx_len_ = 0;
    for (size_t i = 0; i < len && this->rx_len_ < sizeof(this->rx_); i++) {
      this->rx_[this->rx_len_++] = data[i];
    }
  }

  uint8_t tx_[32]{};
  size_t tx_len_{0};

 protected:
  void check_logger_conflict() override {}

  uint8_t rx_[32]{};
  size_t rx_len_{0};
  size_t rx_pos_{0};
};

class UartSplitCopy : public ::testing::Test {
 protected:
  void SetUp() override {
    this->tap_.set_rx_only(true);
    this->tap_.set_mirror_tx(true);
    this->quiet_.set_rx_only(true);
    this->split_.add_output(&this->bus_);
    this->split_.add_output(&this->tap_);
    this->split_.add_output(&this->quiet_);
  }

  uint8_t read_one(uart::UARTComponent *uart) {
    uint8_t byte = 0;
    EXPECT_TRUE(uart->read_byte(&byte));
    return byte;
  }

  FakeUart pins_;
  UartSplit split_{&this->pins_};
  UartSplitOutput bus_{&this->split_};
  UartSplitOutput tap_{&this->split_};
  UartSplitOutput quiet_{&this->split_};
};

TEST_F(UartSplitCopy, OutputsReportTheSettingsOfThePins) {
  this->pins_.set_baud_rate(19200);
  this->pins_.set_data_bits(7);
  this->pins_.set_parity(uart::UART_CONFIG_PARITY_EVEN);
  this->pins_.set_stop_bits(2);
  this->split_.setup();
  for (UartSplitOutput *output : {&this->bus_, &this->tap_, &this->quiet_}) {
    EXPECT_EQ(output->get_baud_rate(), 19200u);
    EXPECT_EQ(output->get_data_bits(), 7);
    EXPECT_EQ(output->get_parity(), uart::UART_CONFIG_PARITY_EVEN);
    EXPECT_EQ(output->get_stop_bits(), 2);
  }
}

TEST_F(UartSplitCopy, ReceivedByteReachesEveryOutput) {
  const uint8_t byte = 0x11;
  this->pins_.rx(&byte, 1);
  this->split_.loop();
  EXPECT_EQ(this->read_one(&this->bus_), 0x11);
  EXPECT_EQ(this->read_one(&this->tap_), 0x11);
  EXPECT_EQ(this->read_one(&this->quiet_), 0x11);
  EXPECT_EQ(this->pins_.available(), 0u);
}

TEST_F(UartSplitCopy, SentByteIsMirroredOnlyToBoth) {
  const uint8_t byte = 0x22;
  this->bus_.write_array(&byte, 1);
  EXPECT_EQ(this->pins_.tx_len_, 1u);
  EXPECT_EQ(this->pins_.tx_[0], 0x22);
  EXPECT_EQ(this->bus_.available(), 0u);
  EXPECT_EQ(this->read_one(&this->tap_), 0x22);
  EXPECT_EQ(this->quiet_.available(), 0u);
}

TEST_F(UartSplitCopy, ReceiveOnlyWriteDoesNotReachThePins) {
  const uint8_t byte = 0x33;
  this->tap_.write_array(&byte, 1);
  EXPECT_EQ(this->pins_.tx_len_, 0u);
  EXPECT_EQ(this->tap_.available_for_write(), 0u);
}

TEST_F(UartSplitCopy, FullWriterStopsTheRead) {
  for (size_t i = 0; i < RX_BUFFER_SIZE; i++) {
    ASSERT_TRUE(this->bus_.push_rx(0x00));
  }
  const uint8_t byte = 0x44;
  this->pins_.rx(&byte, 1);
  this->split_.loop();
  EXPECT_EQ(this->pins_.available(), 1u);
  EXPECT_EQ(this->tap_.available(), 0u);
  for (size_t i = 0; i < RX_BUFFER_SIZE; i++) {
    uint8_t discarded = 0;
    ASSERT_TRUE(this->bus_.read_byte(&discarded));
  }
  this->split_.loop();
  EXPECT_EQ(this->pins_.available(), 0u);
  EXPECT_EQ(this->read_one(&this->bus_), 0x44);
  EXPECT_EQ(this->read_one(&this->tap_), 0x44);
}

TEST_F(UartSplitCopy, TwoWritersBothReachThePins) {
  UartSplitOutput other{&this->split_};
  this->split_.add_output(&other);
  const uint8_t first = 0x41;
  const uint8_t second = 0x42;
  this->bus_.write_array(&first, 1);
  other.write_array(&second, 1);
  EXPECT_EQ(this->pins_.tx_len_, 2u);
  EXPECT_EQ(this->pins_.tx_[0], 0x41);
  EXPECT_EQ(this->pins_.tx_[1], 0x42);
  EXPECT_EQ(this->read_one(&this->tap_), 0x41);
  EXPECT_EQ(this->read_one(&this->tap_), 0x42);
}

}  // namespace esphome::uart_split::testing

#endif
