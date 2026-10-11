#include <gtest/gtest.h>

#include <cstdint>

#include "common.h"
#include "esphome/components/modbus/modbus.h"

namespace esphome::modbus::testing {

namespace {

// Exposes the timing values setup() derives from the UART framing.
class FramingProbeHub : public ModbusClientHub {
 public:
  uint32_t bits_per_char() const { return this->bits_per_char_; }
  uint32_t frame_delay_us() const { return this->frame_delay_us_; }
};

class FramedUART : public NullUART {
 public:
  FramedUART(uint32_t baud_rate, uint8_t data_bits, uint8_t stop_bits, uart::UARTParityOptions parity) {
    this->set_baud_rate(baud_rate);
    this->set_data_bits(data_bits);
    this->set_stop_bits(stop_bits);
    this->set_parity(parity);
  }
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

}  // namespace esphome::modbus::testing
