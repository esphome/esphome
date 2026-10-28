#include <gtest/gtest.h>

#include "common.h"

namespace esphome::uart::testing {

// 8N1 is start + 8 data + 1 stop = 10 bits; even parity adds one bit.
TEST(UARTWireTime, BitsPerCharFollowTheFraming) {
  MockUARTComponent uart;
  uart.set_data_bits(8);
  uart.set_stop_bits(1);
  EXPECT_EQ(uart.bits_per_char(), 10);

  uart.set_parity(UART_CONFIG_PARITY_EVEN);
  EXPECT_EQ(uart.bits_per_char(), 11);
}

// A UART that never sets data or stop bits counts as 8N1.
TEST(UARTWireTime, UnsetBitsCountAsEightNoneOne) {
  MockUARTComponent uart;
  EXPECT_EQ(uart.bits_per_char(), 10);
}

// One 8N1 character at 9600 baud takes 10 / 9600 s = 1041.7 us, seven take 7291.7 us.
TEST(UARTWireTime, CharactersAtNineThousandSixHundredBaud) {
  MockUARTComponent uart;
  uart.set_baud_rate(9600);
  uart.set_data_bits(8);
  uart.set_stop_bits(1);
  EXPECT_EQ(uart.wire_time_us(1), 1041u);
  EXPECT_EQ(uart.wire_time_us(7), 7291u);
}

// Without a baud rate the time is taken at 1 baud: 10 bits take 10 s.
TEST(UARTWireTime, NoBaudRateCountsAsOneBaud) {
  MockUARTComponent uart;
  EXPECT_EQ(uart.wire_time_us(1), 10000000u);
}

// 8E2 is 12 bits, the longest character. At 2 baud, 357 characters take 357 * 12 / 2 = 2142 s and 358
// take 2148 s; 357 is the last count computed in 32 bits, 358 the first one computed in 64 bits.
TEST(UARTWireTime, LongestCharactersAroundThe32BitBound) {
  MockUARTComponent uart;
  uart.set_baud_rate(2);
  uart.set_data_bits(8);
  uart.set_parity(UART_CONFIG_PARITY_EVEN);
  uart.set_stop_bits(2);
  EXPECT_EQ(uart.bits_per_char(), 12);
  EXPECT_EQ(uart.wire_time_us(357), 2142000000u);
  EXPECT_EQ(uart.wire_time_us(358), 2148000000u);
}

}  // namespace esphome::uart::testing
