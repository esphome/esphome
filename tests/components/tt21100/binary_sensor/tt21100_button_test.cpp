#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/tt21100/binary_sensor/tt21100_button.h"
#include "esphome/components/tt21100/touchscreen/tt21100.h"
#include "esphome/core/application.h"
#include "../common.h"

namespace esphome::tt21100::testing {

class TT21100ButtonTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->hub_.set_i2c_bus(&this->bus_);
    this->hub_.set_i2c_address(0x24);
    this->hub_.set_interrupt_pin(nullptr);
  }

  // A touch loop defers work on the hub; a fresh App drops it before the hub goes away
  void TearDown() override {
    App.~Application();
    new (&App) Application();
  }

  void add_button_(TT21100Button &button, uint8_t index) {
    button.set_parent(&this->hub_);
    button.set_index(index);
    this->buttons_.push_back(&button);
  }

  void setup_() {
    this->hub_.setup();
    for (auto *button : this->buttons_)
      button->setup();
  }

  void signals_(uint16_t s0, uint16_t s1, uint16_t s2, uint16_t s3) {
    this->bus_.set_signals(s0, s1, s2, s3);
    this->hub_.update();
    this->hub_.loop();
  }

  ReportBus bus_;
  TT21100Touchscreen hub_;
  std::vector<TT21100Button *> buttons_;
};

TEST_F(TT21100ButtonTest, SetupPublishesOff) {
  TT21100Button button;
  this->add_button_(button, 0);
  std::vector<optional<bool>> published;
  button.add_full_state_callback([&published](optional<bool>, optional<bool> state) { published.push_back(state); });
  EXPECT_FALSE(button.has_state());
  this->setup_();
  ASSERT_TRUE(button.has_state());
  EXPECT_FALSE(button.state);
  EXPECT_EQ(published, (std::vector<optional<bool>>{false}));
}

TEST_F(TT21100ButtonTest, EachButtonFollowsItsSignal) {
  TT21100Button key0;
  TT21100Button key1;
  TT21100Button key3;
  this->add_button_(key0, 0);
  this->add_button_(key1, 1);
  this->add_button_(key3, 3);
  this->setup_();
  std::vector<bool> states;
  key1.add_on_state_callback([&states](bool state) { states.push_back(state); });

  this->signals_(0, 0x0120, 0x0001, 0);
  EXPECT_FALSE(key0.state);
  EXPECT_TRUE(key1.state);
  EXPECT_FALSE(key3.state);
  this->signals_(0, 0x0001, 0, 0);
  EXPECT_TRUE(key1.state);
  this->signals_(0x8000, 0, 0, 0x0002);
  EXPECT_TRUE(key0.state);
  EXPECT_FALSE(key1.state);
  EXPECT_TRUE(key3.state);
  EXPECT_EQ(states, (std::vector<bool>{true, false}));
}

TEST_F(TT21100ButtonTest, ButtonsSharingAnIndexBothFollowIt) {
  TT21100Button first;
  TT21100Button second;
  this->add_button_(first, 2);
  this->add_button_(second, 2);
  this->setup_();

  this->signals_(0, 0, 1, 0);
  EXPECT_TRUE(first.state);
  EXPECT_TRUE(second.state);
  this->signals_(0, 0, 0, 0);
  EXPECT_FALSE(first.state);
  EXPECT_FALSE(second.state);
}

}  // namespace esphome::tt21100::testing
