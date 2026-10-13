#include <gtest/gtest.h>

#include "esphome/components/pca9505/pca9505.h"

namespace esphome::pca9505::testing {

// Models the PCA9505 register map: five ports, each with its own input, output,
// polarity inversion and configuration byte. The command byte selects a register
// and its top bit asks the part to auto-increment through the block.
class FakeBus : public i2c::I2CBus {
 public:
  static constexpr uint8_t ADDRESS = 0x26;

  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                             size_t read_count) override {
    EXPECT_EQ(address, ADDRESS);
    if (this->fail)
      return i2c::ERROR_TIMEOUT;

    // Every transfer starts with the command byte.
    EXPECT_GE(write_count, 1u);
    const uint8_t command = write_buffer[0];
    const bool auto_increment = (command & 0x80) != 0;
    uint8_t *block = this->block_for(command & 0x7F);
    const uint8_t index = (command & 0x7F) % 8;

    if (read_count != 0) {
      this->reads++;
      // Without auto-increment the part keeps returning the same register.
      EXPECT_TRUE(auto_increment || read_count == 1);
      for (size_t i = 0; i < read_count; i++)
        read_buffer[i] = block[auto_increment ? index + i : index];
      return i2c::ERROR_OK;
    }

    this->writes++;
    EXPECT_TRUE(auto_increment || write_count == 2);
    for (size_t i = 1; i < write_count; i++)
      block[auto_increment ? index + i - 1 : index] = write_buffer[i];
    return i2c::ERROR_OK;
  }

  uint8_t *block_for(uint8_t reg) {
    if (reg < 0x08)
      return this->input;
    if (reg < 0x10)
      return this->output;
    if (reg < 0x18)
      return this->invert;
    if (reg < 0x20)
      return this->config;
    return this->mask;
  }

  // Power-on reset defaults from the datasheet.
  uint8_t input[BANK_COUNT]{0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  uint8_t output[BANK_COUNT]{0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  uint8_t invert[BANK_COUNT]{0x00, 0x00, 0x00, 0x00, 0x00};
  uint8_t config[BANK_COUNT]{0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  uint8_t mask[BANK_COUNT]{0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  unsigned reads{0};
  unsigned writes{0};
  bool fail{false};
};

class PCA9505Test : public ::testing::Test {
 protected:
  void SetUp() override {
    this->component_.set_i2c_bus(&this->bus_);
    this->component_.set_i2c_address(FakeBus::ADDRESS);
  }

  PCA9505GPIOPin make_pin_(uint8_t number, gpio::Flags flags, bool inverted = false) {
    PCA9505GPIOPin pin;
    pin.set_parent(&this->component_);
    pin.set_pin(number);
    pin.set_flags(flags);
    pin.set_inverted(inverted);
    return pin;
  }

  FakeBus bus_;
  PCA9505Component component_;
  InternalGPIOPin int_pin_;  // digital_read() returns the inverted flag on the host, so it doubles as the level
};

TEST_F(PCA9505Test, SetupLeavesEveryPinAnInputWithOutputsLow) {
  this->component_.setup();

  ASSERT_FALSE(this->component_.is_failed());
  for (uint8_t bank = 0; bank < BANK_COUNT; bank++) {
    EXPECT_EQ(this->bus_.config[bank], 0xFF) << "bank " << int(bank) << " should still be an input";
    EXPECT_EQ(this->bus_.output[bank], 0x00) << "bank " << int(bank) << " should be driven low";
    EXPECT_EQ(this->bus_.invert[bank], 0x00) << "bank " << int(bank) << " should not be inverted";
  }
}

TEST_F(PCA9505Test, SetupFailsWhenTheDeviceIsAbsent) {
  this->bus_.fail = true;
  this->component_.setup();

  EXPECT_TRUE(this->component_.is_failed());
}

// The chip has 40 pins, so pin numbers reach past the width of an int shift.
// Every pin must land in its own bank and bit.
TEST_F(PCA9505Test, EachPinMapsToItsOwnBankAndBit) {
  this->component_.setup();

  for (uint8_t number = 0; number < PIN_COUNT; number++) {
    PCA9505GPIOPin pin = this->make_pin_(number, gpio::FLAG_OUTPUT);
    pin.setup();
    pin.digital_write(true);

    for (uint8_t bank = 0; bank < BANK_COUNT; bank++) {
      const uint8_t want = (bank == number / 8) ? uint8_t(1 << (number % 8)) : 0x00;
      ASSERT_EQ(this->bus_.output[bank], want) << "pin " << int(number) << " wrote outside its own bit";
    }

    pin.digital_write(false);
    ASSERT_EQ(this->bus_.output[number / 8], 0x00) << "pin " << int(number) << " did not clear";
  }
}

TEST_F(PCA9505Test, ConfiguringAnOutputOnlyTouchesItsOwnBank) {
  this->component_.setup();

  PCA9505GPIOPin pin = this->make_pin_(35, gpio::FLAG_OUTPUT);
  pin.setup();

  // Pin 35 is bit 3 of bank 4. The part reads a 1 as an input, so that bit clears.
  EXPECT_EQ(this->bus_.config[4], 0xFF & ~(1 << 3));
  for (uint8_t bank = 0; bank < 4; bank++)
    EXPECT_EQ(this->bus_.config[bank], 0xFF) << "bank " << int(bank) << " should be untouched";
}

TEST_F(PCA9505Test, ReadsReportTheLevelOnTheHighestPin) {
  this->component_.setup();
  PCA9505GPIOPin pin = this->make_pin_(39, gpio::FLAG_INPUT);
  pin.setup();

  this->bus_.input[4] = 0x80;  // pin 39 high
  EXPECT_TRUE(pin.digital_read());

  this->component_.loop();  // clears the cache so the next read hits the bus
  this->bus_.input[4] = 0x00;
  EXPECT_FALSE(pin.digital_read());
}

TEST_F(PCA9505Test, InvertedPinFlipsTheReportedLevel) {
  this->component_.setup();
  PCA9505GPIOPin pin = this->make_pin_(39, gpio::FLAG_INPUT, /*inverted=*/true);
  pin.setup();

  this->bus_.input[4] = 0x80;
  EXPECT_FALSE(pin.digital_read());

  this->component_.loop();
  this->bus_.input[4] = 0x00;
  EXPECT_TRUE(pin.digital_read());
}

// Eight pins sharing a port must cost one transfer per loop, not eight.
TEST_F(PCA9505Test, PinsInOneBankShareASingleRead) {
  this->component_.setup();

  PCA9505GPIOPin pins[8];
  for (uint8_t i = 0; i < 8; i++) {
    pins[i] = this->make_pin_(8 + i, gpio::FLAG_INPUT);
    pins[i].setup();
  }

  this->bus_.input[1] = 0xA5;
  const unsigned before = this->bus_.reads;
  for (uint8_t i = 0; i < 8; i++)
    EXPECT_EQ(pins[i].digital_read(), ((0xA5 >> i) & 1) != 0) << "pin " << int(8 + i);
  EXPECT_EQ(this->bus_.reads - before, 1u);
}

// Pins in different ports are cached separately, so each port costs its own read.
TEST_F(PCA9505Test, EachBankIsReadSeparately) {
  this->component_.setup();

  PCA9505GPIOPin pins[BANK_COUNT];
  for (uint8_t bank = 0; bank < BANK_COUNT; bank++) {
    pins[bank] = this->make_pin_(bank * 8, gpio::FLAG_INPUT);
    pins[bank].setup();
  }

  const unsigned before = this->bus_.reads;
  for (auto &pin : pins)
    pin.digital_read();
  EXPECT_EQ(this->bus_.reads - before, BANK_COUNT);
}

TEST_F(PCA9505Test, WritesAreSkippedOnceSetupHasFailed) {
  this->bus_.fail = true;
  this->component_.setup();
  ASSERT_TRUE(this->component_.is_failed());

  this->bus_.fail = false;
  const unsigned before = this->bus_.writes;
  PCA9505GPIOPin pin = this->make_pin_(0, gpio::FLAG_OUTPUT);
  pin.setup();
  pin.digital_write(true);
  EXPECT_EQ(this->bus_.writes, before);
}

// A 1 in a mask register blocks that pin from pulling INT low.
TEST_F(PCA9505Test, SetupMasksEveryInterrupt) {
  // Pretend a previous run left port 2 unmasked. The chip keeps its registers
  // through an MCU reboot, so setup must write them rather than assume the reset state.
  this->bus_.mask[2] = 0x00;
  this->component_.setup();

  for (uint8_t bank = 0; bank < BANK_COUNT; bank++)
    EXPECT_EQ(this->bus_.mask[bank], 0xFF) << "bank " << int(bank) << " should be masked";
}

TEST_F(PCA9505Test, ConfiguringAnInputUnmasksOnlyThatPin) {
  this->component_.set_interrupt_pin(&this->int_pin_);
  this->component_.setup();

  PCA9505GPIOPin pin = this->make_pin_(34, gpio::FLAG_INPUT);
  pin.setup();

  // Pin 34 is bit 2 of bank 4, and a 0 lets it through.
  EXPECT_EQ(this->bus_.mask[4], 0xFF & ~(1 << 2));
  for (uint8_t bank = 0; bank < 4; bank++)
    EXPECT_EQ(this->bus_.mask[bank], 0xFF) << "bank " << int(bank) << " should be untouched";
}

TEST_F(PCA9505Test, ConfiguringAnOutputLeavesItMasked) {
  this->component_.set_interrupt_pin(&this->int_pin_);
  this->component_.setup();

  PCA9505GPIOPin pin = this->make_pin_(34, gpio::FLAG_OUTPUT);
  pin.setup();

  // An output must not be able to pull INT low.
  EXPECT_EQ(this->bus_.mask[4], 0xFF);
}

TEST_F(PCA9505Test, WithoutAnInterruptPinTheMasksAreNotRewritten) {
  this->component_.setup();
  const unsigned before = this->bus_.writes;

  PCA9505GPIOPin pin = this->make_pin_(34, gpio::FLAG_INPUT);
  pin.setup();

  EXPECT_EQ(this->bus_.writes, before + 1);  // the configuration register only
  EXPECT_EQ(this->bus_.mask[4], 0xFF);
}

// Without an interrupt pin an input turns the polling loop on; with one it stays off.
TEST_F(PCA9505Test, AnInputEnablesPollingOnlyWithoutAnInterruptPin) {
  this->component_.setup();
  ASSERT_TRUE(this->component_.is_idle());
  PCA9505GPIOPin polled = this->make_pin_(3, gpio::FLAG_INPUT);
  polled.setup();
  EXPECT_TRUE(this->component_.is_in_loop_state());

  PCA9505Component with_interrupt;
  with_interrupt.set_i2c_bus(&this->bus_);
  with_interrupt.set_i2c_address(FakeBus::ADDRESS);
  with_interrupt.set_interrupt_pin(&this->int_pin_);
  with_interrupt.setup();
  ASSERT_TRUE(with_interrupt.is_idle());
  PCA9505GPIOPin pin;
  pin.set_parent(&with_interrupt);
  pin.set_pin(3);
  pin.set_flags(gpio::FLAG_INPUT);
  pin.setup();
  EXPECT_TRUE(with_interrupt.is_idle());
}

// With an interrupt pin a read is served from the cache until loop() clears it.
TEST_F(PCA9505Test, InterruptPinKeepsTheCacheUntilTheLoopRuns) {
  this->component_.set_interrupt_pin(&this->int_pin_);
  this->component_.setup();
  PCA9505GPIOPin pin = this->make_pin_(3, gpio::FLAG_INPUT);
  pin.setup();

  const unsigned before = this->bus_.reads;
  pin.digital_read();
  pin.digital_read();
  EXPECT_EQ(this->bus_.reads, before + 1);

  this->component_.loop();
  pin.digital_read();
  EXPECT_EQ(this->bus_.reads, before + 2);
}

// The loop only turns itself off once INT has gone high again, so a change that
// lands during the read cannot leave the cache stale with no edge to wake it.
TEST_F(PCA9505Test, LoopStaysOnUntilIntIsReleased) {
  this->component_.set_interrupt_pin(&this->int_pin_);
  this->component_.setup();
  this->component_.enable_loop();
  ASSERT_TRUE(this->component_.is_in_loop_state());

  this->int_pin_.set_inverted(false);  // the host pin reads back its inverted flag: INT still low
  this->component_.loop();
  EXPECT_TRUE(this->component_.is_in_loop_state());

  this->int_pin_.set_inverted(true);  // INT released
  this->component_.loop();
  EXPECT_TRUE(this->component_.is_idle());
}

}  // namespace esphome::pca9505::testing
