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
    for (size_t i = 0; i < len && this->rx_len_ < sizeof(this->rx_); i++) {
      this->rx_[this->rx_len_++] = data[i];
    }
  }
  void rx_fill(size_t len) {
    for (size_t i = 0; i < len && this->rx_len_ < sizeof(this->rx_); i++) {
      this->rx_[this->rx_len_++] = static_cast<uint8_t>(i);
    }
  }

  uint8_t tx_[32]{};
  size_t tx_len_{0};

 protected:
  void check_logger_conflict() override {}

  uint8_t rx_[1024]{};
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

  static void fill(UartSplitOutput *output, size_t len) {
    const uint8_t byte = 0x00;
    for (size_t i = 0; i < len; i++) {
      ASSERT_TRUE(output->inject_rx(&byte, 1));
    }
  }

  static void drain(UartSplitOutput *output) {
    uint8_t byte = 0;
    while (output->read_byte(&byte)) {
    }
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

TEST_F(UartSplitCopy, SettingsChangedOnAnOutputAreRestored) {
  this->pins_.set_baud_rate(19200);
  this->bus_.set_baud_rate(115200);
  this->bus_.set_parity(uart::UART_CONFIG_PARITY_ODD);
  this->bus_.copy_settings();
  EXPECT_EQ(this->bus_.get_baud_rate(), 19200u);
  EXPECT_EQ(this->bus_.get_parity(), this->pins_.get_parity());
}

TEST_F(UartSplitCopy, OutputReportsItsBufferSize) { EXPECT_EQ(this->bus_.get_rx_buffer_size(), RX_BUFFER_SIZE); }

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
  EXPECT_EQ(this->tap_.available(), 0u);
}

TEST_F(UartSplitCopy, ReceiveOnlyOutputTakesEveryWrite) {
  // A paced writer like uart_tcp keeps reading its socket, and so sees its client leave.
  EXPECT_EQ(this->tap_.available_for_write(), SIZE_MAX);
  EXPECT_EQ(this->quiet_.available_for_write(), SIZE_MAX);
}

TEST_F(UartSplitCopy, ReceiveOnlyFlushConfirmsNothing) {
  EXPECT_EQ(this->tap_.flush(), uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS);
  EXPECT_EQ(this->bus_.flush(), uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS);
}

TEST_F(UartSplitCopy, FullWriterStopsTheRead) {
  fill(&this->bus_, RX_BUFFER_SIZE);
  const uint8_t byte = 0x44;
  this->pins_.rx(&byte, 1);
  this->split_.loop();
  EXPECT_EQ(this->pins_.available(), 1u);
  EXPECT_EQ(this->tap_.available(), 0u);
  drain(&this->bus_);
  this->split_.loop();
  EXPECT_EQ(this->pins_.available(), 0u);
  EXPECT_EQ(this->read_one(&this->bus_), 0x44);
  EXPECT_EQ(this->read_one(&this->tap_), 0x44);
}

TEST_F(UartSplitCopy, WriterRoomLimitsTheRead) {
  this->pins_.set_rx_buffer_size(256);
  fill(&this->bus_, RX_BUFFER_SIZE - 6);
  this->pins_.rx_fill(20);
  this->split_.loop();
  EXPECT_EQ(this->pins_.available(), 14u);
  EXPECT_EQ(this->bus_.available(), RX_BUFFER_SIZE);
  EXPECT_EQ(this->tap_.available(), 6u);
}

TEST_F(UartSplitCopy, StalledWriterStopsHoldingTheReadAtHalfTheParentBuffer) {
  this->pins_.set_rx_buffer_size(256);
  fill(&this->bus_, RX_BUFFER_SIZE);
  this->pins_.rx_fill(127);
  this->split_.loop();
  EXPECT_EQ(this->pins_.available(), 127u);
  EXPECT_EQ(this->tap_.available(), 0u);
  // 128 bytes waiting: the writer drops them, the others get them.
  this->pins_.rx_fill(1);
  this->split_.loop();
  EXPECT_EQ(this->pins_.available(), 0u);
  EXPECT_EQ(this->tap_.available(), 128u);
  EXPECT_EQ(this->quiet_.available(), 128u);
  EXPECT_EQ(this->bus_.available(), RX_BUFFER_SIZE);
  // Until it has been read empty, the writer no longer holds anything back.
  this->pins_.rx_fill(1);
  this->split_.loop();
  EXPECT_EQ(this->pins_.available(), 0u);
  EXPECT_EQ(this->tap_.available(), 129u);
}

TEST_F(UartSplitCopy, WriterReadEmptyHoldsTheReadAgain) {
  fill(&this->bus_, RX_BUFFER_SIZE);
  EXPECT_TRUE(this->bus_.start_dropping());
  drain(&this->bus_);
  this->pins_.rx_fill(1);
  this->split_.loop();
  EXPECT_EQ(this->bus_.available(), 1u);
  fill(&this->bus_, RX_BUFFER_SIZE - 1);
  this->pins_.rx_fill(1);
  this->split_.loop();
  EXPECT_EQ(this->pins_.available(), 1u);
}

TEST_F(UartSplitCopy, FullReceiveOnlyOutputDoesNotStopTheRead) {
  fill(&this->tap_, RX_BUFFER_SIZE);
  this->pins_.rx_fill(10);
  this->split_.loop();
  EXPECT_EQ(this->pins_.available(), 0u);
  EXPECT_EQ(this->bus_.available(), 10u);
  EXPECT_EQ(this->tap_.available(), RX_BUFFER_SIZE);
}

TEST_F(UartSplitCopy, ReadsMoreThanOneChunkPerPass) {
  this->pins_.rx_fill(200);
  this->split_.loop();
  EXPECT_EQ(this->pins_.available(), 0u);
  EXPECT_EQ(this->bus_.available(), 200u);
  EXPECT_EQ(this->tap_.available(), 200u);
}

TEST_F(UartSplitCopy, ReadsAtMostOneOutputBufferPerPass) {
  this->pins_.rx_fill(300);
  this->split_.loop();
  EXPECT_EQ(this->pins_.available(), 300u - RX_BUFFER_SIZE);
  EXPECT_EQ(this->bus_.available(), RX_BUFFER_SIZE);
}

TEST_F(UartSplitCopy, DroppedWriteDoesNotHideADroppedReceive) {
  const uint8_t byte = 0x66;
  this->tap_.write_array(&byte, 1);
  EXPECT_TRUE(this->tap_.start_dropping());
  EXPECT_FALSE(this->tap_.start_dropping());
}

TEST_F(UartSplitCopy, EmptyBufferEndsARunOfDrops) {
  EXPECT_TRUE(this->tap_.start_dropping());
  fill(&this->tap_, 1);
  this->tap_.end_dropping_if_empty();
  EXPECT_FALSE(this->tap_.start_dropping());
  drain(&this->tap_);
  this->tap_.end_dropping_if_empty();
  EXPECT_TRUE(this->tap_.start_dropping());
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
