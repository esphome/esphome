#include <gtest/gtest.h>

#include <array>
#include <vector>

#include "esphome/components/sx1509/sx1509.h"
#include "esphome/core/hal.h"

namespace esphome::sx1509::testing {

/// Register file with the chip's auto-increment
class KeyBus final : public i2c::I2CBus {
 public:
  KeyBus() { this->registers[REG_INTERRUPT_MASK_A] = 0xFF; }

  std::array<uint8_t, 256> registers{};

  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                             size_t out_size) override {
    uint8_t reg = size != 0 ? data[0] : 0;
    for (size_t i = 1; i < size; i++)
      this->registers[reg++] = data[i];
    for (size_t i = 0; i < out_size; i++)
      out[i] = this->registers[reg++];
    return i2c::ERROR_OK;
  }

  /// The chip reports a pressed row and column as cleared bits
  void press(uint16_t key_data) {
    const uint16_t raw = 0xFFFF ^ key_data;
    this->registers[REG_KEY_DATA_1] = raw >> 8;
    this->registers[REG_KEY_DATA_1 + 1] = raw & 0xFF;
  }
};

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
  KeyBus bus_;
  SX1509Component chip_;

  void SetUp() override {
    key_row0_col1.states.clear();
    key_row1_col0.states.clear();
    this->chip_.set_i2c_bus(&this->bus_);
    this->chip_.set_rows_cols(2, 2);
  }

  void scan(uint16_t key_data) {
    this->bus_.press(key_data);
    delay(16);  // past the scan period
    this->chip_.loop();
  }
};

TEST_F(SX1509KeypadTest, PublishesPressAndReleaseOfEachKey) {
  this->chip_.set_keypad_publisher(keypad_fn);
  this->chip_.setup();
  this->scan((1 << 0) | (1 << 9));
  this->scan(0);
  this->scan((1 << 1) | (1 << 8));
  EXPECT_EQ(key_row0_col1.states, (std::vector<bool>{true, false, false}));
  EXPECT_EQ(key_row1_col0.states, (std::vector<bool>{false, false, true}));
}

TEST_F(SX1509KeypadTest, ScansWithoutAPublisher) {
  this->chip_.setup();
  this->scan((1 << 0) | (1 << 9));
  EXPECT_FALSE(this->chip_.is_failed());
  EXPECT_TRUE(key_row0_col1.states.empty());
}

}  // namespace esphome::sx1509::testing
