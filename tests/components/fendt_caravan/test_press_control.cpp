#include "esphome/components/fendt_caravan/press_control.h"

#include <gtest/gtest.h>

namespace esphome::fendt_caravan::testing {

// Queues `command` the way the hub does and has the link write it at `now`
static void send(PanelControl &control, const char *command, uint32_t now) {
  ASSERT_NE(command, nullptr);
  control.on_queued(true);
  control.on_command_done(true, now);
}

TEST(PanelControl, NothingIsSentBeforeTheFirstReport) {
  PanelControl key(nullptr);
  EXPECT_EQ(key.request(1), nullptr);
}

TEST(PanelControl, OffToAllLightsTakesTwoPresses) {
  PanelControl key(nullptr);
  EXPECT_EQ(key.on_report(0, 0), nullptr);
  const char *first = key.request(2);
  EXPECT_EQ(first, CMD_HS_KEY);
  send(key, first, 100);
  const char *second = key.on_report(1, 500);
  EXPECT_EQ(second, CMD_HS_KEY);
  send(key, second, 600);
  EXPECT_EQ(key.on_report(2, 900), nullptr);
  EXPECT_EQ(key.current(), 2);
}

TEST(PanelControl, OffIsALongPress) {
  PanelControl key(nullptr);
  key.on_report(2, 0);
  EXPECT_EQ(key.request(0), CMD_HS_KEY_LONG);
}

TEST(PanelControl, RepeatedRequestIsANoop) {
  PanelControl heater(CMD_FLOOR_HEATER_TOGGLE);
  heater.on_report(0, 0);
  const char *command = heater.request(1);
  EXPECT_EQ(command, CMD_FLOOR_HEATER_TOGGLE);
  heater.on_queued(true);
  // Still queued behind other commands well past the settling time
  heater.expire(10000);
  EXPECT_EQ(heater.request(1), nullptr);
}

TEST(PanelControl, OppositeRequestWhileQueuedWaitsForTheReport) {
  PanelControl key(nullptr);
  key.on_report(1, 0);
  const char *on = key.request(2);
  ASSERT_EQ(on, CMD_HS_KEY);
  key.on_queued(true);
  EXPECT_EQ(key.request(1), nullptr);  // no second press while the first is out
  EXPECT_EQ(key.request(1), nullptr);  // and a repeat of it adds nothing
  key.on_command_done(true, 100);
  // The first press is reported; the latest request still differs, so exactly one more press goes
  const char *off = key.on_report(2, 400);
  EXPECT_EQ(off, CMD_HS_KEY);
  send(key, off, 500);
  EXPECT_EQ(key.on_report(1, 800), nullptr);
}

TEST(PanelControl, ReportWhileQueuedDoesNotSendAgain) {
  PanelControl heater(CMD_FLOOR_HEATER_TOGGLE);
  heater.on_report(0, 0);
  heater.request(1);
  heater.on_queued(true);
  EXPECT_EQ(heater.on_report(0, 100), nullptr);
}

TEST(PanelControl, IgnoredPressIsNotSentByALaterReport) {
  PanelControl key(nullptr);
  key.on_report(2, 0);
  send(key, key.request(0), 100);
  // The panel never reports the long press; hours later the lights are turned off at the panel
  EXPECT_EQ(key.on_report(1, 100 + PRESS_SETTLE_MS + 3600000), nullptr);
  EXPECT_EQ(key.current(), 1);
}

TEST(PanelControl, GivenUpCommandForgetsTheRequest) {
  PanelControl heater(CMD_FLOOR_HEATER_TOGGLE);
  heater.on_report(0, 0);
  heater.request(1);
  heater.on_queued(true);
  heater.on_command_done(false, 100);
  EXPECT_EQ(heater.current(), 0);
  EXPECT_EQ(heater.on_report(0, 200), nullptr);
}

TEST(PanelControl, RefusedQueueForgetsTheRequest) {
  PanelControl heater(CMD_FLOOR_HEATER_TOGGLE);
  heater.on_report(0, 0);
  heater.request(1);
  heater.on_queued(false);
  EXPECT_EQ(heater.current(), 0);
}

TEST(PanelControl, ResetClearsEverything) {
  PanelControl key(nullptr);
  key.on_report(1, 0);
  key.request(2);
  key.on_queued(true);
  key.reset();
  EXPECT_EQ(key.state(), -1);
  EXPECT_EQ(key.current(), -1);
  EXPECT_EQ(key.request(2), nullptr);
}

TEST(PanelControl, SendsOnlyItsOwnCommands) {
  PanelControl key(nullptr);
  PanelControl heater(CMD_FLOOR_HEATER_TOGGLE);
  EXPECT_TRUE(key.sends(CMD_HS_KEY));
  EXPECT_TRUE(key.sends(CMD_HS_KEY_LONG));
  EXPECT_FALSE(key.sends(CMD_FLOOR_HEATER_TOGGLE));
  EXPECT_TRUE(heater.sends(CMD_FLOOR_HEATER_TOGGLE));
  EXPECT_FALSE(heater.sends(CMD_HS_KEY));
  EXPECT_FALSE(heater.sends("net-BT_VARS"));
}

}  // namespace esphome::fendt_caravan::testing
