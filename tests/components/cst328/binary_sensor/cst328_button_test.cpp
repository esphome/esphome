#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/cst328/touchscreen/cst328_touchscreen.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "../common.h"

namespace esphome::cst328::testing {

class CST328ButtonTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Firmware CRC 0xCACA in bytes 2 and 3, and a 320x240 panel
    this->bus_.registers[CST_REG_FW_CRC_AND_BOOT_TIME] = {0, 0, 0xCA, 0xCA};
    this->bus_.registers[CST_REG_X_Y_RESOLUTION] = {0x40, 0x01, 0xF0, 0x00};
    this->hub_.set_i2c_bus(&this->bus_);
    this->hub_.set_i2c_address(0x1A);
  }

  // A touch loop defers work on the hub; a fresh App drops it before the hub goes away
  void TearDown() override {
    App.~Application();
    new (&App) Application();
  }

  void add_button_(binary_sensor::BinarySensor &button) { this->hub_.add_button_binary_sensor(&button); }

  void setup_() { this->hub_.setup(); }

  void touch_(uint8_t fingers) {
    this->bus_.set_finger_count(fingers);
    this->hub_.update();
    this->hub_.loop();
  }

  // Runs the scheduler until done() holds, so setup timeouts that chain fire in turn; gives up after 2 s
  template<typename F> void run_timers_until_(F done) {
    const uint32_t start = millis();
    while (!done() && millis() - start < 2000) {
      delay(5);
      App.scheduler.call(millis());
    }
  }

  RegisterBus bus_;
  CST328Touchscreen hub_;
};

TEST_F(CST328ButtonTest, SetupPublishesOff) {
  binary_sensor::BinarySensor button;
  this->add_button_(button);
  std::vector<optional<bool>> published;
  button.add_full_state_callback([&published](optional<bool>, optional<bool> state) { published.push_back(state); });
  EXPECT_FALSE(button.has_state());
  this->setup_();
  ASSERT_TRUE(button.has_state());
  EXPECT_FALSE(button.state);
  EXPECT_EQ(published, (std::vector<optional<bool>>{false}));
}

TEST_F(CST328ButtonTest, ResetPinSetupPublishesOffBeforeItFinishes) {
  FakePin reset;
  this->hub_.set_reset_pin(&reset);
  CST328Button button;
  this->add_button_(button);
  this->setup_();
  ASSERT_TRUE(button.has_state());
  EXPECT_FALSE(button.state);
  // Setup finishes from timeouts; until then touches are skipped
  this->touch_(1);
  EXPECT_FALSE(button.state);
  this->run_timers_until_([this] { return this->bus_.last_write == CST_REG_TOUCH_INFORMATION; });
  this->touch_(1);
  EXPECT_TRUE(button.state);
}

TEST_F(CST328ButtonTest, SetupPublishesOffWhenTheHubFails) {
  this->bus_.registers.clear();
  binary_sensor::BinarySensor button;
  this->add_button_(button);
  this->setup_();
  EXPECT_TRUE(this->hub_.is_failed());
  ASSERT_TRUE(button.has_state());
  EXPECT_FALSE(button.state);
}

TEST_F(CST328ButtonTest, FingerCountDrivesState) {
  binary_sensor::BinarySensor button;
  this->add_button_(button);
  this->setup_();
  std::vector<bool> states;
  button.add_on_state_callback([&states](bool state) { states.push_back(state); });

  this->touch_(1);
  EXPECT_TRUE(button.state);
  this->touch_(5);
  EXPECT_TRUE(button.state);
  this->touch_(0);
  EXPECT_FALSE(button.state);
  this->touch_(2);
  EXPECT_TRUE(button.state);
  // More than five fingers is a bad report and releases the button
  this->touch_(6);
  EXPECT_FALSE(button.state);
  EXPECT_EQ(states, (std::vector<bool>{true, false, true, false}));
}

TEST_F(CST328ButtonTest, EveryButtonOfAHubFollowsTheTouch) {
  binary_sensor::BinarySensor first;
  binary_sensor::BinarySensor second;
  this->add_button_(first);
  this->add_button_(second);
  this->setup_();

  this->touch_(1);
  EXPECT_TRUE(first.state);
  EXPECT_TRUE(second.state);
  this->touch_(0);
  EXPECT_FALSE(first.state);
  EXPECT_FALSE(second.state);
}

}  // namespace esphome::cst328::testing
