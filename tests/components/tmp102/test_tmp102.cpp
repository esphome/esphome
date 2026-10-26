#ifdef USE_HOST

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <functional>
#include <utility>
#include "esphome/components/host/preferences.h"
#include "esphome/components/tmp102/tmp102.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::tmp102::testing {

class EntityIdentity : public EntityBase {
 public:
  static void configure(EntityBase &entity, const char *name, uint32_t hash) {
    auto configure = &EntityIdentity::configure_entity_;
    (entity.*configure)(name, hash, 0);
  }
};

class RegisterBus final : public i2c::I2CBus {
 public:
  std::array<uint16_t, 4> registers{0x1900, 0x60A0, 0x4B00, 0x5000};
  int fail_read{-1};
  int fail_write{-1};
  int fail_select{-1};
  unsigned transactions{0};
  unsigned writes{0};
  uint8_t pointer{0};
  std::function<void()> on_write;

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
        if (this->on_write) {
          auto callback = std::move(this->on_write);
          callback();
        }
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
  binary_sensor::BinarySensor alert_;
  text_sensor::TextSensor status_;
  TMP102LimitNumber high_{TMP102_LIMIT_HIGH};
  TMP102LimitNumber low_{TMP102_LIMIT_LOW};
  unsigned publications_{0};

  void SetUp() override {
    // The test main keeps App alive; this test replaces it with a fresh one it owns
    App.~Application();
    new (&App) Application();
    App.pre_setup("tmp102-regression-tests", 23, "TMP102 tests", 12);
    host::setup_preferences();
    host::host_preferences->reset();
    this->chip_.set_i2c_bus(&this->bus_);
    this->chip_.set_i2c_address(0x48);
    this->chip_.add_on_state_callback([this](float) { ++this->publications_; });
    EntityIdentity::configure(this->high_, "High", 0x102001);
    EntityIdentity::configure(this->low_, "Low", 0x102002);
    for (auto *number : {&this->high_, &this->low_}) {
      number->set_parent(&this->chip_);
      number->traits.set_min_value(-55);
      number->traits.set_max_value(127.9375f);
      number->traits.set_step(0.0625f);
    }
  }

  void TearDown() override {
    App.scheduler.cancel_timeout(&this->chip_, uint32_t{0});
    App.scheduler.call(millis());
    host::host_preferences->reset();
    App.~Application();
    new (&App) Application();  // later tests and the test listener use App
  }

  static void run_timers(uint32_t duration) {
    App.scheduler.call(millis());
    delay(duration);
    App.scheduler.call(millis());
  }

  void configure(uint16_t config = 0x0080, uint16_t high = 0x5000, uint16_t low = 0x4B00) {
    this->chip_.set_configuration(config, high, low);
  }

  static uint16_t encode(float temperature, bool extended = false) {
    const int16_t steps = static_cast<int16_t>(std::lround(temperature / 0.0625f));
    return static_cast<uint16_t>(static_cast<uint32_t>(static_cast<uint16_t>(steps)) << (extended ? 3 : 4));
  }

  void numbers_(float initial_high = 30, float initial_low = 25, bool extended = false) {
    this->configure(extended ? 0x0090 : 0x0080, encode(initial_high, extended), encode(initial_low, extended));
    if (extended) {
      this->high_.traits.set_max_value(150);
      this->low_.traits.set_max_value(150);
    }
    this->chip_.set_limit_control(&this->high_);
    this->chip_.set_limit_control(&this->low_);
  }

  static float saved(TMP102LimitNumber &number) {
    float value = NAN;
    EXPECT_TRUE(number.make_entity_preference<float>().load(&value));
    return value;
  }

  static void save(TMP102LimitNumber &number, float value) {
    ASSERT_TRUE(number.make_entity_preference<float>().save(&value));
  }

  static void fail_saves(TMP102LimitNumber &number) { number.pref_ = {}; }
};

TEST_F(TMP102Test, LegacySetupAndContinuousReadDoNotWriteRegisters) {
  const auto original = this->bus_.registers;
  this->chip_.setup();
  EXPECT_EQ(this->bus_.transactions, 0);
  this->chip_.update();
  run_timers(55);
  EXPECT_FLOAT_EQ(this->chip_.state, 25);
  EXPECT_EQ(this->publications_, 1);
  EXPECT_EQ(this->bus_.writes, 0);
  EXPECT_EQ(this->bus_.registers, original);
}

TEST_F(TMP102Test, ReadOnlyStatusDoesNotConfigureOrWriteRegisters) {
  this->chip_.set_threshold_status_text_sensor(&this->status_);
  const auto original = this->bus_.registers;
  this->chip_.setup();
  EXPECT_EQ(this->bus_.transactions, 0);
  EXPECT_EQ(this->bus_.registers, original);
  ASSERT_TRUE(this->status_.has_state());
  EXPECT_EQ(this->status_.state, "Not configured");
}

TEST_F(TMP102Test, ConfiguredSetupPublishesStatus) {
  this->chip_.set_threshold_status_text_sensor(&this->status_);
  this->configure();
  this->chip_.setup();
  ASSERT_TRUE(this->status_.has_state());
  EXPECT_EQ(this->status_.state, "OK");
}

TEST_F(TMP102Test, SampleFormatOverridesConfiguredModeInBothDirections) {
  for (bool configured_extended : {false, true}) {
    this->configure(configured_extended ? 0x0010 : 0, configured_extended ? 0x2800 : 0x5000,
                    configured_extended ? 0x2580 : 0x4B00);
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
  this->configure(0x1FD0, 0x2800, 0x2580);
  this->chip_.setup();
  EXPECT_EQ(this->bus_.registers[1], 0x1FD0);
  EXPECT_EQ(this->bus_.registers[3], 0x2800);
  EXPECT_EQ(this->bus_.registers[2], 0x2580);
}

TEST_F(TMP102Test, AlertEntityPreservesHardwareAndUsesObservedPolarity) {
  this->chip_.set_alert_binary_sensor(&this->alert_);
  this->chip_.setup();
  EXPECT_EQ(this->bus_.transactions, 0);
  for (uint16_t config : {0x60A0, 0x6080, 0x64A0, 0x6480}) {
    this->bus_.registers[1] = config;
    this->chip_.update();
    run_timers(55);
    ASSERT_TRUE(this->alert_.has_state());
    EXPECT_EQ(this->alert_.state, bool(config & 0x20) == bool(config & 0x400));
    EXPECT_EQ(this->bus_.registers[1], config);
  }
  EXPECT_EQ(this->bus_.writes, 0);
}

TEST_F(TMP102Test, FailedTemperatureOrAlertReadInvalidatesAlert) {
  this->chip_.set_alert_binary_sensor(&this->alert_);
  this->chip_.setup();
  this->chip_.update();
  run_timers(55);
  ASSERT_TRUE(this->alert_.has_state());
  for (int reg : {0, 1}) {
    this->bus_.fail_read = reg;
    this->chip_.update();
    run_timers(55);
    EXPECT_TRUE(this->chip_.status_has_warning());
    EXPECT_FALSE(this->alert_.has_state());
    this->bus_.fail_read = -1;
    this->chip_.update();
    run_timers(55);
    EXPECT_FALSE(this->chip_.status_has_warning());
    EXPECT_TRUE(this->alert_.has_state());
  }
}

TEST_F(TMP102Test, ConfiguredAlertUsesObservedPolarity) {
  this->chip_.set_alert_binary_sensor(&this->alert_);
  for (uint16_t config : {0x0080, 0x00A0, 0x0480, 0x04A0}) {
    this->configure(config & ~0x0020);
    this->chip_.setup();
    this->bus_.registers[1] = config;
    this->chip_.update();
    ASSERT_TRUE(this->alert_.has_state());
    EXPECT_EQ(this->alert_.state, bool(config & 0x0020) == bool(config & 0x0400));
    EXPECT_FALSE(this->chip_.status_has_warning());
  }
}

TEST_F(TMP102Test, ConfiguredAlertInvalidatesDuringRecoveryAndRepublishes) {
  this->chip_.set_alert_binary_sensor(&this->alert_);
  this->configure(0x0090, 0x2D00, 0x2A80);
  this->chip_.setup();
  this->chip_.update();
  ASSERT_TRUE(this->alert_.has_state());

  this->bus_.registers = {0x1900, 0x60A0, 0x4B00, 0x5000};
  this->chip_.update();
  EXPECT_FALSE(this->alert_.has_state());
  EXPECT_TRUE(this->chip_.status_has_warning());

  this->chip_.update();
  EXPECT_TRUE(this->alert_.has_state());
  EXPECT_FALSE(this->chip_.status_has_warning());
}

TEST_F(TMP102Test, OneShotPublishesAlertAfterDelayedRead) {
  this->chip_.set_alert_binary_sensor(&this->alert_);
  this->configure(0x0180);
  this->chip_.setup();
  this->chip_.update();
  EXPECT_FALSE(this->alert_.has_state());
  run_timers(45);
  EXPECT_TRUE(this->alert_.has_state());
  EXPECT_TRUE(this->alert_.state);
  EXPECT_FALSE(this->chip_.status_has_warning());
}

TEST_F(TMP102Test, QuantizedRuntimeValuesMatchRegistersAndPreferences) {
  this->numbers_((10.0f - 32) * 5 / 9, (9.0f - 32) * 5 / 9);
  this->chip_.setup();
  EXPECT_FLOAT_EQ(this->high_.state, -12.25f);
  EXPECT_FLOAT_EQ(this->low_.state, -12.75f);
  EXPECT_EQ(this->bus_.registers[3], 0xF3C0);
  for (auto [requested, expected] :
       {std::pair{12.22f, 12.25f}, {-12.22f, -12.25f}, {12.03125f, 12.0625f}, {-12.03125f, -12.0625f}}) {
    this->high_.make_call().set_value(requested).perform();
    EXPECT_FLOAT_EQ(this->high_.state, expected);
    EXPECT_FLOAT_EQ(saved(this->high_), expected);
    EXPECT_FLOAT_EQ(static_cast<int16_t>(this->bus_.registers[3]) / 256.0f, expected);
  }
}

TEST_F(TMP102Test, ExtendedModeRuntimeValueMatchesRegister) {
  this->numbers_(140, 120, true);
  this->chip_.setup();
  this->high_.make_call().set_value(145.0625f).perform();
  EXPECT_FLOAT_EQ(this->high_.state, 145.0625f);
  EXPECT_EQ(this->bus_.registers[3], encode(145.0625f, true));
  EXPECT_FLOAT_EQ(saved(this->high_), 145.0625f);
}

TEST_F(TMP102Test, HighOnlyInvalidRestoreFallsBackAgainstStaticLow) {
  this->configure(0x0080, encode(30), encode(25));
  this->chip_.set_limit_control(&this->high_);
  save(this->high_, 20);
  this->chip_.setup();
  EXPECT_FLOAT_EQ(this->high_.state, 30);
  EXPECT_EQ(this->bus_.registers[3], encode(30));
  EXPECT_EQ(this->bus_.registers[2], encode(25));
  EXPECT_FLOAT_EQ(saved(this->high_), 30);
}

TEST_F(TMP102Test, OrderingUsesAcceptedQuantizedThreshold) {
  this->numbers_(30.02f, 30.01f);
  this->chip_.setup();
  EXPECT_FLOAT_EQ(this->high_.state, 30);
  EXPECT_FLOAT_EQ(this->low_.state, 30);
  this->high_.make_call().set_value(29.96f).perform();
  EXPECT_FLOAT_EQ(this->high_.state, 30);
  this->high_.make_call().set_value(29.99f).perform();
  EXPECT_FLOAT_EQ(this->high_.state, 30);
}

TEST_F(TMP102Test, FailedThresholdWriteWarningClearsAfterSuccessfulRead) {
  this->numbers_();
  this->chip_.setup();
  this->bus_.fail_write = 3;
  this->high_.make_call().set_value(90).perform();
  EXPECT_TRUE(this->chip_.status_has_warning());
  this->chip_.update();
  EXPECT_FALSE(this->chip_.status_has_warning());
}

TEST_F(TMP102Test, RestoresRegistersAfterChipOnlyResetBeforePublishing) {
  this->configure(0x0090, 0x2D00, 0x2A80);
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
  this->configure();
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
  this->configure();
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
  this->configure();
  this->chip_.setup();
  const auto writes = this->bus_.writes;
  this->bus_.registers[1] ^= 0xE020;
  this->chip_.update();
  EXPECT_EQ(this->publications_, 1);
  EXPECT_EQ(this->bus_.writes, writes);
  EXPECT_FALSE(this->chip_.status_has_warning());
}

TEST_F(TMP102Test, OneShotTriggerFailureSetsWarning) {
  this->chip_.set_alert_binary_sensor(&this->alert_);
  this->configure(0x0180);
  this->chip_.setup();
  this->alert_.publish_state(false);
  this->bus_.fail_write = 1;
  this->chip_.update();
  run_timers(45);
  EXPECT_EQ(this->publications_, 0);
  EXPECT_TRUE(this->chip_.status_has_warning());
  EXPECT_FALSE(this->alert_.has_state());
}

TEST_F(TMP102Test, OneShotRestartsPendingReadAndReselectsTemperature) {
  this->configure(0x0180);
  this->chip_.setup();
  this->chip_.update();
  this->chip_.update();
  EXPECT_EQ(this->publications_, 0);
  this->bus_.pointer = 3;
  run_timers(45);
  EXPECT_EQ(this->publications_, 1);
  EXPECT_FLOAT_EQ(this->chip_.state, 25);
}

TEST_F(TMP102Test, LimitsAreOnlyCheckedWhenConfigurationMatchesResetDefault) {
  this->configure();
  this->chip_.setup();
  this->bus_.registers[2] = 0;
  this->chip_.update();
  EXPECT_EQ(this->publications_, 0);
  EXPECT_TRUE(this->chip_.status_has_warning());
  EXPECT_EQ(this->bus_.registers[2], 0x4B00);

  this->configure(0x00C0);
  this->chip_.setup();
  this->bus_.registers[2] = 0;
  const auto transactions = this->bus_.transactions;
  const auto writes = this->bus_.writes;
  this->chip_.update();
  EXPECT_EQ(this->bus_.transactions - transactions, 2);
  EXPECT_EQ(this->bus_.writes, writes);
  EXPECT_EQ(this->publications_, 1);
}

TEST_F(TMP102Test, FailureOfAnySetupWriteMarksComponentFailed) {
  for (int reg : {1, 2, 3}) {
    TMP102Component failed;
    failed.set_i2c_bus(&this->bus_);
    failed.set_i2c_address(0x48);
    failed.set_configuration(0x0080, 0x5000, 0x4B00);
    this->bus_.fail_write = reg;
    failed.setup();
    EXPECT_TRUE(failed.is_failed());
  }
}

TEST_F(TMP102Test, InvalidRestoredPairIsRepairedAfterSuccessfulSetup) {
  this->chip_.set_threshold_status_text_sensor(&this->status_);
  this->numbers_();
  save(this->high_, 20);
  save(this->low_, 35);
  this->chip_.setup();
  EXPECT_FLOAT_EQ(this->high_.state, 30);
  EXPECT_FLOAT_EQ(this->low_.state, 25);
  EXPECT_FLOAT_EQ(saved(this->high_), 30);
  EXPECT_FLOAT_EQ(saved(this->low_), 25);
  EXPECT_EQ(this->status_.state, "Restored thresholds reset");
}

TEST_F(TMP102Test, InvalidRestoredPairAndSaveFailurePublishCombinedStatus) {
  this->chip_.set_threshold_status_text_sensor(&this->status_);
  this->numbers_();
  save(this->high_, 20);
  save(this->low_, 35);
  this->bus_.on_write = [this]() {
    fail_saves(this->high_);
    fail_saves(this->low_);
  };
  this->chip_.setup();
  EXPECT_FLOAT_EQ(this->high_.state, 30);
  EXPECT_FLOAT_EQ(this->low_.state, 25);
  EXPECT_EQ(this->status_.state, "Restore reset; save failed");
}

TEST_F(TMP102Test, InvalidRestoredValuesAreRepaired) {
  this->chip_.set_threshold_status_text_sensor(&this->status_);
  this->numbers_();
  save(this->high_, NAN);
  save(this->low_, INFINITY);
  this->chip_.setup();
  EXPECT_FLOAT_EQ(this->high_.state, 30);
  EXPECT_FLOAT_EQ(this->low_.state, 25);
  EXPECT_FLOAT_EQ(saved(this->high_), 30);
  EXPECT_FLOAT_EQ(saved(this->low_), 25);
  EXPECT_EQ(this->status_.state, "Restored thresholds reset");
}

TEST_F(TMP102Test, FailedSetupDoesNotOverwritePreferences) {
  this->chip_.set_threshold_status_text_sensor(&this->status_);
  this->numbers_();
  save(this->high_, 90);
  save(this->low_, 85);
  this->bus_.fail_write = 3;
  this->chip_.setup();
  EXPECT_TRUE(this->chip_.is_failed());
  EXPECT_EQ(this->status_.state, "Setup failed");
  EXPECT_FALSE(this->high_.has_state());
  EXPECT_FLOAT_EQ(saved(this->high_), 90);
  EXPECT_FLOAT_EQ(saved(this->low_), 85);
}

TEST_F(TMP102Test, FailedSetupRejectsThresholdChanges) {
  this->chip_.set_threshold_status_text_sensor(&this->status_);
  this->numbers_();
  save(this->high_, 90);
  this->bus_.fail_write = 3;
  this->chip_.setup();
  ASSERT_TRUE(this->chip_.is_failed());
  const unsigned writes = this->bus_.writes;
  this->high_.make_call().set_value(60).perform();
  EXPECT_FALSE(this->high_.has_state());
  EXPECT_EQ(this->bus_.writes, writes);
  EXPECT_FLOAT_EQ(saved(this->high_), 90);
  EXPECT_EQ(this->status_.state, "Rejected: sensor setup failed");
}

TEST_F(TMP102Test, SetupOnlySavesChangedThresholds) {
  this->numbers_();
  save(this->high_, 35);
  this->chip_.setup();
  EXPECT_FLOAT_EQ(this->high_.state, 35);
  EXPECT_FLOAT_EQ(this->low_.state, 25);
  EXPECT_EQ(this->bus_.registers[3], encode(35));
  float value;
  EXPECT_FALSE(this->low_.make_entity_preference<float>().load(&value));
}

TEST_F(TMP102Test, FailedNumberWriteKeepsAcceptedState) {
  this->chip_.set_threshold_status_text_sensor(&this->status_);
  this->numbers_();
  save(this->high_, 90);
  save(this->low_, 85);
  this->chip_.setup();
  unsigned publications = 0;
  this->high_.add_on_state_callback([&publications](float) { ++publications; });
  this->bus_.fail_write = 3;
  this->high_.make_call().set_value(100).perform();
  EXPECT_FLOAT_EQ(this->high_.state, 90);
  EXPECT_EQ(publications, 1);
  EXPECT_FLOAT_EQ(saved(this->high_), 90);
  EXPECT_EQ(this->status_.state, "Write failed: high threshold");

  this->bus_.fail_write = 2;
  this->low_.make_call().set_value(80).perform();
  EXPECT_FLOAT_EQ(this->low_.state, 85);
  EXPECT_FLOAT_EQ(saved(this->low_), 85);
  EXPECT_EQ(this->status_.state, "Write failed: low threshold");
}

TEST_F(TMP102Test, PreferenceSaveFailurePublishesStatus) {
  this->chip_.set_threshold_status_text_sensor(&this->status_);
  this->numbers_();
  this->chip_.setup();
  fail_saves(this->high_);
  this->high_.make_call().set_value(35).perform();
  EXPECT_FLOAT_EQ(this->high_.state, 35);
  EXPECT_EQ(this->status_.state, "Preference save failed");
}

TEST_F(TMP102Test, RejectedNumberWritePublishesStatus) {
  this->chip_.set_threshold_status_text_sensor(&this->status_);
  this->numbers_();
  this->chip_.setup();
  this->high_.make_call().set_value(20).perform();
  EXPECT_EQ(this->status_.state, "Rejected: high 20.0000°C crosses low 25.0000°C");
  this->high_.make_call().set_value(35).perform();
  EXPECT_EQ(this->status_.state, "OK");
}

TEST_F(TMP102Test, ConfigurationRecoveryPublishesStatus) {
  this->chip_.set_threshold_status_text_sensor(&this->status_);
  this->configure();
  this->chip_.setup();
  this->bus_.registers[1] = 0;
  this->chip_.update();
  EXPECT_EQ(this->status_.state, "Configuration restored");

  this->bus_.registers[1] = 0;
  this->bus_.fail_write = 1;
  this->chip_.update();
  EXPECT_EQ(this->status_.state, "Configuration recovery failed");
}

TEST_F(TMP102Test, RestoreDisabledNeverChangesSavedValue) {
  this->numbers_();
  save(this->high_, 90);
  this->high_.set_restore_value(false);
  this->chip_.setup();
  EXPECT_FLOAT_EQ(this->high_.state, 30);
  this->high_.make_call().set_value(40).perform();
  EXPECT_FLOAT_EQ(this->high_.state, 40);
  EXPECT_FLOAT_EQ(saved(this->high_), 90);
}

}  // namespace esphome::tmp102::testing

#endif
