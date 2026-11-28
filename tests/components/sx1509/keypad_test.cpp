#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/sx1509/sx1509.h"
#include "esphome/core/hal.h"

#include "common.h"

namespace esphome::sx1509::testing {

struct KeySensor {
  std::vector<bool> states;
  void publish_state(bool state) { this->states.push_back(state); }
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static KeySensor key_row0_col1, key_row1_col0;

/// What codegen generates for two keypad binary sensors
static void keypad_fn(uint16_t key_data) {
  publish_key(&key_row0_col1, key_data, (1 << 0) | (1 << 9));
  publish_key(&key_row1_col0, key_data, (1 << 1) | (1 << 8));
}

class SX1509KeypadTest : public ::testing::Test {
 protected:
  RegisterBus bus_;
  SX1509Component chip_;

  void SetUp() override {
    key_row0_col1.states.clear();
    key_row1_col0.states.clear();
    this->chip_.set_i2c_bus(&this->bus_);
    this->chip_.set_rows_cols(2, 2);
  }

  void scan_(uint16_t key_data) {
    this->bus_.press(key_data);
    delay(16);  // past the scan period
    this->chip_.loop();
  }
};

TEST_F(SX1509KeypadTest, PublishesPressAndReleaseOfEachKey) {
  this->chip_.set_keypad_publisher(keypad_fn);
  this->chip_.setup();
  this->scan_((1 << 0) | (1 << 9));
  this->scan_(0);
  this->scan_((1 << 1) | (1 << 8));
  EXPECT_EQ(key_row0_col1.states, (std::vector<bool>{true, false, false}));
  EXPECT_EQ(key_row1_col0.states, (std::vector<bool>{false, false, true}));
}

TEST_F(SX1509KeypadTest, ScansWithoutAPublisher) {
  this->chip_.setup();
  this->scan_((1 << 0) | (1 << 9));
  EXPECT_FALSE(this->chip_.is_failed());
  EXPECT_TRUE(key_row0_col1.states.empty());
}

}  // namespace esphome::sx1509::testing
