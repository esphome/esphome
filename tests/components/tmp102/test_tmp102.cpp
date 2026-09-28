#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <utility>
#include "esphome/components/host/preferences.h"
#include "esphome/components/tmp102/tmp102.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::tmp102::testing {

// Only the physical bus is simulated; entities, preferences and scheduler are real.
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

// Set test entity identities without registering stack objects in the global application.
class EntityIdentity : public EntityBase {
 public:
  static void configure(EntityBase &entity, const char *name, uint32_t hash) {
    auto configure = &EntityIdentity::configure_entity_;
    (entity.*configure)(name, hash, 0);
  }
};

class TMP102Test : public ::testing::Test {
 protected:
  RegisterBus bus_;
  TMP102Component chip_;
  binary_sensor::BinarySensor alert_;
  text_sensor::TextSensor status_;
  TMP102LimitNumber high_{&this->chip_, TMP102_LIMIT_HIGH};
  TMP102LimitNumber low_{&this->chip_, TMP102_LIMIT_LOW};
  unsigned publications_{0};

  void SetUp() override {
    // The test runner does not call generated setup(), which normally constructs App.
    new (&App) Application;
    App.pre_setup("tmp102-regression-tests", 23, "TMP102 tests", 12);
    // No sync() is called: test preferences remain in memory.
    host::setup_preferences();
    host::host_preferences->reset();
    this->chip_.set_i2c_bus(&this->bus_);
    this->chip_.set_i2c_address(0x48);
    this->chip_.add_on_state_callback([this](float) { ++this->publications_; });
    this->chip_.set_threshold_status_text_sensor(&this->status_);
    EntityIdentity::configure(this->high_, "High", 0x102001);
    EntityIdentity::configure(this->low_, "Low", 0x102002);
    for (auto *number : {&this->high_, &this->low_}) {
      number->traits.set_min_value(-55);
      number->traits.set_max_value(127.9375f);
      number->traits.set_step(0.0625f);
    }
  }

  void TearDown() override {
    App.scheduler.cancel_timeout(&this->chip_, "read_temp");
    App.scheduler.cancel_timeout(&this->high_, "revert");
    App.scheduler.cancel_timeout(&this->low_, "revert");
    App.scheduler.call(millis());
    host::host_preferences->reset();
    App.~Application();
  }

  void numbers_(float initial_high = 30, float initial_low = 25) {
    this->chip_.set_configure(true);
    this->chip_.set_temperature_high(initial_high);
    this->chip_.set_temperature_low(initial_low);
    this->high_.set_initial_value(initial_high);
    this->low_.set_initial_value(initial_low);
    this->chip_.set_high_limit_control(&this->high_);
    this->chip_.set_low_limit_control(&this->low_);
  }

  void setup_numbers_() {
    this->chip_.setup();
    this->high_.setup();
    this->low_.setup();
  }

  static float saved(TMP102LimitNumber &number) {
    float value = NAN;
    EXPECT_TRUE(number.make_entity_preference<float>().load(&value));
    return value;
  }

  static void save(TMP102LimitNumber &number, float value) {
    ASSERT_TRUE(number.make_entity_preference<float>().save(&value));
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

TEST_F(TMP102Test, ThresholdEncodingRangeAndFailedWrites) {
  for (bool extended : {false, true}) {
    this->chip_.set_configure(true);
    this->chip_.set_extended_mode(extended);
    this->chip_.set_temperature_high(80);
    this->chip_.set_temperature_low(75);
    this->chip_.setup();
    ASSERT_TRUE(this->chip_.set_limit_temperature(TMP102_LIMIT_LOW, -55));
    EXPECT_EQ(this->bus_.registers[2], extended ? 0xE480 : 0xC900);
    for (float invalid : {NAN, INFINITY, extended ? 151.0f : 128.0f, -56.0f})
      EXPECT_FALSE(this->chip_.set_limit_temperature(TMP102_LIMIT_HIGH, invalid));
    EXPECT_FALSE(this->chip_.set_limit_temperature(TMP102_LIMIT_LOW, 81));
    ASSERT_TRUE(this->chip_.set_limit_temperature(TMP102_LIMIT_HIGH, 90));
    this->bus_.fail_write = 3;
    EXPECT_FALSE(this->chip_.set_limit_temperature(TMP102_LIMIT_HIGH, 100));
    EXPECT_FLOAT_EQ(this->chip_.get_limit_temperature(TMP102_LIMIT_HIGH), 90);
    this->bus_.fail_write = -1;
  }
}

TEST_F(TMP102Test, QuantizedInitialAndRuntimeValuesMatchRegistersAndPreferences) {
  this->numbers_((10.0f - 32) * 5 / 9, (9.0f - 32) * 5 / 9);
  this->setup_numbers_();
  EXPECT_FLOAT_EQ(this->high_.state, -12.25f);
  EXPECT_FLOAT_EQ(this->low_.state, -12.75f);
  EXPECT_EQ(this->bus_.registers[3], 0xF3C0);
  EXPECT_FLOAT_EQ(saved(this->high_), this->high_.state);
  for (auto [requested, expected] :
       {std::pair{12.22f, 12.25f}, {-12.22f, -12.25f}, {12.03125f, 12.0625f}, {-12.03125f, -12.0625f}}) {
    this->high_.make_call().set_value(requested).perform();
    EXPECT_FLOAT_EQ(this->high_.state, expected);
    EXPECT_FLOAT_EQ(this->chip_.get_limit_temperature(TMP102_LIMIT_HIGH), expected);
    EXPECT_FLOAT_EQ(saved(this->high_), expected);
    EXPECT_FLOAT_EQ(static_cast<int16_t>(this->bus_.registers[3]) / 256.0f, expected);
  }
}

TEST_F(TMP102Test, OrderingUsesAcceptedQuantizedThreshold) {
  this->numbers_(30.02f, 30.01f);
  this->setup_numbers_();
  EXPECT_FLOAT_EQ(this->high_.state, 30);
  EXPECT_FLOAT_EQ(this->low_.state, 30);
  EXPECT_FALSE(this->chip_.set_limit_temperature(TMP102_LIMIT_HIGH, 29.96f));
  EXPECT_TRUE(this->chip_.set_limit_temperature(TMP102_LIMIT_HIGH, 29.99f));
  EXPECT_FLOAT_EQ(this->chip_.get_limit_temperature(TMP102_LIMIT_HIGH), 30);
}

TEST_F(TMP102Test, ReadOnlyEntitiesPreserveHardwareAndUseObservedPolarity) {
  this->chip_.set_alert_binary_sensor(&this->alert_);
  this->chip_.setup();
  EXPECT_EQ(this->bus_.transactions, 0);
  for (uint16_t config : {0x60A0, 0x6080, 0x64A0, 0x6480}) {
    this->bus_.registers[1] = config;
    this->chip_.update();
    ASSERT_TRUE(this->alert_.has_state());
    EXPECT_EQ(this->alert_.state, bool(config & 0x20) == bool(config & 0x400));
    EXPECT_EQ(this->bus_.registers[1], config);
  }
  EXPECT_EQ(this->bus_.writes, 0);
  EXPECT_FALSE(this->status_.has_state());
}

TEST_F(TMP102Test, ReadFailuresInvalidateAlertAndSuccessfulWriteDoesNotClearWarning) {
  this->chip_.set_alert_binary_sensor(&this->alert_);
  this->chip_.setup();
  this->chip_.update();
  ASSERT_TRUE(this->alert_.has_state());
  for (int reg : {0, 1}) {
    this->bus_.fail_read = reg;
    this->chip_.update();
    EXPECT_TRUE(this->chip_.status_has_warning());
    EXPECT_FALSE(this->alert_.has_state());
    ASSERT_TRUE(this->chip_.set_limit_temperature(TMP102_LIMIT_HIGH, 90));
    EXPECT_TRUE(this->chip_.status_has_warning());
    this->bus_.fail_read = -1;
    this->chip_.update();
    EXPECT_FALSE(this->chip_.status_has_warning());
    EXPECT_TRUE(this->alert_.has_state());
  }
  this->bus_.fail_select = 0;
  this->chip_.update();
  EXPECT_TRUE(this->chip_.status_has_warning());
  EXPECT_FALSE(this->alert_.has_state());
}

TEST_F(TMP102Test, FailedThresholdWriteRetainsWarningAcrossSuccessfulRead) {
  this->chip_.setup();
  this->bus_.fail_write = 3;
  EXPECT_FALSE(this->chip_.set_limit_temperature(TMP102_LIMIT_HIGH, 90));
  this->chip_.update();
  EXPECT_TRUE(this->chip_.status_has_warning());
  this->bus_.fail_write = -1;
  EXPECT_TRUE(this->chip_.set_limit_temperature(TMP102_LIMIT_HIGH, 90));
  this->chip_.update();
  EXPECT_FALSE(this->chip_.status_has_warning());
}

TEST_F(TMP102Test, RestoresRegistersAfterChipOnlyResetBeforePublishing) {
  this->chip_.set_configure(true);
  this->chip_.set_extended_mode(true);
  this->chip_.set_alert_binary_sensor(&this->alert_);
  this->chip_.set_temperature_high(90);
  this->chip_.set_temperature_low(85);
  this->chip_.setup();
  const auto configured = this->bus_.registers;
  this->chip_.update();
  this->bus_.registers = {0x1900, 0x60A0, 0x4B00, 0x5000};
  this->chip_.update();
  EXPECT_EQ(this->publications_, 1);
  EXPECT_FALSE(this->alert_.has_state());
  EXPECT_TRUE(this->chip_.status_has_warning());
  EXPECT_EQ(this->status_.state, "Configuration restored");
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
  EXPECT_EQ(this->status_.state, "Configuration recovery failed");
  this->bus_.fail_write = -1;
  this->chip_.update();
  EXPECT_EQ(this->bus_.registers[3], 0x5000);
  this->chip_.update();
  EXPECT_EQ(this->publications_, 1);
}

TEST_F(TMP102Test, ConfigurationReadFailuresDoNotPublishOrOverwriteRegisters) {
  this->chip_.set_configure(true);
  this->chip_.set_alert_binary_sensor(&this->alert_);
  this->chip_.setup();
  for (int reg : {1, 2, 3}) {
    this->chip_.update();
    ASSERT_TRUE(this->alert_.has_state());
    const auto publications = this->publications_;
    const auto writes = this->bus_.writes;
    this->bus_.fail_read = reg;
    this->chip_.update();
    EXPECT_TRUE(this->chip_.status_has_warning());
    EXPECT_FALSE(this->alert_.has_state());
    EXPECT_EQ(this->publications_, publications);
    EXPECT_EQ(this->bus_.writes, writes);
    this->bus_.fail_read = -1;
  }
}

TEST_F(TMP102Test, ReadOnlyConfigurationBitsDoNotTriggerRecovery) {
  this->chip_.set_configure(true);
  this->chip_.setup();
  const auto writes = this->bus_.writes;
  this->bus_.registers[1] ^= 0xE020;  // OS, resolution and AL are not configuration mismatches.
  this->chip_.update();
  EXPECT_EQ(this->publications_, 1);
  EXPECT_EQ(this->bus_.writes, writes);
  EXPECT_FALSE(this->chip_.status_has_warning());
}

TEST_F(TMP102Test, OneShotTriggerFailureInvalidatesAlert) {
  this->chip_.set_configure(true);
  this->chip_.set_one_shot_mode(true);
  this->chip_.set_alert_binary_sensor(&this->alert_);
  this->chip_.setup();
  this->alert_.publish_state(false);
  this->bus_.fail_write = 1;
  this->chip_.update();
  run_timers(45);
  EXPECT_EQ(this->publications_, 0);
  EXPECT_TRUE(this->chip_.status_has_warning());
  EXPECT_FALSE(this->alert_.has_state());
}

TEST_F(TMP102Test, OneShotWaitsOnceAndReselectsTemperatureAfterThresholdWrite) {
  this->chip_.set_configure(true);
  this->chip_.set_one_shot_mode(true);
  this->chip_.setup();
  this->chip_.update();
  auto transactions = this->bus_.transactions;
  this->chip_.update();
  EXPECT_EQ(this->bus_.transactions, transactions);
  EXPECT_EQ(this->publications_, 0);
  EXPECT_TRUE(this->chip_.set_limit_temperature(TMP102_LIMIT_HIGH, 90));
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

TEST_F(TMP102Test, RecursiveUpdateFromRecoveryStatusDoesNotRetryImmediately) {
  this->chip_.set_configure(true);
  this->chip_.setup();
  this->bus_.registers[3] = 0;
  this->bus_.fail_write = 3;
  unsigned status_updates = 0;
  this->status_.add_on_state_callback([this, &status_updates](const std::string &) {
    ++status_updates;
    this->chip_.update();
  });
  this->chip_.update();
  EXPECT_EQ(status_updates, 1);
  EXPECT_EQ(this->publications_, 0);
  this->bus_.fail_write = -1;
  this->chip_.update();
  EXPECT_EQ(status_updates, 2);
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
    EXPECT_FALSE(failed.set_limit_temperature(TMP102_LIMIT_HIGH, 90));
  }
}

TEST_F(TMP102Test, InvalidRestoredPairIsRepairedAfterSuccessfulSetup) {
  this->numbers_();
  save(this->high_, 20);
  save(this->low_, 35);
  this->setup_numbers_();
  EXPECT_FLOAT_EQ(this->high_.state, 30);
  EXPECT_FLOAT_EQ(this->low_.state, 25);
  EXPECT_FLOAT_EQ(saved(this->high_), 30);
  EXPECT_FLOAT_EQ(saved(this->low_), 25);
  EXPECT_EQ(this->status_.state, "Invalid restored thresholds; reset to initial");
  this->chip_.setup();
  EXPECT_EQ(this->status_.state, "OK");
}

TEST_F(TMP102Test, InvalidRestoredValuesAreRepaired) {
  this->numbers_();
  save(this->high_, NAN);
  save(this->low_, INFINITY);
  this->setup_numbers_();
  EXPECT_FLOAT_EQ(saved(this->high_), 30);
  EXPECT_FLOAT_EQ(saved(this->low_), 25);
  EXPECT_EQ(this->status_.state, "Invalid restored thresholds; reset to initial");
}

TEST_F(TMP102Test, FailedSetupDoesNotOverwritePreferences) {
  this->numbers_();
  save(this->high_, 20);
  save(this->low_, 35);
  this->bus_.fail_write = 2;
  this->setup_numbers_();
  EXPECT_TRUE(this->chip_.is_failed());
  EXPECT_TRUE(this->high_.is_failed());
  EXPECT_FLOAT_EQ(saved(this->high_), 20);
  EXPECT_FLOAT_EQ(saved(this->low_), 35);
  this->chip_.update();
  EXPECT_EQ(this->publications_, 0);
}

TEST_F(TMP102Test, ValidRestoredPairAndFailedNumberWriteKeepAcceptedState) {
  this->numbers_();
  save(this->high_, 40);
  save(this->low_, 35);
  this->setup_numbers_();
  EXPECT_FLOAT_EQ(this->high_.state, 40);
  EXPECT_FLOAT_EQ(this->low_.state, 35);
  this->bus_.fail_write = 3;
  this->high_.make_call().set_value(50).perform();
  EXPECT_FLOAT_EQ(this->high_.state, 40);
  EXPECT_FLOAT_EQ(saved(this->high_), 40);
  this->bus_.fail_write = -1;
  this->high_.make_call().set_value(60).perform();
  run_timers(260);
  EXPECT_FLOAT_EQ(this->high_.state, 60);
  EXPECT_FLOAT_EQ(saved(this->high_), 60);
}

TEST_F(TMP102Test, RestoreDisabledNeverChangesSavedValue) {
  this->numbers_();
  save(this->high_, 90);
  this->high_.set_restore_value(false);
  this->setup_numbers_();
  EXPECT_FLOAT_EQ(this->high_.state, 30);
  this->high_.make_call().set_value(40).perform();
  EXPECT_FLOAT_EQ(this->high_.state, 40);
  EXPECT_FLOAT_EQ(saved(this->high_), 90);
}

}  // namespace esphome::tmp102::testing
