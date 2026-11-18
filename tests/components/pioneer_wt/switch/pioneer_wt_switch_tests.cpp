#include <gtest/gtest.h>

#include "esphome/components/pioneer_wt/pioneer_wt.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/application.h"
#include "../common.h"

namespace esphome::pioneer_wt::testing {

namespace {

class TestSwitch : public switch_::Switch {
 protected:
  void write_state(bool state) override {}
};

}  // namespace

// Reports captured from the AC after the app changed the display and the beep.
TEST(PioneerWTSwitch, StateFollowsReports) {
  MockUARTComponent uart;
  PioneerWT sut;
  TestSwitch display;
  TestSwitch beeper;
  sut.set_uart_parent(&uart);
  sut.set_display_switch(&display);
  sut.set_beeper_switch(&beeper);
  EXPECT_FALSE(display.has_state());

  uart.push_rx_hex("a50101217400000f53010c0c001e01");
  uart.push_rx_hex("a50101217700000fb70b0c0c002500");
  sut.loop();
  EXPECT_TRUE(display.has_state());
  EXPECT_TRUE(display.state);
  EXPECT_TRUE(beeper.has_state());
  EXPECT_FALSE(beeper.state);

  uart.push_rx_hex("a50101217300000ff28b0c0c001e00");
  uart.push_rx_hex("a50101217800000f3f7e0c0c002501");
  sut.loop();
  EXPECT_FALSE(display.state);
  EXPECT_TRUE(beeper.state);
}

}  // namespace esphome::pioneer_wt::testing
