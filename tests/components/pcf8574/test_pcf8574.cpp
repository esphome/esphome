#include <gtest/gtest.h>

#include "esphome/components/pcf8574/pcf8574.h"

namespace esphome::pcf8574::testing {

// Model quasi-bidirectional readback separately from the output latch.
class PortBus : public i2c::I2CBus {
 public:
  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                             size_t read_count) override {
    EXPECT_EQ(address, 0x21);
    if (read_count != 0) {
      EXPECT_EQ(read_count, this->width);
      this->reads++;
      if (this->fail_read)
        return i2c::ERROR_TIMEOUT;
      uint16_t levels = this->latch & ~this->external_low;
      for (size_t i = 0; i < read_count; i++)
        read_buffer[i] = levels >> (8 * i);
    }
    if (write_count != 0) {
      EXPECT_EQ(write_count, this->width);
      this->writes++;
      this->latch = write_buffer[0];
      if (write_count == 2)
        this->latch |= uint16_t(write_buffer[1]) << 8;
    }
    return i2c::ERROR_OK;
  }

  size_t width{1};
  uint16_t latch{0};
  uint16_t external_low{0};
  unsigned reads{0};
  bool fail_read{false};
  unsigned writes{0};
};

class PCF8574ResetTest : public ::testing::TestWithParam<bool> {
 protected:
  void SetUp() override {
    this->bus_.width = GetParam() ? 2 : 1;
    this->mask_ = GetParam() ? 0xFFFF : 0xFF;
    this->high_pin_ = GetParam() ? 15 : 7;
    this->bus_.latch = 0xA55A & this->mask_;
    this->component_.set_i2c_bus(&this->bus_);
    this->component_.set_i2c_address(0x21);
    this->component_.set_pcf8575(GetParam());
  }

  void configure_input_(uint8_t number) {
    // Codegen registers input pins with the parent before setup()
    this->component_.register_input_pin(number);
    EXPECT_EQ(this->bus_.writes, 0);
    EXPECT_EQ(this->bus_.reads, 0);
  }

  PortBus bus_;
  PCF8574Component component_;
  uint16_t mask_;
  uint8_t high_pin_;
};

TEST_P(PCF8574ResetTest, DefaultResetRetainsStartupAndPinModeBehavior) {
  this->configure_input_(1);

  this->component_.setup();
  ASSERT_EQ(this->bus_.writes, 1);
  EXPECT_EQ(this->bus_.latch, this->mask_);
  EXPECT_EQ(this->bus_.reads, 2);

  this->component_.pin_mode(0, gpio::FLAG_OUTPUT);
  EXPECT_EQ(this->bus_.writes, 1);

  this->component_.digital_write(0, false);
  EXPECT_EQ(this->bus_.latch, this->mask_ & ~1);
}

TEST_P(PCF8574ResetTest, PreservesOtherOutputsBeforeAndAfterTheirPinSetup) {
  this->component_.set_reset(false);
  const uint16_t initial = this->bus_.latch;

  this->component_.setup();
  EXPECT_EQ(this->bus_.writes, 0);
  EXPECT_EQ(this->bus_.reads, 1);
  EXPECT_EQ(this->bus_.latch, initial);

  // Consumers can write before calling their GPIO pin's setup().
  this->component_.digital_write(0, true);
  EXPECT_EQ(this->bus_.latch, initial | 1);

  this->component_.pin_mode(0, gpio::FLAG_OUTPUT);
  this->component_.pin_mode(this->high_pin_, gpio::FLAG_OUTPUT);
  const uint16_t high_pin_mask = 1 << this->high_pin_;
  this->component_.digital_write(this->high_pin_, (initial & high_pin_mask) == 0);
  const uint16_t expected = (initial | 1) ^ high_pin_mask;
  EXPECT_EQ(this->bus_.latch, expected);

  this->component_.digital_write(0, false);
  EXPECT_EQ(this->bus_.latch, expected & ~1);
  ASSERT_EQ(this->bus_.writes, 3);
}

TEST_P(PCF8574ResetTest, MixedInputsAreReleasedBeforeTheirConsumersInitialize) {
  const uint16_t inputs = (1 << 1) | (1 << this->high_pin_);
  this->bus_.latch |= inputs;
  const uint16_t initial = this->bus_.latch;
  this->bus_.external_low = inputs;

  this->component_.set_reset(false);
  this->configure_input_(1);
  this->configure_input_(this->high_pin_);
  this->component_.setup();
  EXPECT_EQ(this->bus_.writes, 0);

  this->component_.digital_write(0, true);
  EXPECT_EQ(this->bus_.latch, initial | 1);

  this->component_.pin_mode(1, gpio::FLAG_INPUT);
  this->component_.pin_mode(this->high_pin_, gpio::FLAG_INPUT);
  EXPECT_EQ(this->bus_.latch, initial | 1);
  EXPECT_FALSE(this->component_.digital_read(1));
}

TEST_P(PCF8574ResetTest, InputSetupBeforeOutputSetupPreservesOutputs) {
  this->component_.set_reset(false);
  this->configure_input_(1);
  const uint16_t initial = this->bus_.latch;

  this->component_.setup();
  this->component_.pin_mode(1, gpio::FLAG_INPUT);
  EXPECT_EQ(this->bus_.latch, initial);

  this->component_.pin_mode(0, gpio::FLAG_OUTPUT);
  this->component_.digital_write(0, true);
  EXPECT_EQ(this->bus_.latch, initial | 1);
}

TEST_P(PCF8574ResetTest, LoadedHighOutputIsLatchedLowOnFirstWrite) {
  const uint16_t loaded_pin = 1 << this->high_pin_;
  this->bus_.latch |= loaded_pin;
  this->bus_.external_low = loaded_pin;
  const uint16_t initial = this->bus_.latch;
  this->component_.set_reset(false);

  this->component_.setup();
  this->component_.pin_mode(this->high_pin_, gpio::FLAG_OUTPUT);
  EXPECT_EQ(this->bus_.writes, 0);
  EXPECT_EQ(this->bus_.latch, initial);

  this->component_.digital_write(0, true);
  EXPECT_EQ(this->bus_.latch, (initial | 1) & ~loaded_pin);
}

TEST_P(PCF8574ResetTest, UndeclaredLowPinRemainsDrivenAfterExternalRelease) {
  const uint16_t undeclared_pin = 1 << this->high_pin_;
  this->bus_.latch |= undeclared_pin;
  this->bus_.external_low = undeclared_pin;
  const uint16_t initial = this->bus_.latch;
  this->component_.set_reset(false);

  this->component_.setup();
  EXPECT_EQ(this->bus_.writes, 0);
  EXPECT_EQ(this->bus_.latch, initial);

  this->component_.digital_write(0, true);
  this->bus_.external_low = 0;
  EXPECT_EQ(this->bus_.latch, (initial | 1) & ~undeclared_pin);
  EXPECT_FALSE(this->component_.digital_read(this->high_pin_));
}

TEST_P(PCF8574ResetTest, InitialReadFailurePreventsAllWrites) {
  this->bus_.fail_read = true;

  this->component_.set_reset(false);
  this->component_.setup();
  EXPECT_TRUE(this->component_.is_failed());

  this->component_.pin_mode(1, gpio::FLAG_INPUT);
  this->component_.digital_write(0, true);
  EXPECT_EQ(this->bus_.writes, 0);
}

INSTANTIATE_TEST_SUITE_P(PCF8574AndPCF8575, PCF8574ResetTest, ::testing::Bool());

}  // namespace esphome::pcf8574::testing
