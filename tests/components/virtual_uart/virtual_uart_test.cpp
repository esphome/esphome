#include "esphome/components/virtual_uart/virtual_uart.h"

#ifdef USE_HOST

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace esphome::virtual_uart::testing {

static constexpr uint8_t FRAME[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0x84, 0x0A};

TEST(VirtualUART, WrittenBytesGoToOnTx) {
  VirtualUART uart(16);
  std::vector<std::vector<uint8_t>> sent;
  uart.add_on_tx_callback([&sent](std::span<const uint8_t> data) { sent.emplace_back(data.begin(), data.end()); });
  uart.write_array(FRAME, sizeof(FRAME));
  uart.write_byte(0x55);
  ASSERT_EQ(sent.size(), 2u);
  EXPECT_EQ(sent[0], std::vector<uint8_t>(std::begin(FRAME), std::end(FRAME)));
  EXPECT_EQ(sent[1], std::vector<uint8_t>{0x55});
}

TEST(VirtualUART, InjectedBytesAreRead) {
  VirtualUART uart(16);
  uart.inject(FRAME, sizeof(FRAME));
  ASSERT_EQ(uart.available(), sizeof(FRAME));
  uint8_t out[sizeof(FRAME)];
  ASSERT_TRUE(uart.read_array(out, sizeof(out)));
  EXPECT_EQ(out[7], 0x0A);
}

TEST(VirtualUART, BlockThatDoesNotFitIsDropped) {
  VirtualUART uart(4);
  uart.inject(FRAME, sizeof(FRAME));
  EXPECT_EQ(uart.available(), 0u);
}

TEST(VirtualUART, InjectActionUsesStaticData) {
  VirtualUART uart(16);
  InjectRXAction<> action;
  action.set_parent(&uart);
  action.set_data_static(FRAME, sizeof(FRAME));
  action.play();
  EXPECT_EQ(uart.available(), sizeof(FRAME));
}

}  // namespace esphome::virtual_uart::testing

#endif  // USE_HOST
