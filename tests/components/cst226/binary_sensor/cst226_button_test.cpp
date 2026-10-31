#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/cst226/binary_sensor/cs226_button.h"
#include "esphome/components/cst226/touchscreen/cst226_touchscreen.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "../common.h"

namespace esphome::cst226::testing {

class CST226ButtonTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->hub_.set_i2c_bus(&this->bus_);
    this->hub_.set_i2c_address(0x5A);
  }

  // A touch loop defers work on the hub; a fresh App drops it before the hub goes away
  void TearDown() override {
    App.~Application();
    new (&App) Application();
  }

  void add_button_(CST226Button &button) {
    button.set_parent(&this->hub_);
    this->buttons_.push_back(&button);
  }

  void setup_() {
    this->hub_.setup();
    for (auto *button : this->buttons_)
      button->setup();
  }

  void key_(bool pressed) {
    this->bus_.set_key(pressed);
    this->hub_.update();
    this->hub_.loop();
  }

  // Runs the scheduler for ms, so setup timeouts that chain fire in turn
  static void run_timers_(uint32_t ms) {
    const uint32_t end = millis() + ms;
    while (static_cast<int32_t>(end - millis()) > 0) {
      delay(5);
      App.scheduler.call(millis());
    }
  }

  StatusBus bus_;
  CST226Touchscreen hub_;
  std::vector<CST226Button *> buttons_;
};

TEST_F(CST226ButtonTest, SetupPublishesOff) {
  CST226Button button;
  this->add_button_(button);
  std::vector<optional<bool>> published;
  button.add_full_state_callback([&published](optional<bool>, optional<bool> state) { published.push_back(state); });
  EXPECT_FALSE(button.has_state());
  this->setup_();
  ASSERT_TRUE(button.has_state());
  EXPECT_FALSE(button.state);
  EXPECT_EQ(published, (std::vector<optional<bool>>{false}));
}

TEST_F(CST226ButtonTest, ResetPinSetupPublishesOffBeforeItFinishes) {
  FakePin reset;
  this->hub_.set_reset_pin(&reset);
  CST226Button button;
  this->add_button_(button);
  this->setup_();
  ASSERT_TRUE(button.has_state());
  EXPECT_FALSE(button.state);
  EXPECT_FALSE(this->hub_.can_proceed());
  this->run_timers_(100);
  EXPECT_TRUE(this->hub_.can_proceed());
  this->key_(true);
  EXPECT_TRUE(button.state);
}

TEST_F(CST226ButtonTest, SetupPublishesOffWhenTheHubFails) {
  this->bus_.fail = true;
  CST226Button button;
  this->add_button_(button);
  this->setup_();
  EXPECT_TRUE(this->hub_.is_failed());
  ASSERT_TRUE(button.has_state());
  EXPECT_FALSE(button.state);
}

TEST_F(CST226ButtonTest, KeyReportDrivesState) {
  CST226Button button;
  this->add_button_(button);
  this->setup_();
  std::vector<bool> states;
  button.add_on_state_callback([&states](bool state) { states.push_back(state); });

  this->key_(true);
  EXPECT_TRUE(button.state);
  this->key_(true);
  EXPECT_TRUE(button.state);
  this->key_(false);
  EXPECT_FALSE(button.state);
  this->key_(true);
  EXPECT_TRUE(button.state);
  EXPECT_EQ(states, (std::vector<bool>{true, false, true}));
}

TEST_F(CST226ButtonTest, FailedReadKeepsState) {
  CST226Button button;
  this->add_button_(button);
  this->setup_();
  this->key_(true);
  this->bus_.fail = true;
  this->hub_.update();
  this->hub_.loop();
  EXPECT_TRUE(button.state);
}

TEST_F(CST226ButtonTest, EveryButtonOfAHubFollowsTheKey) {
  CST226Button first;
  CST226Button second;
  this->add_button_(first);
  this->add_button_(second);
  this->setup_();

  this->key_(true);
  EXPECT_TRUE(first.state);
  EXPECT_TRUE(second.state);
  this->key_(false);
  EXPECT_FALSE(first.state);
  EXPECT_FALSE(second.state);
}

}  // namespace esphome::cst226::testing
