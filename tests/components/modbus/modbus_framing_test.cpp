#include <gtest/gtest.h>

#include <cstdint>

#include "common.h"
#include "esphome/components/modbus/modbus.h"

namespace esphome::modbus::testing {

namespace {

// Exposes the timing values setup() derives from the UART framing.
class FramingProbeHub : public ModbusClientHub {
 public:
  uint32_t bits_per_char() const { return this->parent_->bits_per_char(); }
  uint32_t frame_delay_us() const { return this->frame_delay_us_; }
  uint32_t long_rx_buffer_delay_us() const { return this->long_rx_buffer_delay_us_; }
  uint32_t rx_detect_latency_us() const { return this->rx_detect_latency_us_; }
  uint32_t last_send_tx_offset() const { return this->last_send_tx_offset_; }
  bool send(const ModbusFrame &frame) { return this->send_frame_(frame); }
};

class FramedUART : public NullUART {
 public:
  FramedUART(uint32_t baud_rate, uint8_t data_bits, uint8_t stop_bits, uart::UARTParityOptions parity) {
    this->set_baud_rate(baud_rate);
    this->set_data_bits(data_bits);
    this->set_stop_bits(stop_bits);
    this->set_parity(parity);
  }
  // The base class ignores both; an ESP32 UART keeps them.
  void set_rx_full_threshold(size_t rx_full_threshold) override { this->rx_full_threshold_ = rx_full_threshold; }
  void set_rx_timeout(size_t rx_timeout) override { this->rx_timeout_ = rx_timeout; }
};

}  // namespace

// 8N1 is 10 bits on the wire, so t3.5 at 9600 baud is 3.5 * 10 / 9600 = 3645.8us.
TEST(ModbusFraming, EightNoneOneDerivesTenBits) {
  FramedUART uart(9600, 8, 1, uart::UART_CONFIG_PARITY_NONE);
  FramingProbeHub hub;
  hub.set_uart_parent(&uart);
  hub.setup();

  EXPECT_EQ(hub.bits_per_char(), 10u);
  EXPECT_EQ(hub.frame_delay_us(), 3646u);
}

// Spec-conformant RTU framing is 11 bits, which lengthens the interframe gap to
// 3.5 * 11 / 9600 = 4010.4us, rounded up.
TEST(ModbusFraming, EightEvenOneDerivesElevenBits) {
  FramedUART uart(9600, 8, 1, uart::UART_CONFIG_PARITY_EVEN);
  FramingProbeHub hub;
  hub.set_uart_parent(&uart);
  hub.setup();

  EXPECT_EQ(hub.bits_per_char(), 11u);
  EXPECT_EQ(hub.frame_delay_us(), 4011u);
}

// Above 19200 baud the spec's fixed 1750us floor governs instead of 3.5 characters.
TEST(ModbusFraming, FastBaudUsesSpecFloor) {
  FramedUART uart(115200, 8, 1, uart::UART_CONFIG_PARITY_NONE);
  FramingProbeHub hub;
  hub.set_uart_parent(&uart);
  hub.setup();

  EXPECT_EQ(hub.frame_delay_us(), 1750u);
}

// The ESP32 defaults at 9600 8N1 are an RX threshold of 8 characters and an RX timeout of 2:
// 8 * 10 / 9600 = 8333.3us, rounded up, and 2 * 10 / 9600 = 2083.3us, rounded down.
TEST(ModbusFraming, RxDelaysFollowTheFraming) {
  FramedUART uart(9600, 8, 1, uart::UART_CONFIG_PARITY_NONE);
  uart.set_rx_full_threshold(8);
  uart.set_rx_timeout(2);
  FramingProbeHub hub;
  hub.set_uart_parent(&uart);
  hub.setup();

  EXPECT_EQ(hub.long_rx_buffer_delay_us(), 8334u);
  EXPECT_EQ(hub.rx_detect_latency_us(), 2083u);
}

// An 8-byte request at 9600 8N1 is on the wire for 8 * 10 / 9600 = 8333.3us, rounded up.
TEST(ModbusFraming, SendOffsetIsTheFrameOnTheWire) {
  FramedUART uart(9600, 8, 1, uart::UART_CONFIG_PARITY_NONE);
  FramingProbeHub hub;
  hub.set_uart_parent(&uart);
  hub.setup();

  const uint8_t read_one_register[] = {0x03, 0x00, 0x00, 0x00, 0x01};
  ASSERT_TRUE(hub.send(ModbusFrame(1, read_one_register, sizeof(read_one_register))));
  EXPECT_EQ(hub.last_send_tx_offset(), 8334u);
}

}  // namespace esphome::modbus::testing
