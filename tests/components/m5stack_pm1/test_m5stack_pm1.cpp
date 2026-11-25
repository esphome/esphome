#include <gtest/gtest.h>

#include <cstdint>

#include "esphome/components/m5stack_pm1/m5stack_pm1.h"
#include "esphome/core/application.h"

namespace esphome::m5stack_pm1::testing {

namespace {

constexpr uint8_t ADDRESS = 0x6E;

}  // namespace

/// Register file of an M5Stack PM1, plus a way to fail the first transactions.
class FakeBus : public i2c::I2CBus {
 public:
  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                             size_t read_count) override {
    EXPECT_EQ(address, ADDRESS);
    if (this->fail_transactions != 0) {
      if (this->fail_transactions > 0) {
        this->fail_transactions--;
      }
      return i2c::ERROR_TIMEOUT;
    }
    if (write_count == 0) {
      return i2c::ERROR_OK;
    }
    uint8_t reg = write_buffer[0];
    for (size_t i = 1; i < write_count; i++, reg++) {
      this->regs[reg] = write_buffer[i];
    }
    for (size_t i = 0; i < read_count; i++, reg++) {
      read_buffer[i] = this->regs[reg];
    }
    return i2c::ERROR_OK;
  }

  uint8_t regs[0x100]{};
  // Positive: fail that many transactions; zero: never fail.
  int fail_transactions{0};
};

class M5StackPM1Test : public ::testing::Test {
 protected:
  void SetUp() override {
    this->component_.set_i2c_bus(&this->bus_);
    this->component_.set_i2c_address(ADDRESS);
  }

  void load_device_info_() {
    this->bus_.regs[REG_DEVICE_ID] = M5STACK_PM1_DEVICE_ID;
    this->bus_.regs[REG_DEVICE_MODEL] = M5STACK_PM1_DEVICE_MODEL;
  }

  FakeBus bus_;
  M5StackPM1Component component_{false};
  InternalGPIOPin int_pin_;
};

TEST_F(M5StackPM1Test, PinModeInputEnablesPullup) {
  this->component_.pin_mode(0, gpio::FLAG_INPUT | gpio::FLAG_PULLUP);

  EXPECT_EQ(this->bus_.regs[REG_GPIO_MODE], 0x00);
  EXPECT_EQ(this->bus_.regs[REG_GPIO_PU_PD_0], 0x01);
  EXPECT_EQ(this->bus_.regs[REG_GPIO_FUNC_0], 0x00);
}

TEST_F(M5StackPM1Test, PinModeOutputOpenDrainConfiguresDrive) {
  this->component_.pin_mode(3, gpio::FLAG_OUTPUT | gpio::FLAG_OPEN_DRAIN);

  EXPECT_EQ(this->bus_.regs[REG_GPIO_MODE], 0x08);
  EXPECT_EQ(this->bus_.regs[REG_GPIO_DRV], 0x08);
  EXPECT_EQ(this->bus_.regs[REG_GPIO_PU_PD_0], 0x00);
  EXPECT_EQ(this->bus_.regs[REG_GPIO_FUNC_0], 0x00);
}

TEST_F(M5StackPM1Test, DigitalWriteSetsOnlyTheRequestedPin) {
  this->component_.digital_write(3, true);

  EXPECT_EQ(this->bus_.regs[REG_GPIO_OUT], 0x08);

  this->component_.digital_write(3, false);

  EXPECT_EQ(this->bus_.regs[REG_GPIO_OUT], 0x00);
}

TEST_F(M5StackPM1Test, SetupWritesTheWakeConfiguration) {
  this->load_device_info_();
  this->component_.set_wake_config(0x05, 0x01);

  this->bus_.regs[REG_GPIO_WAKE_EN] = 0x18;  // from the last boot

  this->component_.setup();

  EXPECT_FALSE(this->component_.is_failed());
  EXPECT_EQ(this->bus_.regs[REG_GPIO_WAKE_EN], 0x00);  // off until the pins are set up
  App.scheduler.process_to_add();
  App.scheduler.call(millis());
  EXPECT_EQ(this->bus_.regs[REG_GPIO_WAKE_EN], 0x05);
  EXPECT_EQ(this->bus_.regs[REG_GPIO_WAKE_CFG], 0x01);
}

TEST_F(M5StackPM1Test, LoopClearsIrqStatusAndWake) {
  this->component_.set_interrupt_pin(&this->int_pin_);
  this->int_pin_.set_inverted(true);  // INT released
  this->bus_.regs[REG_IRQ_STAT_3] = M5STACK_PM1_IRQ_SYSTEM_WAKEUP;
  this->bus_.regs[REG_WAKE_SRC] = 0x03;

  this->component_.loop();

  EXPECT_EQ(this->bus_.regs[REG_WAKE_SRC], 0x00);
  EXPECT_EQ(this->bus_.regs[REG_IRQ_STAT_1], 0x00);
  EXPECT_EQ(this->bus_.regs[REG_IRQ_STAT_1 + 1], 0x00);
  EXPECT_EQ(this->bus_.regs[REG_IRQ_STAT_3], 0x00);
}

TEST_F(M5StackPM1Test, SetupConfiguresIrqOutput) {
  this->load_device_info_();
  this->bus_.regs[REG_GPIO_WAKE_EN] = 0xFF;
  // GPIO0 and GPIO2 interrupts enabled, the IRQ output GPIO4 masked.
  this->component_.set_interrupt_gpio(4, 0x1A);
  this->component_.set_interrupt_pin(&this->int_pin_);
  this->int_pin_.set_inverted(true);

  this->component_.setup();

  EXPECT_FALSE(this->component_.is_failed());
  EXPECT_EQ(this->bus_.regs[REG_I2C_CFG], 0x00);
  EXPECT_EQ(this->bus_.regs[REG_PWR_CFG], M5STACK_PM1_POWER_CONFIG_LED);
  EXPECT_EQ(this->bus_.regs[REG_GPIO_WAKE_EN], 0x00);
  EXPECT_EQ(this->bus_.regs[REG_IRQ_STAT_1_MASK], 0x1A);
  EXPECT_EQ(this->bus_.regs[REG_IRQ_STAT_1_MASK + 1], M5STACK_PM1_IRQ_POWER_MASK);
  EXPECT_EQ(this->bus_.regs[REG_IRQ_STAT_1_MASK + 2], M5STACK_PM1_IRQ_SYSTEM_MASK);
  EXPECT_EQ(this->bus_.regs[REG_GPIO_DRV] & (1 << 4), 1 << 4);
  EXPECT_EQ(this->bus_.regs[REG_GPIO_FUNC_1] & 0b11,
            static_cast<uint8_t>(M5StackPM1PinFunction::M5STACK_PM1_PIN_FUNCTION_IRQ));
}

TEST_F(M5StackPM1Test, FailedComponentDoesNotWriteOutputsOrPinMode) {
  this->component_.mark_failed();

  this->component_.digital_write(3, true);
  this->component_.pin_mode(0, gpio::FLAG_INPUT | gpio::FLAG_PULLUP);

  EXPECT_EQ(this->bus_.regs[REG_GPIO_OUT], 0x00);
  EXPECT_EQ(this->bus_.regs[REG_GPIO_MODE], 0x00);
  EXPECT_EQ(this->bus_.regs[REG_GPIO_PU_PD_0], 0x00);
}

TEST_F(M5StackPM1Test, MaskedInputReadsHardwareEveryTime) {
  this->bus_.regs[REG_GPIO_IN] = 0x00;

  EXPECT_FALSE(this->component_.digital_read(0));

  this->bus_.regs[REG_GPIO_IN] = 0x01;

  EXPECT_TRUE(this->component_.digital_read(0));
}

TEST_F(M5StackPM1Test, InterruptInputUsesTheCacheUntilAnIrq) {
  this->component_.set_interrupt_gpio(4, 0x1D);
  this->bus_.regs[REG_GPIO_IN] = 0x02;

  EXPECT_TRUE(this->component_.digital_read(1));

  this->bus_.regs[REG_GPIO_IN] = 0x00;

  EXPECT_TRUE(this->component_.digital_read(1));
}

TEST_F(M5StackPM1Test, FailedPinModeKeepsTheWarningUntilReapplied) {
  this->bus_.fail_transactions = 1;
  this->component_.pin_mode(0, gpio::FLAG_INPUT | gpio::FLAG_PULLUP);

  EXPECT_TRUE(this->component_.status_has_warning());
  EXPECT_EQ(this->bus_.regs[REG_GPIO_PU_PD_0], 0x00);

  // Another pin succeeding does not hide the pending one.
  this->component_.pin_mode(2, gpio::FLAG_INPUT);

  EXPECT_TRUE(this->component_.status_has_warning());

  this->component_.pin_mode(0, gpio::FLAG_INPUT | gpio::FLAG_PULLUP);

  EXPECT_FALSE(this->component_.status_has_warning());
  EXPECT_EQ(this->bus_.regs[REG_GPIO_PU_PD_0], 0x01);
}

TEST_F(M5StackPM1Test, SetupRetriesTheFirstWrite) {
  this->load_device_info_();
  this->bus_.fail_transactions = 1;

  this->component_.setup();

  EXPECT_FALSE(this->component_.is_failed());
  EXPECT_EQ(this->bus_.regs[REG_I2C_CFG], 0x00);
}

TEST_F(M5StackPM1Test, FastModeSelectsThe400kMode) {
  this->load_device_info_();
  M5StackPM1Component fast_mode{true};
  fast_mode.set_i2c_bus(&this->bus_);
  fast_mode.set_i2c_address(ADDRESS);

  fast_mode.setup();

  EXPECT_FALSE(fast_mode.is_failed());
  EXPECT_EQ(this->bus_.regs[REG_I2C_CFG], M5STACK_PM1_I2C_CFG_SPD);
}

TEST_F(M5StackPM1Test, SetupFailsOnAnUnexpectedDeviceId) {
  this->bus_.regs[REG_DEVICE_ID] = 0x00;
  this->bus_.regs[REG_DEVICE_MODEL] = M5STACK_PM1_DEVICE_MODEL;

  this->component_.setup();

  EXPECT_TRUE(this->component_.is_failed());
  // An unrecognised device does not get its rails or status LED enabled.
  EXPECT_EQ(this->bus_.regs[REG_PWR_CFG], 0x00);
}

TEST_F(M5StackPM1Test, PinConfigurationRetriesFromTheInterval) {
  this->bus_.fail_transactions = 1;
  this->component_.pin_mode(0, gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  EXPECT_EQ(this->bus_.regs[REG_GPIO_PU_PD_0], 0x00);

  // The retry runs from the component's 1 s interval.
  delay(1050);  // NOLINT
  App.scheduler.call(millis());

  EXPECT_EQ(this->bus_.regs[REG_GPIO_PU_PD_0], 0x01);
  EXPECT_FALSE(this->component_.status_has_warning());
}

}  // namespace esphome::m5stack_pm1::testing
