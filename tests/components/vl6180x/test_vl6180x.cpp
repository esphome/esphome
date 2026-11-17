#include <gtest/gtest.h>

#include <array>
#include <cmath>

#include "esphome/components/vl6180x/vl6180x.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::vl6180x::testing {

// The sensor's 16-bit register map; writes and reads advance the register pointer.
class RegisterBus final : public i2c::I2CBus {
 public:
  i2c::ErrorCode write_readv(uint8_t /*address*/, const uint8_t *data, size_t size, uint8_t *out,
                             size_t out_size) override {
    uint16_t pointer = encode_uint16(data[0], data[1]);
    for (size_t i = 2; i < size; i++)
      this->registers[pointer++] = data[i];
    for (size_t i = 0; i < out_size; i++)
      out[i] = this->registers[pointer++];
    return i2c::ERROR_OK;
  }

  std::array<uint8_t, 0x213> registers{};
};

class VL6180XTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->hub_.set_i2c_bus(&this->bus_);
    this->hub_.set_i2c_address(0x29);
    this->hub_.set_distance_sensor(&this->range_);
    this->hub_.set_illuminance_sensor(&this->lux_);
    // 1x gain over 1 ms: 32 lux per count
    this->hub_.set_als(6, 1, 32.0f);
    this->bus_.registers[0x000] = 0xB4;  // model ID
    this->bus_.registers[0x016] = 0x01;  // fresh out of reset
  }

  // One update: range, then the ALS once its integration time and readout margin have passed.
  void measure_() {
    this->hub_.update();
    delay(80);  // NOLINT
    App.scheduler.call(millis());
    delay(420);  // NOLINT
    App.scheduler.call(millis());
  }

  RegisterBus bus_;
  VL6180XComponent hub_;
  sensor::Sensor range_;
  sensor::Sensor lux_;
};

TEST_F(VL6180XTest, FreshSensorGetsTheMandatorySettingsAndTheScaler) {
  this->bus_.registers[0x024] = 0xFA;  // factory part-to-part offset, -6 mm
  this->hub_.set_scaler(2);
  this->hub_.setup();
  ASSERT_FALSE(this->hub_.is_failed());
  EXPECT_EQ(this->bus_.registers[0x0207], 0x01);
  EXPECT_EQ(this->bus_.registers[0x016], 0x00);
  EXPECT_EQ(encode_uint16(this->bus_.registers[0x096], this->bus_.registers[0x097]), 127);
  EXPECT_EQ(this->bus_.registers[0x024], 0xFD);  // -3 mm at 2x
  EXPECT_EQ(this->bus_.registers[0x02D], 0x10);
}

TEST_F(VL6180XTest, SensorThatKeptPowerRestoresTheOffsetBeforeRescaling) {
  this->bus_.registers[0x016] = 0x00;
  this->bus_.registers[0x024] = 0xFE;  // -2 mm, written at 3x by the last setup
  this->bus_.registers[0x096] = 0x00;
  this->bus_.registers[0x097] = 84;
  this->hub_.set_scaler(2);
  this->hub_.setup();
  EXPECT_EQ(this->bus_.registers[0x0207], 0x00);  // the one-time settings are not written again
  EXPECT_EQ(this->bus_.registers[0x024], 0xFD);   // -6 mm / 2
}

TEST_F(VL6180XTest, WrongModelIdFailsSetup) {
  this->bus_.registers[0x000] = 0x00;
  this->hub_.setup();
  EXPECT_TRUE(this->hub_.is_failed());
}

TEST_F(VL6180XTest, PublishesScaledRangeAndLux) {
  this->hub_.set_scaler(2);
  this->hub_.setup();
  this->bus_.registers[0x04D] = 0x01;  // no range error
  this->bus_.registers[0x04F] = 0x24;  // range and ALS samples ready
  this->bus_.registers[0x062] = 100;
  this->bus_.registers[0x050] = 0x00;
  this->bus_.registers[0x051] = 10;
  this->measure_();
  EXPECT_FLOAT_EQ(this->range_.state, 200.0f);
  EXPECT_FLOAT_EQ(this->lux_.state, 320.0f);
  EXPECT_FALSE(this->hub_.status_has_warning());
}

TEST_F(VL6180XTest, NoTargetAndAlsOverflowPublishNanWithoutAWarning) {
  this->hub_.setup();
  this->bus_.registers[0x04D] = 0xB1;  // range error 11, no target
  this->bus_.registers[0x04E] = 0x31;  // ALS error 3, overflow in bright light
  this->bus_.registers[0x04F] = 0x24;
  this->measure_();
  EXPECT_TRUE(std::isnan(this->range_.state));
  EXPECT_TRUE(std::isnan(this->lux_.state));
  EXPECT_FALSE(this->hub_.status_has_warning());
}

TEST_F(VL6180XTest, SampleNotReadySetsAWarning) {
  this->hub_.setup();
  this->bus_.registers[0x04F] = 0x00;
  this->hub_.update();
  delay(80);  // NOLINT
  App.scheduler.call(millis());
  EXPECT_TRUE(this->hub_.status_has_warning());
  EXPECT_FALSE(this->range_.has_state());
}

class FakePin final : public GPIOPin {
 public:
  void setup() override {}
  void pin_mode(gpio::Flags /*flags*/) override {}
  gpio::Flags get_flags() const override { return gpio::FLAG_OUTPUT; }
  bool digital_read() override { return this->level; }
  void digital_write(bool value) override { this->level = value; }
  bool level{true};
};

// Two chips that answer at their own address only while their enable pin is high.
class TwoChipBus final : public i2c::I2CBus {
 public:
  struct Chip {
    FakePin *pin;
    uint8_t address{0x29};
    uint8_t model_id{0xB4};
  };

  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                             size_t out_size) override {
    bool acked = false;
    for (auto &chip : this->chips) {
      if (!chip.pin->level || chip.address != address)
        continue;
      const uint16_t reg = encode_uint16(data[0], data[1]);
      if (out_size > 0)
        out[0] = reg == 0x000 ? chip.model_id : 0x01;  // the fresh-out-of-reset flag reads 1
      if (reg == 0x212 && size == 3)
        chip.address = data[2];
      acked = true;
    }
    return acked ? i2c::ERROR_OK : i2c::ERROR_NOT_ACKNOWLEDGED;
  }

  FakePin pin_a;
  FakePin pin_b;
  std::array<Chip, 2> chips{Chip{&pin_a}, Chip{&pin_b}};
};

class VL6180XEnablePinTest : public ::testing::Test {
 protected:
  void SetUp() override {
    uint8_t address = 0x30;
    for (auto *hub : {&this->hub_a_, &this->hub_b_}) {
      hub->set_i2c_bus(&this->bus_);
      hub->set_i2c_address(address++);
    }
    this->hub_a_.set_enable_pin(&this->bus_.pin_a);
    this->hub_b_.set_enable_pin(&this->bus_.pin_b);
    VL6180XComponent::register_enable_pin(&this->bus_.pin_a);
    VL6180XComponent::register_enable_pin(&this->bus_.pin_b);
  }

  TwoChipBus bus_;
  VL6180XComponent hub_a_;
  VL6180XComponent hub_b_;
};

TEST_F(VL6180XEnablePinTest, SensorsAreMovedOffTheDefaultAddressOneAtATime) {
  this->hub_a_.setup();
  EXPECT_FALSE(this->bus_.pin_b.level);  // still held in reset while the first one moves
  this->hub_b_.setup();
  EXPECT_FALSE(this->hub_a_.is_failed());
  EXPECT_FALSE(this->hub_b_.is_failed());
  EXPECT_EQ(this->bus_.chips[0].address, 0x30);
  EXPECT_EQ(this->bus_.chips[1].address, 0x31);
}

TEST_F(VL6180XEnablePinTest, FailedSensorIsHeldInResetSoTheNextOneIsAlone) {
  this->bus_.chips[0].model_id = 0x00;
  this->hub_a_.setup();
  EXPECT_TRUE(this->hub_a_.is_failed());
  EXPECT_FALSE(this->bus_.pin_a.level);
  this->hub_b_.setup();
  EXPECT_FALSE(this->hub_b_.is_failed());
  EXPECT_EQ(this->bus_.chips[0].address, 0x29);
  EXPECT_EQ(this->bus_.chips[1].address, 0x31);
}

}  // namespace esphome::vl6180x::testing
