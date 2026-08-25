#include <gtest/gtest.h>

#include <cstring>
#include <deque>

#include "esphome/components/emontx/emontx.h"
#include "esphome/components/uart/uart_component.h"

namespace esphome::emontx::testing {

// Minimal fake UART that serves bytes from an in-memory queue, so loop()'s
// interaction with the UART bus can be observed without real hardware.
class FakeUART final : public uart::UARTComponent {
 public:
  void feed(const std::string &data) {
    for (char c : data) {
      this->rx_.push_back(static_cast<uint8_t>(c));
    }
  }

  void write_array(const uint8_t *data, size_t len) override {}
  bool peek_byte(uint8_t *data) override {
    if (this->rx_.empty())
      return false;
    *data = this->rx_.front();
    return true;
  }
  bool read_array(uint8_t *data, size_t len) override {
    if (this->rx_.size() < len)
      return false;
    for (size_t i = 0; i < len; i++) {
      data[i] = this->rx_.front();
      this->rx_.pop_front();
    }
    return true;
  }
  size_t available() override { return this->rx_.size(); }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }

 protected:
  void check_logger_conflict() override {}

 private:
  std::deque<uint8_t> rx_;
};

TEST(EmonTxPause, LoopConsumesBytesWhenNotPaused) {
  FakeUART uart;
  EmonTx emontx;
  emontx.set_uart_parent(&uart);
  emontx.setup();

  uart.feed("{\"P1\":123}\n");
  emontx.loop();

  EXPECT_EQ(uart.available(), 0u);
}

TEST(EmonTxPause, LoopLeavesBytesUntouchedWhenPaused) {
  FakeUART uart;
  EmonTx emontx;
  emontx.set_uart_parent(&uart);
  emontx.setup();

  uart.feed("{\"P1\":123}\n");
  emontx.set_paused(true);
  emontx.loop();

  EXPECT_EQ(uart.available(), std::strlen("{\"P1\":123}\n"));
}

TEST(EmonTxPause, LoopResumesConsumingBytesAfterUnpause) {
  FakeUART uart;
  EmonTx emontx;
  emontx.set_uart_parent(&uart);
  emontx.setup();

  uart.feed("{\"P1\":123}\n");
  emontx.set_paused(true);
  emontx.loop();
  ASSERT_EQ(uart.available(), std::strlen("{\"P1\":123}\n"));

  emontx.set_paused(false);
  emontx.loop();

  EXPECT_EQ(uart.available(), 0u);
}

}  // namespace esphome::emontx::testing
