#include <gtest/gtest.h>

#include <array>

#include "esphome/components/output/float_output.h"
#include "esphome/components/sx1509/sx1509.h"

namespace esphome::sx1509::testing {

/// Register file with the chip's auto-increment; the setup check and LED intensities hold their reset values.
class RegisterBus final : public i2c::I2CBus {
 public:
  RegisterBus() {
    this->registers[REG_INTERRUPT_MASK_A] = 0xFF;
    for (uint8_t reg : REG_I_ON)
      this->registers[reg] = 0xFF;
  }

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

  uint16_t word(uint8_t reg_b) const { return (this->registers[reg_b] << 8) | this->registers[reg_b + 1]; }
};

static SX1509Component *hub = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static void write_led(output::FloatOutput *entity, uint16_t index, float state) { hub->write_led(index, state); }

class SX1509LedTest : public ::testing::Test {
 protected:
  RegisterBus bus_;
  SX1509Component chip_;

  void SetUp() override {
    this->chip_.set_i2c_bus(&this->bus_);
    hub = &this->chip_;
  }
};

TEST_F(SX1509LedTest, SetupDrivesEachRegisteredChannelWithItsBootDuty) {
  this->chip_.register_led_channel(5, 0);
  this->chip_.register_led_channel(3, 255);
  this->chip_.register_led_channel(8, 230);
  this->chip_.setup();

  ASSERT_FALSE(this->chip_.is_failed());
  EXPECT_EQ(this->bus_.registers[REG_I_ON[5]], 0);
  EXPECT_EQ(this->bus_.registers[REG_I_ON[3]], 255);
  EXPECT_EQ(this->bus_.registers[REG_I_ON[8]], 230);
  EXPECT_EQ(this->bus_.registers[REG_I_ON[4]], 0xFF);  // not an LED channel
  constexpr uint16_t led_pins = (1 << 3) | (1 << 5) | (1 << 8);
  EXPECT_EQ(this->bus_.word(REG_LED_DRIVER_ENABLE_B), led_pins);
  EXPECT_EQ(this->bus_.word(REG_DIR_B) & led_pins, 0);
  EXPECT_EQ(this->bus_.word(REG_DATA_B) & led_pins, 0);
}

// The duty FloatOutput::turn_off() writes must match what codegen registers (sx1509/output initial_duty)
TEST_F(SX1509LedTest, TurnOffWritesTheDutyCodegenRegisters) {
  struct Case {
    bool inverted;
    float min_power;
    float max_power;
    bool zero_means_zero;
    uint8_t duty;
  };
  static constexpr std::array<Case, 8> CASES{{
      {false, 0.0f, 1.0f, false, 0},
      {true, 0.0f, 1.0f, false, 255},
      {false, 0.1f, 1.0f, false, 26},
      {true, 0.1f, 1.0f, false, 230},
      {false, 0.1f, 1.0f, true, 0},
      {true, 0.1f, 1.0f, true, 255},
      {false, 0.5f, 0.2f, false, 51},
      {true, 0.002f, 1.0f, false, 254},
  }};
  for (const auto &c : CASES) {
    output::ApplyFloatOutput<write_led> out(7);
    out.set_inverted(c.inverted);
    out.set_max_power(c.max_power);
    out.set_min_power(c.min_power);
    out.set_zero_means_zero(c.zero_means_zero);
    this->bus_.registers[REG_I_ON[7]] = 0x42;
    out.turn_off();
    EXPECT_EQ(this->bus_.registers[REG_I_ON[7]], c.duty) << "inverted " << c.inverted << " min " << c.min_power;
  }
}

TEST_F(SX1509LedTest, LevelsRoundToTheNearestDuty) {
  output::ApplyFloatOutput<write_led> out(2);
  out.set_level(0.5f);
  EXPECT_EQ(this->bus_.registers[REG_I_ON[2]], 128);
  out.set_level(1.0f);
  EXPECT_EQ(this->bus_.registers[REG_I_ON[2]], 255);
  out.set_inverted(true);
  out.set_level(0.25f);
  EXPECT_EQ(this->bus_.registers[REG_I_ON[2]], 191);
}

}  // namespace esphome::sx1509::testing
