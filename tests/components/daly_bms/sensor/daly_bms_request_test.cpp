#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/daly_bms/daly_bms.h"
#include "../common.h"

namespace esphome::daly_bms::testing {

namespace {

std::vector<uint8_t> request_for(DalyBmsComponent &bms, FakeUART &uart) {
  bms.setup();
  bms.update();
  bms.loop();
  return uart.tx;
}

// The last byte is the low byte of the sum of the start flag, address, command and data length.
TEST(DalyBmsRequestTest, BatteryLevelRequestFrame) {
  FakeUART uart;
  DalyBmsComponent bms;
  bms.set_uart_parent(&uart);
  bms.set_address(0x80);
  const std::vector<uint8_t> expected{0xA5, 0x80, 0x90, 0x08, 0, 0, 0, 0, 0, 0, 0, 0, 0xBD};
  EXPECT_EQ(request_for(bms, uart), expected);
}

TEST(DalyBmsRequestTest, ChecksumUsesAddress) {
  FakeUART uart;
  DalyBmsComponent bms;
  bms.set_uart_parent(&uart);
  bms.set_address(0x40);
  const std::vector<uint8_t> expected{0xA5, 0x40, 0x90, 0x08, 0, 0, 0, 0, 0, 0, 0, 0, 0x7D};
  EXPECT_EQ(request_for(bms, uart), expected);
}

}  // namespace

}  // namespace esphome::daly_bms::testing
