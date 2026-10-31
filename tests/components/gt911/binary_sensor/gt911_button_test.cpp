#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/gt911/touchscreen/gt911_touchscreen.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "../common.h"

namespace esphome::gt911::testing {

class GT911ButtonTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->bus_.registers[0x8048] = {0x40, 0x01, 0xF0, 0x00};
    this->hub_.set_i2c_bus(&this->bus_);
    this->hub_.set_i2c_address(0x5D);
  }

  // A touch loop defers work on the hub; a fresh App drops it before the hub goes away
  void TearDown() override {
    App.~Application();
    new (&App) Application();
  }

  void add_button_(binary_sensor::BinarySensor &button, uint8_t index) {
    this->hub_.add_button_binary_sensor(&button, index);
  }

  void setup_() { this->hub_.setup(); }

  void keys_(uint8_t keys) {
    this->bus_.set_keys(keys);
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
  GT911Touchscreen hub_;
};

TEST_F(GT911ButtonTest, SetupPublishesOff) {
  binary_sensor::BinarySensor button;
  this->add_button_(button, 0);
  std::vector<optional<bool>> published;
  button.add_full_state_callback([&published](optional<bool>, optional<bool> state) { published.push_back(state); });
  EXPECT_FALSE(button.has_state());
  this->setup_();
  ASSERT_TRUE(button.has_state());
  EXPECT_FALSE(button.state);
  EXPECT_EQ(published, (std::vector<optional<bool>>{false}));
}

TEST_F(GT911ButtonTest, ResetPinSetupPublishesOffBeforeItFinishes) {
  FakePin reset;
  this->hub_.set_reset_pin(&reset);
  GT911Button button;
  this->add_button_(button, 0);
  this->setup_();
  ASSERT_TRUE(button.has_state());
  EXPECT_FALSE(button.state);
  EXPECT_FALSE(this->hub_.can_proceed());
  this->run_timers_until_([this] { return this->hub_.can_proceed(); });
  EXPECT_TRUE(this->hub_.can_proceed());
  this->keys_(0b0001);
  EXPECT_TRUE(button.state);
}

TEST_F(GT911ButtonTest, SetupPublishesOffWhenTheHubFails) {
  this->bus_.fail = true;
  binary_sensor::BinarySensor button;
  this->add_button_(button, 0);
  this->setup_();
  EXPECT_TRUE(this->hub_.is_failed());
  ASSERT_TRUE(button.has_state());
  EXPECT_FALSE(button.state);
}

TEST_F(GT911ButtonTest, EachButtonFollowsItsKeyBit) {
  binary_sensor::BinarySensor key0;
  binary_sensor::BinarySensor key2;
  binary_sensor::BinarySensor key3;
  this->add_button_(key0, 0);
  this->add_button_(key2, 2);
  this->add_button_(key3, 3);
  this->setup_();
  std::vector<bool> states;
  key2.add_on_state_callback([&states](bool state) { states.push_back(state); });

  this->keys_(0b0100);
  EXPECT_FALSE(key0.state);
  EXPECT_TRUE(key2.state);
  EXPECT_FALSE(key3.state);
  this->keys_(0b1001);
  EXPECT_TRUE(key0.state);
  EXPECT_FALSE(key2.state);
  EXPECT_TRUE(key3.state);
  // Bits above the four keys are ignored
  this->keys_(0b11110000);
  EXPECT_FALSE(key0.state);
  EXPECT_FALSE(key2.state);
  EXPECT_FALSE(key3.state);
  EXPECT_EQ(states, (std::vector<bool>{true, false}));
}

TEST_F(GT911ButtonTest, KeysPublishInIndexOrder) {
  GT911Button key3;
  GT911Button key0;
  GT911Button key0_again;
  this->add_button_(key3, 3);
  this->add_button_(key0, 0);
  this->add_button_(key0_again, 0);
  this->setup_();
  std::vector<int> order;
  key3.add_on_state_callback([&order](bool) { order.push_back(3); });
  key0.add_on_state_callback([&order](bool) { order.push_back(0); });
  key0_again.add_on_state_callback([&order](bool) { order.push_back(10); });

  this->keys_(0b1001);
  EXPECT_EQ(order, (std::vector<int>{0, 10, 3}));
}

TEST_F(GT911ButtonTest, ButtonsSharingAnIndexBothFollowIt) {
  binary_sensor::BinarySensor first;
  binary_sensor::BinarySensor second;
  this->add_button_(first, 1);
  this->add_button_(second, 1);
  this->setup_();

  this->keys_(0b0010);
  EXPECT_TRUE(first.state);
  EXPECT_TRUE(second.state);
  this->keys_(0);
  EXPECT_FALSE(first.state);
  EXPECT_FALSE(second.state);
}

}  // namespace esphome::gt911::testing
