#ifdef USE_HOST

#include <gtest/gtest.h>

#include <array>
#include "esphome/components/tmp102/tmp102.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::tmp102::testing {

class RegisterBus final : public i2c::I2CBus {
 public:
  std::array<uint16_t, 4> registers{0x1900, 0x60A0, 0x4B00, 0x5000};
  int fail_read{-1};
  int fail_write{-1};
  int fail_select{-1};
  unsigned transactions{0};
  unsigned writes{0};
  uint8_t pointer{0};

  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                             size_t out_size) override {
    ++this->transactions;
    if (size != 0) {
      if (data[0] >= this->registers.size() || data[0] == this->fail_select)
        return i2c::ERROR_NOT_ACKNOWLEDGED;
      this->pointer = data[0];
      if (size == 3) {
        if (this->pointer == this->fail_write)
          return i2c::ERROR_NOT_ACKNOWLEDGED;
        ++this->writes;
        this->registers[this->pointer] = (uint16_t(data[1]) << 8) | data[2];
      }
    }
    if (out_size != 0) {
      if (out_size != 2 || this->pointer == this->fail_read)
        return i2c::ERROR_NOT_ACKNOWLEDGED;
      out[0] = this->registers[this->pointer] >> 8;
      out[1] = this->registers[this->pointer];
    }
    return i2c::ERROR_OK;
  }
};

class TMP102Test : public ::testing::Test {
 protected:
  RegisterBus bus_;
  TMP102Component chip_;
  unsigned publications_{0};

  void SetUp() override {
    new (&App) Application;
    char name[] = "tmp102-regression-tests";
    char friendly_name[] = "TMP102 tests";
    App.pre_setup(name, 23, friendly_name, 12);
    this->chip_.set_i2c_bus(&this->bus_);
    this->chip_.set_i2c_address(0x48);
    this->chip_.add_on_state_callback([this](float) { ++this->publications_; });
  }

  void TearDown() override {
    App.scheduler.cancel_timeout(&this->chip_, "read_temp");
    App.scheduler.call(millis());
    App.~Application();
  }

  static void run_timers(uint32_t duration) {
    App.scheduler.call(millis());
    delay(duration);
    App.scheduler.call(millis());
  }
};

TEST_F(TMP102Test, LegacySetupAndContinuousReadDoNotWriteRegisters) {
  const auto original = this->bus_.registers;
  this->chip_.setup();
  EXPECT_EQ(this->bus_.transactions, 0);
  this->chip_.update();
  EXPECT_FLOAT_EQ(this->chip_.state, 25);
  EXPECT_EQ(this->publications_, 1);
  EXPECT_EQ(this->bus_.writes, 0);
  EXPECT_EQ(this->bus_.registers, original);
}

TEST_F(TMP102Test, SampleFormatOverridesConfiguredModeInBothDirections) {
  for (bool configured_extended : {false, true}) {
    this->chip_.set_configure(true);
    this->chip_.set_extended_mode(configured_extended);
    this->chip_.set_conversion_rate(TMP102_CONVERSION_RATE_0_25HZ);
    this->chip_.setup();
    const auto writes = this->bus_.writes;
    for (auto raw : {0x1900, 0x0C81, 0xFF00, 0xFF81}) {
      this->bus_.registers[0] = raw;
      this->chip_.update();
      EXPECT_FLOAT_EQ(this->chip_.state, raw & 0x8000 ? -1 : 25);
      EXPECT_FALSE(this->chip_.status_has_warning());
      EXPECT_EQ(this->bus_.writes, writes);
    }
  }
}

TEST_F(TMP102Test, WritableConfigurationFields) {
  for (auto rate : {TMP102_CONVERSION_RATE_0_25HZ, TMP102_CONVERSION_RATE_1HZ, TMP102_CONVERSION_RATE_4HZ,
                    TMP102_CONVERSION_RATE_8HZ}) {
    for (uint8_t faults : {1, 2, 4, 6}) {
      for (bool enabled : {false, true}) {
        this->chip_.set_configure(true);
        this->chip_.set_conversion_rate(rate);
        this->chip_.set_fault_queue(faults);
        this->chip_.set_extended_mode(enabled);
        this->chip_.set_one_shot_mode(enabled);
        this->chip_.set_alert_polarity(enabled ? TMP102_ALERT_POLARITY_ACTIVE_HIGH : TMP102_ALERT_POLARITY_ACTIVE_LOW);
        this->chip_.set_thermostat_mode(enabled ? TMP102_THERMOSTAT_MODE_INTERRUPT : TMP102_THERMOSTAT_MODE_COMPARATOR);
        this->chip_.setup();
        unsigned fault_bits = faults == 1 ? 0 : faults == 2 ? 1 : faults == 4 ? 2 : 3;
        EXPECT_EQ(this->bus_.registers[1], (fault_bits << 11) | (unsigned(rate) << 6) | (enabled ? 0x710 : 0));
        EXPECT_EQ(this->bus_.registers[3], enabled ? 0x2800 : 0x5000);
        EXPECT_EQ(this->bus_.registers[2], enabled ? 0x2580 : 0x4B00);
      }
    }
  }
}

TEST_F(TMP102Test, StaticThresholdsAreQuantizedAndEncoded) {
  this->chip_.set_configure(true);
  this->chip_.set_temperature_high(12.22f);
  this->chip_.set_temperature_low(-12.22f);
  this->chip_.setup();
  EXPECT_FLOAT_EQ(this->chip_.get_limit_temperature(TMP102_LIMIT_HIGH), 12.25f);
  EXPECT_FLOAT_EQ(this->chip_.get_limit_temperature(TMP102_LIMIT_LOW), -12.25f);
  EXPECT_EQ(this->bus_.registers[3], 0x0C40);
  EXPECT_EQ(this->bus_.registers[2], 0xF3C0);
}

TEST_F(TMP102Test, RestoresRegistersAfterChipOnlyResetBeforePublishing) {
  this->chip_.set_configure(true);
  this->chip_.set_extended_mode(true);
  this->chip_.set_temperature_high(90);
  this->chip_.set_temperature_low(85);
  this->chip_.setup();
  const auto configured = this->bus_.registers;
  this->chip_.update();
  this->bus_.registers = {0x1900, 0x60A0, 0x4B00, 0x5000};
  this->chip_.update();
  EXPECT_EQ(this->publications_, 1);
  EXPECT_TRUE(this->chip_.status_has_warning());
  for (size_t reg = 1; reg < 4; ++reg)
    EXPECT_EQ(this->bus_.registers[reg], configured[reg]);
  this->chip_.update();
  EXPECT_EQ(this->publications_, 2);
  EXPECT_FLOAT_EQ(this->chip_.state, 25);
  EXPECT_FALSE(this->chip_.status_has_warning());
}

TEST_F(TMP102Test, RecoveryRetriesFailedWritesAndDetectsThresholdOnlyChange) {
  this->chip_.set_configure(true);
  this->chip_.setup();
  this->bus_.registers[3] = 0;
  this->bus_.fail_write = 3;
  this->chip_.update();
  EXPECT_EQ(this->publications_, 0);
  this->bus_.fail_write = -1;
  this->chip_.update();
  EXPECT_EQ(this->bus_.registers[3], 0x5000);
  this->chip_.update();
  EXPECT_EQ(this->publications_, 1);
}

TEST_F(TMP102Test, ConfigurationReadFailuresDoNotPublishOrOverwriteRegisters) {
  this->chip_.set_configure(true);
  this->chip_.setup();
  for (int reg : {1, 2, 3}) {
    const auto publications = this->publications_;
    const auto writes = this->bus_.writes;
    this->bus_.fail_read = reg;
    this->chip_.update();
    EXPECT_TRUE(this->chip_.status_has_warning());
    EXPECT_EQ(this->publications_, publications);
    EXPECT_EQ(this->bus_.writes, writes);
    this->bus_.fail_read = -1;
  }
}

TEST_F(TMP102Test, ReadOnlyConfigurationBitsDoNotTriggerRecovery) {
  this->chip_.set_configure(true);
  this->chip_.setup();
  const auto writes = this->bus_.writes;
  this->bus_.registers[1] ^= 0xE020;
  this->chip_.update();
  EXPECT_EQ(this->publications_, 1);
  EXPECT_EQ(this->bus_.writes, writes);
  EXPECT_FALSE(this->chip_.status_has_warning());
}

TEST_F(TMP102Test, OneShotTriggerFailureSetsWarning) {
  this->chip_.set_configure(true);
  this->chip_.set_one_shot_mode(true);
  this->chip_.setup();
  this->bus_.fail_write = 1;
  this->chip_.update();
  run_timers(45);
  EXPECT_EQ(this->publications_, 0);
  EXPECT_TRUE(this->chip_.status_has_warning());
}

TEST_F(TMP102Test, OneShotWaitsOnceAndReselectsTemperature) {
  this->chip_.set_configure(true);
  this->chip_.set_one_shot_mode(true);
  this->chip_.setup();
  this->chip_.update();
  auto transactions = this->bus_.transactions;
  this->chip_.update();
  EXPECT_EQ(this->bus_.transactions, transactions);
  EXPECT_EQ(this->publications_, 0);
  this->bus_.pointer = 3;
  run_timers(45);
  EXPECT_EQ(this->publications_, 1);
  EXPECT_FLOAT_EQ(this->chip_.state, 25);
}

TEST_F(TMP102Test, RecursiveUpdateFromPublicationDoesNotStartAnotherRead) {
  this->chip_.add_on_state_callback([this](float) { this->chip_.update(); });
  this->chip_.setup();
  this->chip_.update();
  EXPECT_EQ(this->publications_, 1);
}

TEST_F(TMP102Test, FailureOfAnySetupWriteMarksComponentFailed) {
  for (int reg : {1, 2, 3}) {
    TMP102Component failed;
    failed.set_i2c_bus(&this->bus_);
    failed.set_i2c_address(0x48);
    failed.set_configure(true);
    this->bus_.fail_write = reg;
    failed.setup();
    EXPECT_TRUE(failed.is_failed());
    const auto transactions = this->bus_.transactions;
    failed.update();
    EXPECT_EQ(this->bus_.transactions, transactions);
  }
}

}  // namespace esphome::tmp102::testing

#endif
