#include <gtest/gtest.h>

#include <array>
#include <ctime>
#include <vector>

#include "esphome/components/rx8111/rx8111.h"
#include "esphome/core/application.h"

namespace esphome::rx8111::testing {

static constexpr uint8_t ADDRESS = 0x32;
static constexpr uint8_t SEC = 0x10;
static constexpr uint8_t WEEK = 0x13;
static constexpr uint8_t DAY = 0x14;
static constexpr uint8_t MONTH = 0x15;
static constexpr uint8_t FLAG = 0x1E;
static constexpr uint8_t CONTROL = 0x1F;
static constexpr uint8_t EXTENSION = 0x1D;
static constexpr uint8_t SEC_ALARM = 0x2C;
static constexpr uint8_t MIN_ALARM = 0x17;
static constexpr uint8_t POWER_SWITCH = 0x32;
static constexpr uint8_t TIMER_CONTROL = 0x2D;
static constexpr uint8_t EVIN = 0x2B;
static constexpr uint8_t TIMESTAMP_CONTROL = 0x34;

class RegisterBus final : public i2c::I2CBus {
 public:
  std::array<uint8_t, 256> regs{};
  std::vector<uint8_t> writes;
  int fail_read{-1};
  int fail_read_once{-1};
  int fail_write{-1};
  int fail_control_write_number{-1};
  unsigned control_writes{0};
  unsigned flag_reads{0};
  bool latch_event_before_flag_write{false};

  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *out, size_t out_size, uint8_t *in,
                             size_t in_size) override {
    EXPECT_EQ(address, ADDRESS);
    EXPECT_GT(out_size, 0);
    if (out_size == 0)
      return i2c::ERROR_TIMEOUT;
    const uint8_t reg = out[0];
    if (in_size > 0) {
      if (reg == FLAG)
        this->flag_reads++;
      if (reg == this->fail_read)
        return i2c::ERROR_TIMEOUT;
      if (reg == this->fail_read_once) {
        this->fail_read_once = -1;
        return i2c::ERROR_TIMEOUT;
      }
      for (size_t i = 0; i < in_size; i++)
        in[i] = this->regs[reg + i];
    } else {
      if (reg == CONTROL && ++this->control_writes == this->fail_control_write_number)
        return i2c::ERROR_TIMEOUT;
      if (reg == this->fail_write)
        return i2c::ERROR_TIMEOUT;
      this->writes.push_back(reg);
      if (reg == FLAG) {
        if (this->latch_event_before_flag_write)
          this->regs[FLAG] |= 0x04;  // EVF latches immediately before FLAG write.
        // RX8111 flags are write-zero-to-clear; writing one leaves the bit unchanged.
        this->regs[FLAG] &= out[1];
      } else {
        for (size_t i = 1; i < out_size; i++)
          this->regs[reg + i - 1] = out[i];
      }
    }
    return i2c::ERROR_OK;
  }
};

class RX8111Test : public ::testing::Test {
 protected:
  void SetUp() override {
    this->rtc_.set_i2c_bus(&this->bus_);
    this->rtc_.set_i2c_address(ADDRESS);
    EXPECT_EQ(this->rtc_.boot_retry_delay_ms_, 1000U);  // Production default remains one second.
    this->rtc_.boot_retry_delay_ms_ = 5;
    this->rtc_.add_on_time_sync_callback([this]() { this->syncs_++; });
  }

  void TearDown() override {
    App.scheduler.cancel_interval(&this->rtc_, uint32_t{0});
    App.scheduler.cancel_timeout(&this->rtc_, uint32_t{1});
    App.scheduler.cancel_timeout(&this->rtc_, uint32_t{2});
    App.scheduler.cancel_timeout(&this->rtc_, uint32_t{3});
    App.scheduler.cancel_timeout(&this->rtc_, uint32_t{4});
    App.scheduler.cancel_timeout(&this->rtc_, uint32_t{5});
    App.scheduler.call(millis());  // Reap cancelled timers while the fixture is still alive.
  }

  void advance_retry_() {
    delay(20);
    App.scheduler.call(millis());
  }

  // TEST_F bodies are subclasses, so the friendship stops here
  bool polling_started_() const { return this->rtc_.event_polling_started_; }
  bool timer_enabled_() const { return this->rtc_.timer_enabled_; }
  void configure_battery_() { this->rtc_.configure_boot_battery_(); }
  bool power_warning_() const { return (this->rtc_.warnings_ & RX8111Component::WARN_BATTERY) != 0; }
  bool fout_warning_() const { return (this->rtc_.warnings_ & RX8111Component::WARN_FOUT) != 0; }
  bool event_warning_() const { return (this->rtc_.warnings_ & RX8111Component::WARN_EVENT) != 0; }
  bool event_clear_warning_() const { return (this->rtc_.warnings_ & RX8111Component::WARN_EVENT_FLAG_CLEAR) != 0; }
  void poll_() { this->rtc_.check_event_flags_(); }

  RegisterBus bus_;
  RX8111Component rtc_;
  unsigned syncs_{0};
};

TEST_F(RX8111Test, OmittedPowerAndFoutOptionsDoNotTouchBatteryBackedRegisters) {
  this->bus_.regs[POWER_SWITCH] = 0xC5;
  this->bus_.regs[EXTENSION] = 0xA7;
  this->rtc_.setup();
  EXPECT_EQ(this->bus_.regs[POWER_SWITCH], 0xC5);
  EXPECT_EQ(this->bus_.regs[EXTENSION], 0xA7);
  EXPECT_TRUE(this->bus_.writes.empty());
}

TEST_F(RX8111Test, EventSettingsPreserveUnrelatedBitsAndLatchedFlag) {
  this->bus_.regs[TIMESTAMP_CONTROL] = 0x85;  // EISEL plus TSRAM and a reserved bit.
  this->bus_.regs[EVIN] = 1;
  this->bus_.regs[EXTENSION] = 0x90;
  this->bus_.regs[CONTROL] = 0x90;
  this->bus_.regs[FLAG] = 0x04;
  this->rtc_.set_event_config(0xC4);
  int callbacks = 0;
  this->rtc_.add_on_event_callback([&]() { callbacks++; });
  this->rtc_.setup();
  EXPECT_EQ(this->bus_.regs[EVIN], 0xC5);
  EXPECT_EQ(this->bus_.regs[EXTENSION], 0x94);  // ETS enables EVIN detection; preserve other bits.
  EXPECT_EQ(this->bus_.regs[CONTROL], 0x94);
  EXPECT_EQ(this->bus_.regs[TIMESTAMP_CONTROL], 0x81);
  this->poll_();
  this->poll_();
  EXPECT_EQ(callbacks, 1);
  EXPECT_EQ(this->bus_.regs[FLAG], 0x04);
  EXPECT_TRUE(this->rtc_.clear_event_flag());
  this->bus_.regs[FLAG] = 0x04;
  this->poll_();
  EXPECT_EQ(callbacks, 2);
}

TEST_F(RX8111Test, EventWithoutCallbackConfiguresHardwareWithoutPolling) {
  this->rtc_.set_event_config(0xC4);
  this->rtc_.setup();
  EXPECT_EQ(this->bus_.regs[CONTROL] & 0x04, 0x04);
  EXPECT_FALSE(this->polling_started_());
  const auto reads = this->bus_.flag_reads;
  this->rtc_.update();
  // One calendar FLAG read, no event FLAG read.
  EXPECT_EQ(this->bus_.flag_reads, reads + 1);
  EXPECT_TRUE(this->rtc_.clear_event_flag());
}

TEST_F(RX8111Test, EventWithCallbackStartsPolling) {
  this->rtc_.set_event_config(0);
  this->rtc_.add_on_event_callback([]() {});
  this->rtc_.setup();
  EXPECT_TRUE(this->polling_started_());
}

TEST_F(RX8111Test, EventConfigurationRetriesWithoutDroppingRetainedFlag) {
  this->bus_.regs[EVIN] = 0x03;  // Preserve EVIN's reserved and retained capture bits.
  this->bus_.regs[FLAG] = 0x04;
  this->bus_.fail_write = EVIN;
  this->rtc_.set_event_config(0xC4);
  int callbacks = 0;
  this->rtc_.add_on_event_callback([&]() { callbacks++; });
  this->rtc_.setup();
  EXPECT_TRUE(this->event_warning_());
  EXPECT_EQ(this->bus_.regs[EVIN], 0x03);
  EXPECT_EQ(this->bus_.regs[CONTROL] & 0x04, 0);
  EXPECT_EQ(this->bus_.regs[EXTENSION] & 0x04, 0);
  this->poll_();
  EXPECT_EQ(callbacks, 0);
  this->bus_.fail_write = -1;
  this->advance_retry_();
  EXPECT_FALSE(this->event_warning_());
  EXPECT_EQ(this->bus_.regs[EVIN], 0xC7);
  EXPECT_EQ(this->bus_.regs[EXTENSION] & 0x04, 0x04);
  EXPECT_EQ(this->bus_.regs[CONTROL] & 0x04, 0x04);
  this->poll_();
  EXPECT_EQ(callbacks, 1);
  EXPECT_EQ(this->bus_.regs[FLAG], 0x04);
}

TEST_F(RX8111Test, EventExtensionFailureRetriesBeforeEnablingInterrupt) {
  this->bus_.regs[EXTENSION] = 0xA0;
  this->bus_.fail_write = EXTENSION;
  this->rtc_.set_event_config(0xD0);
  this->rtc_.setup();
  EXPECT_TRUE(this->event_warning_());
  EXPECT_EQ(this->bus_.regs[EVIN], 0xD0);
  EXPECT_EQ(this->bus_.regs[EXTENSION], 0xA0);
  EXPECT_EQ(this->bus_.regs[CONTROL] & 0x04, 0);
  this->bus_.fail_write = -1;
  this->advance_retry_();
  EXPECT_FALSE(this->event_warning_());
  EXPECT_EQ(this->bus_.regs[EXTENSION], 0xA4);
  EXPECT_EQ(this->bus_.regs[CONTROL] & 0x04, 0x04);
}

TEST_F(RX8111Test, ClearEventFlagPreservesOtherFlagsAndRetriesAfterFailure) {
  this->bus_.regs[FLAG] = 0x1C;
  this->bus_.fail_write = FLAG;
  EXPECT_FALSE(this->rtc_.clear_event_flag());
  EXPECT_TRUE(this->event_clear_warning_());
  EXPECT_EQ(this->bus_.regs[FLAG], 0x1C);
  this->bus_.fail_write = -1;
  EXPECT_TRUE(this->rtc_.clear_event_flag());
  EXPECT_FALSE(this->event_clear_warning_());
  EXPECT_EQ(this->bus_.regs[FLAG], 0x18);
}

TEST_F(RX8111Test, EventFlagClearDoesNotHideBootConfigurationFailure) {
  this->rtc_.set_event_config(0xC4);
  this->bus_.fail_write = EVIN;
  this->rtc_.setup();
  ASSERT_TRUE(this->event_warning_());
  this->bus_.fail_write = FLAG;
  EXPECT_FALSE(this->rtc_.clear_event_flag());
  EXPECT_TRUE(this->event_clear_warning_());
  this->bus_.fail_write = EVIN;
  EXPECT_TRUE(this->rtc_.clear_event_flag());
  EXPECT_FALSE(this->event_clear_warning_());
  EXPECT_TRUE(this->event_warning_());
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->bus_.fail_write = -1;
  this->advance_retry_();
  EXPECT_FALSE(this->event_warning_());
  EXPECT_FALSE(this->event_clear_warning_());
}

TEST_F(RX8111Test, ExplicitEventDisableDoesNotChangeCaptureOrFlag) {
  this->bus_.regs[EXTENSION] = 0x04;
  this->bus_.regs[CONTROL] = 0x0C;
  this->bus_.regs[FLAG] = 0x04;
  this->rtc_.set_event_disabled();
  this->rtc_.setup();
  EXPECT_EQ(this->bus_.writes, (std::vector<uint8_t>{CONTROL}));
  EXPECT_EQ(this->bus_.regs[CONTROL], 0x08);
  EXPECT_EQ(this->bus_.regs[EXTENSION], 0x04);
  EXPECT_EQ(this->bus_.regs[FLAG], 0x04);
}

TEST_F(RX8111Test, BatteryAndFoutPreserveUnrelatedBitsAndTimerRouting) {
  this->bus_.regs[POWER_SWITCH] = 0xB5;
  this->bus_.regs[EXTENSION] = 0xDA;
  this->rtc_.set_battery_backup(true, false);
  this->rtc_.set_fout_frequency(1);
  this->rtc_.setup();
  EXPECT_EQ(this->bus_.regs[POWER_SWITCH], 0x79);
  EXPECT_EQ(this->bus_.regs[EXTENSION], 0x5A);
  EXPECT_EQ(this->bus_.writes, (std::vector<uint8_t>{POWER_SWITCH, EXTENSION}));
  EXPECT_FALSE(this->power_warning_());
  EXPECT_FALSE(this->fout_warning_());
}

TEST_F(RX8111Test, ChargingRequiresExplicitEnableAndCanBeDisabled) {
  this->bus_.regs[POWER_SWITCH] = 0x15;
  this->rtc_.set_battery_backup(true, true);
  this->rtc_.setup();
  EXPECT_EQ(this->bus_.regs[POWER_SWITCH], 0xD9);
  this->rtc_.set_battery_backup(false, false);
  this->configure_battery_();
  EXPECT_EQ(this->bus_.regs[POWER_SWITCH], 0x19);
}

TEST_F(RX8111Test, FoutRefusesTimerPinRoutingWithoutModifyingIt) {
  this->bus_.regs[TIMER_CONTROL] = 0x02;
  this->bus_.regs[EXTENSION] = 0xC5;
  this->rtc_.set_fout_frequency(0);
  this->rtc_.setup();
  EXPECT_EQ(this->bus_.regs[EXTENSION], 0xC5);
  EXPECT_TRUE(this->fout_warning_());
  EXPECT_TRUE(this->bus_.writes.empty());
  this->bus_.regs[TIMER_CONTROL] = 0;
  this->advance_retry_();
  EXPECT_EQ(this->bus_.regs[EXTENSION], 0x05);
  EXPECT_FALSE(this->fout_warning_());
}

TEST_F(RX8111Test, PowerWriteFailureRetriesAndLeavesWarningOnExhaustion) {
  this->bus_.regs[POWER_SWITCH] = 0x85;
  this->bus_.fail_write = POWER_SWITCH;
  this->rtc_.set_battery_backup(true, false);
  this->rtc_.setup();
  EXPECT_EQ(this->bus_.regs[POWER_SWITCH], 0x85);
  EXPECT_TRUE(this->power_warning_());
  this->advance_retry_();
  this->advance_retry_();
  this->bus_.fail_write = -1;
  this->advance_retry_();
  EXPECT_EQ(this->bus_.regs[POWER_SWITCH], 0x85);
  EXPECT_TRUE(this->power_warning_());
  this->configure_battery_();
  EXPECT_EQ(this->bus_.regs[POWER_SWITCH], 0x49);
  EXPECT_FALSE(this->power_warning_());
}

TEST_F(RX8111Test, ExplicitBatteryConfigurationNormalizesEverySwitchSelection) {
  for (uint8_t selection = 0; selection < 4; selection++) {
    for (bool enabled : {false, true}) {
      this->bus_.regs[POWER_SWITCH] = 0xB3 | (selection << 2);
      this->rtc_.set_battery_backup(enabled, false);
      this->configure_battery_();
      EXPECT_EQ(this->bus_.regs[POWER_SWITCH], enabled ? 0x7B : 0x3B);
    }
  }
}

TEST_F(RX8111Test, TimestampSelectionFailureRetriesBeforeEnablingDetection) {
  this->bus_.regs[TIMESTAMP_CONTROL] = 0x05;
  this->bus_.fail_write = TIMESTAMP_CONTROL;
  this->rtc_.set_event_config(0);
  this->rtc_.setup();
  EXPECT_EQ(this->bus_.regs[EXTENSION] & 0x04, 0);
  EXPECT_EQ(this->bus_.regs[CONTROL] & 0x04, 0);
  this->bus_.fail_write = -1;
  this->advance_retry_();
  EXPECT_EQ(this->bus_.regs[TIMESTAMP_CONTROL], 0x01);
  EXPECT_EQ(this->bus_.regs[EXTENSION] & 0x04, 0x04);
  EXPECT_EQ(this->bus_.regs[CONTROL] & 0x04, 0x04);
}

TEST_F(RX8111Test, ExplicitTimerStartNormalizesRetainedModeAtBootAndRuntime) {
  this->rtc_.set_timer_count(2);
  this->rtc_.set_timer_configured(true);
  this->rtc_.set_timer_enabled(true);
  this->bus_.regs[TIMER_CONTROL] = 0xFF;
  this->rtc_.setup();
  EXPECT_EQ(this->bus_.regs[TIMER_CONTROL], 0xF0);
  for (uint8_t mode : {0x01, 0x02, 0x04, 0x08, 0x0C, 0x0F}) {
    this->bus_.regs[TIMER_CONTROL] = 0xF0 | mode;
    ASSERT_TRUE(this->rtc_.start_timer());
    EXPECT_EQ(this->bus_.regs[TIMER_CONTROL], 0xF0);
    EXPECT_EQ(this->bus_.regs[EXTENSION] & 0x10, 0x10);
  }
}

TEST_F(RX8111Test, TimerModeFailureDoesNotStartCounterAndBootRetries) {
  this->rtc_.set_timer_count(2);
  this->rtc_.set_timer_configured(true);
  this->rtc_.set_timer_enabled(true);
  this->bus_.regs[TIMER_CONTROL] = 1;
  this->bus_.fail_write = TIMER_CONTROL;
  this->rtc_.setup();
  EXPECT_EQ(this->bus_.regs[EXTENSION] & 0x10, 0);
  this->bus_.fail_write = -1;
  this->advance_retry_();
  EXPECT_EQ(this->bus_.regs[TIMER_CONTROL], 0);
  EXPECT_EQ(this->bus_.regs[EXTENSION] & 0x10, 0x10);
  this->bus_.fail_write = TIMER_CONTROL;
  EXPECT_FALSE(this->rtc_.start_timer());
  EXPECT_EQ(this->bus_.regs[EXTENSION] & 0x10, 0);
  this->bus_.fail_write = -1;
  ASSERT_TRUE(this->rtc_.start_timer());
  EXPECT_EQ(this->bus_.regs[TIMER_CONTROL], 0);
}

TEST_F(RX8111Test, FailedRestartKeepsStopAcrossHostResetUntilFreshTrustedWrite) {
  this->bus_.fail_control_write_number = 2;
  this->rtc_.write_time();
  EXPECT_EQ(this->bus_.regs[CONTROL] & 1, 1);
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->rtc_.read_time();
  EXPECT_EQ(this->syncs_, 0);
  this->bus_.writes.clear();
  this->bus_.fail_write = CONTROL;
  this->advance_retry_();
  EXPECT_EQ(this->bus_.regs[CONTROL] & 1, 1);
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->bus_.fail_write = -1;
  this->advance_retry_();
  EXPECT_EQ(this->bus_.regs[CONTROL] & 1, 1);
  EXPECT_TRUE(this->bus_.writes.empty());
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->rtc_.read_time();
  EXPECT_EQ(this->syncs_, 0);  // Even with initially clear hardware validity flags.
  RX8111Component rebooted;
  rebooted.set_i2c_bus(&this->bus_);
  rebooted.set_i2c_address(ADDRESS);
  rebooted.add_on_time_sync_callback([this]() { this->syncs_++; });
  rebooted.setup();
  EXPECT_EQ(this->syncs_, 0);           // The RAM guard is gone, but STOP survives.
  EXPECT_EQ(this->bus_.regs[FLAG], 0);  // Even with validity flags already cleared.
  rebooted.write_time();
  rebooted.read_time();
  EXPECT_EQ(this->syncs_, 1);
  this->rtc_.write_time();
  EXPECT_FALSE(this->rtc_.status_has_warning());
  this->rtc_.read_time();
  EXPECT_EQ(this->syncs_, 2);
}

TEST_F(RX8111Test, FailedCalendarWriteKeepsStopAndDoesNotRetry) {
  this->bus_.regs[FLAG] = 0x83;
  this->bus_.fail_write = SEC;
  this->bus_.fail_control_write_number = 2;
  this->rtc_.write_time();
  this->bus_.fail_write = -1;
  this->bus_.writes.clear();
  this->advance_retry_();
  EXPECT_EQ(this->bus_.regs[CONTROL] & 1, 1);
  EXPECT_EQ(this->bus_.regs[FLAG], 0x83);
  EXPECT_TRUE(this->bus_.writes.empty());
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->rtc_.read_time();
  EXPECT_EQ(this->syncs_, 0);
}

TEST_F(RX8111Test, FailedCalendarWriteRejectsPreviouslyValidCalendarUntilFreshWrite) {
  this->rtc_.write_time();
  ASSERT_EQ(this->bus_.regs[FLAG], 0);
  this->bus_.fail_write = SEC;
  this->rtc_.write_time();
  EXPECT_EQ(this->bus_.regs[CONTROL] & 1, 1);
  this->rtc_.read_time();
  EXPECT_EQ(this->syncs_, 0);
  this->bus_.fail_write = -1;
  this->rtc_.write_time();
  this->rtc_.read_time();
  EXPECT_EQ(this->syncs_, 1);
}

TEST_F(RX8111Test, IncompleteWritesRemainInvalidAfterComponentRecreation) {
  for (const uint8_t failed_register : {SEC, FLAG, CONTROL}) {
    this->bus_.fail_write = -1;
    this->rtc_.write_time();
    ASSERT_EQ(this->bus_.regs[FLAG], 0);
    this->bus_.fail_write = failed_register == CONTROL ? -1 : failed_register;
    this->bus_.fail_control_write_number = failed_register == CONTROL ? this->bus_.control_writes + 2 : -1;
    this->rtc_.write_time();
    ASSERT_EQ(this->bus_.regs[CONTROL] & 1, 1);
    this->bus_.fail_write = -1;
    RX8111Component rebooted;
    rebooted.set_i2c_bus(&this->bus_);
    rebooted.set_i2c_address(ADDRESS);
    unsigned syncs = 0;
    rebooted.add_on_time_sync_callback([&syncs]() { syncs++; });
    rebooted.setup();
    rebooted.read_time();
    EXPECT_EQ(syncs, 0) << "failed register " << unsigned(failed_register);
    rebooted.write_time();
    rebooted.read_time();
    EXPECT_EQ(syncs, 1);
  }
}

TEST_F(RX8111Test, PersistentControlFailureDoesNotRearmRecovery) {
  this->bus_.fail_control_write_number = 2;
  this->rtc_.write_time();
  this->bus_.fail_write = CONTROL;
  const auto control_writes = this->bus_.control_writes;
  for (unsigned i = 0; i < 10; i++)
    this->advance_retry_();
  EXPECT_EQ(this->bus_.control_writes, control_writes);
  EXPECT_EQ(this->bus_.regs[CONTROL] & 1, 1);
  EXPECT_TRUE(this->rtc_.status_has_warning());
}

TEST_F(RX8111Test, NewTrustedWriteRestartsStoppedCounterWithoutDeferredRecovery) {
  this->bus_.fail_control_write_number = 2;
  this->rtc_.write_time();
  ASSERT_EQ(this->bus_.regs[CONTROL] & 1, 1);
  this->rtc_.write_time();
  EXPECT_EQ(this->bus_.regs[CONTROL] & 1, 0);
  EXPECT_FALSE(this->rtc_.status_has_warning());
  this->bus_.writes.clear();
  this->advance_retry_();
  EXPECT_TRUE(this->bus_.writes.empty());
}

TEST_F(RX8111Test, FailedValidityCleanupRequiresFreshWriteAndPreservesOtherFaults) {
  this->bus_.regs[FLAG] = 0x83;
  this->bus_.fail_write = FLAG;
  this->rtc_.write_time();
  EXPECT_TRUE(this->rtc_.status_has_warning());
  EXPECT_EQ(this->bus_.regs[CONTROL] & 1, 1);
  this->bus_.fail_write = POWER_SWITCH;
  this->rtc_.set_battery_backup(false, false);
  this->configure_battery_();
  this->bus_.writes.clear();
  this->advance_retry_();
  EXPECT_TRUE(this->bus_.writes.empty());
  EXPECT_EQ(this->bus_.regs[FLAG], 0x83);
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->bus_.fail_write = -1;
  this->configure_battery_();
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->rtc_.read_time();
  EXPECT_EQ(this->syncs_, 0);
  this->rtc_.write_time();
  EXPECT_FALSE(this->rtc_.status_has_warning());
  this->rtc_.read_time();
  EXPECT_EQ(this->syncs_, 1);
}

TEST_F(RX8111Test, FoutReadFailureDoesNotAlterExtensionAndRecovers) {
  this->bus_.regs[EXTENSION] = 0xC5;
  this->bus_.fail_read = EXTENSION;
  this->rtc_.set_fout_frequency(3);
  this->rtc_.setup();
  EXPECT_EQ(this->bus_.regs[EXTENSION], 0xC5);
  EXPECT_TRUE(this->fout_warning_());
  this->bus_.fail_read = -1;
  this->advance_retry_();
  EXPECT_EQ(this->bus_.regs[EXTENSION], 0xC5);
  EXPECT_FALSE(this->fout_warning_());
}

TEST_F(RX8111Test, WritesCalendarAndPreservesEventFlagsWithoutReadModifyWrite) {
  this->bus_.regs[FLAG] = 0xB8;  // POR + existing UF/TF/AF.
  this->bus_.regs[CONTROL] = 0x5C;
  this->bus_.latch_event_before_flag_write = true;
  const auto before = ESPTime::from_epoch_utc(::time(nullptr));
  this->rtc_.write_time();
  const auto after = ESPTime::from_epoch_utc(::time(nullptr));
  EXPECT_EQ(this->bus_.regs[CONTROL], 0x5C);
  EXPECT_EQ(this->bus_.regs[FLAG], 0x3C);  // The newly latched EVF survives too.
  auto bcd = [](int value) { return (value / 10) * 16 + value % 10; };
  EXPECT_TRUE((this->bus_.regs[SEC] == bcd(before.second) && this->bus_.regs[DAY] == bcd(before.day_of_month)) ||
              (this->bus_.regs[SEC] == bcd(after.second) && this->bus_.regs[DAY] == bcd(after.day_of_month)));
  EXPECT_TRUE(this->bus_.regs[WEEK] == (1 << (before.day_of_week - 1)) ||
              this->bus_.regs[WEEK] == (1 << (after.day_of_week - 1)));
  EXPECT_EQ(this->bus_.writes, (std::vector<uint8_t>{CONTROL, SEC, FLAG, CONTROL}));
}

TEST_F(RX8111Test, RejectsInvalidFlagsBCDWeekdayAndCalendar) {
  this->bus_.regs[FLAG] = 0x83;
  this->rtc_.write_time();
  EXPECT_EQ(this->bus_.regs[FLAG], 0);
  this->bus_.regs[FLAG] = 0x83;
  this->bus_.writes.clear();
  this->rtc_.read_time();
  EXPECT_EQ(this->syncs_, 0);
  EXPECT_TRUE(this->rtc_.status_has_warning());
  EXPECT_TRUE(this->bus_.writes.empty());
  this->bus_.regs[FLAG] = 0;
  this->rtc_.read_time();
  EXPECT_EQ(this->syncs_, 1);
  EXPECT_FALSE(this->rtc_.status_has_warning());
  const uint8_t valid_weekday = this->bus_.regs[WEEK];
  const uint8_t valid_day = this->bus_.regs[DAY];
  const uint8_t valid_month = this->bus_.regs[MONTH];
  const uint8_t wrong_weekday = valid_weekday == 1 ? 2 : 1;
  for (uint8_t weekday : {uint8_t{0x00}, uint8_t{0x03}, uint8_t{0x80}, wrong_weekday}) {
    this->bus_.regs[WEEK] = weekday;
    this->rtc_.read_time();
    EXPECT_EQ(this->syncs_, 1) << "weekday " << unsigned(weekday);
    EXPECT_TRUE(this->rtc_.status_has_warning());
  }
  this->bus_.regs[WEEK] = valid_weekday;
  this->bus_.regs[DAY] = 0x32;
  this->rtc_.read_time();
  EXPECT_EQ(this->syncs_, 1);
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->bus_.regs[DAY] = 0x31;
  this->bus_.regs[MONTH] = 0x04;  // April 31 is always invalid.
  this->rtc_.read_time();
  EXPECT_EQ(this->syncs_, 1);
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->bus_.regs[DAY] = valid_day;
  this->bus_.regs[MONTH] = valid_month;
  this->bus_.regs[SEC] = 0x6A;
  this->rtc_.read_time();
  EXPECT_EQ(this->syncs_, 1);
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->bus_.regs[SEC] = 0x60;  // Hardware supports leap adjustment; synchronization requires normal seconds.
  this->rtc_.read_time();
  EXPECT_EQ(this->syncs_, 1);
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->bus_.regs[SEC] = 0x00;
  this->rtc_.read_time();
  EXPECT_EQ(this->syncs_, 2);
  EXPECT_FALSE(this->rtc_.status_has_warning());
}

TEST_F(RX8111Test, I2CFailuresDoNotSyncAndIncompleteWritesLeaveCounterStopped) {
  this->bus_.fail_read = FLAG;
  this->rtc_.read_time();
  EXPECT_EQ(this->syncs_, 0);
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->bus_.fail_read = SEC;
  this->rtc_.read_time();
  EXPECT_EQ(this->syncs_, 0);
  this->bus_.fail_read = -1;
  this->bus_.fail_write = CONTROL;
  this->rtc_.write_time();
  EXPECT_TRUE(this->bus_.writes.empty());
  this->bus_.fail_write = SEC;
  this->rtc_.write_time();
  EXPECT_EQ(this->bus_.writes, (std::vector<uint8_t>{CONTROL}));
  EXPECT_EQ(this->bus_.regs[CONTROL] & 1, 1);
  this->bus_.fail_write = FLAG;
  this->bus_.regs[FLAG] = 0x83;
  this->rtc_.write_time();
  EXPECT_EQ(this->bus_.regs[FLAG], 0x83);
  EXPECT_EQ(this->bus_.regs[CONTROL] & 1, 1);
}

TEST_F(RX8111Test, WriteFailureClearsOnSuccessfulWriteButPreservesEventPollWarning) {
  this->bus_.fail_write = SEC;
  this->rtc_.write_time();
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->bus_.fail_write = -1;
  this->rtc_.write_time();
  EXPECT_FALSE(this->rtc_.status_has_warning());

  this->rtc_.set_alarm_enabled(true);
  ASSERT_TRUE(this->rtc_.set_alarm(1, 0xFF, 0xFF, 0xFF, 0xFF));
  this->bus_.fail_read = FLAG;
  this->rtc_.update();
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->bus_.fail_read = -1;
  this->bus_.fail_write = FLAG;
  this->rtc_.write_time();
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->bus_.fail_write = -1;
  this->rtc_.write_time();
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->rtc_.update();
  EXPECT_FALSE(this->rtc_.status_has_warning());
}

TEST_F(RX8111Test, RuntimeAlarmFailureRecoversOnlyAfterSuccessfulAlarmRequest) {
  this->rtc_.write_time();
  this->rtc_.set_alarm_enabled(true);
  this->bus_.fail_write = MIN_ALARM;
  EXPECT_FALSE(this->rtc_.set_alarm(1, 0xFF, 0xFF, 0xFF, 0xFF));
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->bus_.fail_write = -1;
  this->rtc_.read_time();
  EXPECT_TRUE(this->rtc_.status_has_warning());
  ASSERT_TRUE(this->rtc_.set_alarm(2, 0xFF, 0xFF, 0xFF, 0xFF));
  EXPECT_FALSE(this->rtc_.status_has_warning());

  this->bus_.regs[FLAG] = 0x80;
  this->rtc_.read_time();
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->bus_.fail_read = CONTROL;
  EXPECT_FALSE(this->rtc_.unset_alarm());
  this->bus_.fail_read = -1;
  ASSERT_TRUE(this->rtc_.unset_alarm());
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->rtc_.write_time();
  EXPECT_FALSE(this->rtc_.status_has_warning());
}

TEST_F(RX8111Test, RuntimeTimerAndFlagClearFailuresRecoverIndependently) {
  this->rtc_.write_time();
  this->rtc_.set_timer_count(2);
  this->bus_.fail_read = EXTENSION;
  EXPECT_FALSE(this->rtc_.start_timer());
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->bus_.fail_read = -1;
  ASSERT_TRUE(this->rtc_.start_timer());
  EXPECT_FALSE(this->rtc_.status_has_warning());

  this->bus_.fail_write = FLAG;
  EXPECT_FALSE(this->rtc_.clear_alarm_flag());
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->bus_.fail_write = -1;
  ASSERT_TRUE(this->rtc_.stop_timer());
  EXPECT_TRUE(this->rtc_.status_has_warning());
  ASSERT_TRUE(this->rtc_.clear_timer_flag());
  EXPECT_TRUE(this->rtc_.status_has_warning());
  ASSERT_TRUE(this->rtc_.clear_alarm_flag());
  EXPECT_FALSE(this->rtc_.status_has_warning());
}

TEST_F(RX8111Test, TimerFlagClearWarningRecoversOnlyAfterSuccessfulClear) {
  this->rtc_.write_time();
  this->bus_.regs[FLAG] = 0x10;
  this->bus_.fail_write = FLAG;
  EXPECT_FALSE(this->rtc_.clear_timer_flag());
  EXPECT_TRUE(this->rtc_.status_has_warning());
  EXPECT_EQ(this->bus_.regs[FLAG] & 0x10, 0x10);

  this->bus_.fail_write = -1;
  ASSERT_TRUE(this->rtc_.stop_timer());
  EXPECT_TRUE(this->rtc_.status_has_warning());
  ASSERT_TRUE(this->rtc_.clear_timer_flag());
  EXPECT_FALSE(this->rtc_.status_has_warning());
  EXPECT_EQ(this->bus_.regs[FLAG] & 0x10, 0);
}

TEST_F(RX8111Test, FailedBootTimerProgrammingRecoversOnRetry) {
  this->rtc_.write_time();
  this->rtc_.set_timer_count(2);
  this->rtc_.set_timer_enabled(true);
  this->rtc_.set_timer_configured(true);
  this->bus_.fail_write = 0x1A;  // Timer reload register; boot does not clear a retained TF.
  this->rtc_.setup();
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->bus_.fail_write = -1;
  this->advance_retry_();
  EXPECT_FALSE(this->rtc_.status_has_warning());
}

TEST_F(RX8111Test, AlarmProgramsMaskedFieldsAndPreservesOtherControls) {
  this->bus_.regs[CONTROL] = 0x54;
  this->bus_.regs[EXTENSION] = 0xC8;
  this->rtc_.set_alarm_enabled(true);
  ASSERT_TRUE(this->rtc_.set_alarm(0, 30, 7, 2, 0xFF));
  EXPECT_EQ(this->bus_.regs[SEC_ALARM], 0x00);
  EXPECT_EQ(this->bus_.regs[MIN_ALARM], 0x30);
  EXPECT_EQ(this->bus_.regs[MIN_ALARM + 1], 0x07);
  EXPECT_EQ(this->bus_.regs[MIN_ALARM + 2], 0x02);
  EXPECT_EQ(this->bus_.regs[EXTENSION], 0xC0);
  EXPECT_EQ(this->bus_.regs[CONTROL], 0x5C);
  ASSERT_TRUE(this->rtc_.set_alarm(0xFF, 5, 0xFF, 0xFF, 31));
  EXPECT_EQ(this->bus_.regs[SEC_ALARM], 0x80);
  EXPECT_EQ(this->bus_.regs[MIN_ALARM + 2], 0x31);
  EXPECT_EQ(this->bus_.regs[EXTENSION], 0xC8);
  EXPECT_EQ(this->bus_.regs[FLAG], 0);
  EXPECT_FALSE(this->rtc_.set_alarm(60, 0, 0, 0xFF, 0xFF));
  EXPECT_FALSE(this->rtc_.set_alarm(0, 0, 0, 1, 1));
  EXPECT_EQ(this->bus_.regs[SEC_ALARM], 0x80);
}

TEST_F(RX8111Test, FlagIsLatchedUntilExplicitClearAndOnlyEdgesTrigger) {
  unsigned alarms = 0;
  this->rtc_.set_alarm_enabled(true);
  this->rtc_.add_on_alarm_callback([&alarms]() { alarms++; });
  this->bus_.regs[FLAG] = 0x08;  // Event already latched before alarm configuration.
  ASSERT_TRUE(this->rtc_.set_alarm(0, 0, 0, 0xFF, 0xFF));
  this->rtc_.update();  // A preexisting battery-backed event is not a new edge.
  EXPECT_EQ(alarms, 0);
  this->rtc_.update();
  EXPECT_EQ(alarms, 0);
  this->bus_.regs[FLAG] = 0x3C;
  this->bus_.latch_event_before_flag_write = true;
  ASSERT_TRUE(this->rtc_.clear_alarm_flag());
  EXPECT_EQ(this->bus_.regs[FLAG], 0x34);
  EXPECT_EQ(this->bus_.writes.back(), FLAG);
  this->bus_.regs[FLAG] |= 0x08;
  this->rtc_.update();
  EXPECT_EQ(alarms, 1);
  this->rtc_.update();
  EXPECT_EQ(alarms, 1);
  ASSERT_TRUE(this->rtc_.unset_alarm());
  EXPECT_EQ(this->bus_.regs[CONTROL] & 0x08, 0);
  EXPECT_EQ(this->bus_.regs[FLAG] & 0x08, 0x08);
  EXPECT_EQ(this->bus_.regs[SEC_ALARM], 0x80);
}

TEST_F(RX8111Test, AlarmI2CFailureLeavesInterruptDisabled) {
  this->rtc_.set_alarm_enabled(true);
  this->bus_.regs[CONTROL] = 0x08;
  this->bus_.fail_write = SEC_ALARM;
  EXPECT_FALSE(this->rtc_.set_alarm(0, 0, 0, 0xFF, 0xFF));
  EXPECT_EQ(this->bus_.regs[CONTROL] & 0x08, 0);
  EXPECT_TRUE(this->rtc_.status_has_warning());
}

TEST_F(RX8111Test, FailedAlarmReprogramOrUnsetKeepsArmedAlarmObservable) {
  this->rtc_.set_alarm_enabled(true);
  ASSERT_TRUE(this->rtc_.set_alarm(0, 0, 0, 0xFF, 0xFF));
  unsigned calls = 0;
  this->rtc_.add_on_alarm_callback([this, &calls]() {
    calls++;
    EXPECT_TRUE(this->rtc_.clear_alarm_flag());
  });
  this->rtc_.update();  // Establish AF low before each attempt.

  for (const uint8_t failed_register : {EXTENSION, CONTROL}) {
    this->bus_.fail_read = failed_register;
    EXPECT_FALSE(this->rtc_.set_alarm(1, 0, 0, 0xFF, 0xFF));
    this->bus_.fail_read = -1;
    EXPECT_EQ(this->bus_.regs[CONTROL] & 0x08, 0x08);
    this->bus_.regs[FLAG] = 0x08;
    this->rtc_.update();
    EXPECT_EQ(calls, failed_register == EXTENSION ? 1 : 2);
  }
  for (const bool fail_write : {false, true}) {
    if (fail_write) {
      this->bus_.fail_write = CONTROL;
    } else {
      this->bus_.fail_read = CONTROL;
    }
    EXPECT_FALSE(this->rtc_.unset_alarm());
    this->bus_.fail_read = -1;
    this->bus_.fail_write = -1;
    EXPECT_EQ(this->bus_.regs[CONTROL] & 0x08, 0x08);
    this->bus_.regs[FLAG] = 0x08;
    this->rtc_.update();
    EXPECT_EQ(calls, fail_write ? 4 : 3);
  }
}

TEST_F(RX8111Test, FailedAlarmReprogramAfterDisableDoesNotDeliverOldAlarm) {
  this->rtc_.set_alarm_enabled(true);
  ASSERT_TRUE(this->rtc_.set_alarm(0, 0, 0, 0xFF, 0xFF));
  unsigned calls = 0;
  this->rtc_.add_on_alarm_callback([&calls]() { calls++; });
  this->rtc_.update();
  this->bus_.fail_write = SEC_ALARM;
  EXPECT_FALSE(this->rtc_.set_alarm(1, 0, 0, 0xFF, 0xFF));
  this->bus_.fail_write = -1;
  EXPECT_EQ(this->bus_.regs[CONTROL] & 0x08, 0);
  this->bus_.regs[FLAG] = 0x08;
  this->rtc_.update();
  EXPECT_EQ(calls, 0);
}

TEST_F(RX8111Test, ClearFromAlarmAutomationAllowsNextEvent) {
  this->rtc_.set_alarm_enabled(true);
  unsigned calls = 0;
  this->rtc_.add_on_alarm_callback([this, &calls]() {
    calls++;
    EXPECT_TRUE(this->rtc_.clear_alarm_flag());
  });
  ASSERT_TRUE(this->rtc_.set_alarm(0, 0, 0, 0xFF, 0xFF));
  this->rtc_.update();  // Initialize the edge detector with AF low.
  this->bus_.regs[FLAG] = 0x08;
  this->rtc_.update();
  EXPECT_EQ(calls, 1);
  EXPECT_EQ(this->bus_.regs[FLAG] & 0x08, 0);
  this->bus_.regs[FLAG] = 0x08;
  this->rtc_.update();
  EXPECT_EQ(calls, 2);
}

TEST_F(RX8111Test, RelativeAlarmUsesUTCAndRejectsInvalidDurations) {
  EXPECT_FALSE(this->rtc_.schedule_alarm_in(1));  // Opt-in required.
  this->rtc_.set_alarm_enabled(true);
  EXPECT_FALSE(this->rtc_.schedule_alarm_in(0));
  EXPECT_FALSE(this->rtc_.schedule_alarm_in(28U * 86400U));
  this->rtc_.write_time();
  const auto before = ESPTime::from_epoch_utc(::time(nullptr) + 60);
  ASSERT_TRUE(this->rtc_.schedule_alarm_in(60));
  const auto after = ESPTime::from_epoch_utc(::time(nullptr) + 60);
  const auto matches = [this](const ESPTime &time) {
    const auto bcd = [](int value) { return (value / 10) * 16 + value % 10; };
    return this->bus_.regs[SEC_ALARM] == bcd(time.second) && this->bus_.regs[MIN_ALARM] == bcd(time.minute) &&
           this->bus_.regs[MIN_ALARM + 1] == bcd(time.hour) && this->bus_.regs[MIN_ALARM + 2] == bcd(time.day_of_month);
  };
  EXPECT_TRUE(matches(before) || matches(after));
}

TEST_F(RX8111Test, RelativeAlarmReadsRTCWithoutSynchronizingSystemClock) {
  this->rtc_.set_alarm_enabled(true);
  this->bus_.regs[SEC] = 0x30;
  this->bus_.regs[SEC + 1] = 0x59;
  this->bus_.regs[SEC + 2] = 0x23;
  this->bus_.regs[WEEK] = 0x04;  // Tuesday, 2024-01-02.
  this->bus_.regs[DAY] = 0x02;
  this->bus_.regs[MONTH] = 0x01;
  this->bus_.regs[MONTH + 1] = 0x24;
  ASSERT_TRUE(this->rtc_.schedule_alarm_in(60));
  EXPECT_EQ(this->syncs_, 0);
  EXPECT_EQ(this->bus_.regs[SEC_ALARM], 0x30);
  EXPECT_EQ(this->bus_.regs[MIN_ALARM], 0x00);
  EXPECT_EQ(this->bus_.regs[MIN_ALARM + 1], 0x00);
  EXPECT_EQ(this->bus_.regs[MIN_ALARM + 2], 0x03);
  this->bus_.regs[FLAG] = 0x80;
  this->bus_.writes.clear();
  EXPECT_FALSE(this->rtc_.schedule_alarm_in(60));
  EXPECT_TRUE(this->bus_.writes.empty());
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->bus_.regs[FLAG] = 0;
  this->bus_.regs[WEEK] = 0;
  EXPECT_FALSE(this->rtc_.schedule_alarm_in(60));
  EXPECT_TRUE(this->bus_.writes.empty());
}

TEST_F(RX8111Test, TimerPrograms24BitCountAndPreservesAlarmAndOtherExtensionBits) {
  this->bus_.regs[EXTENSION] = 0xC8;
  this->bus_.regs[CONTROL] = 0x09;
  this->bus_.regs[FLAG] = 0x1C;
  this->rtc_.set_timer_enabled(true);
  this->rtc_.set_timer_configured(true);
  this->rtc_.set_timer_count(0x123456);
  this->rtc_.set_timer_clock(1);
  this->rtc_.setup();
  EXPECT_EQ(this->bus_.regs[0x1A], 0x56);
  EXPECT_EQ(this->bus_.regs[0x1B], 0x34);
  EXPECT_EQ(this->bus_.regs[0x1C], 0x12);
  EXPECT_EQ(this->bus_.regs[EXTENSION], 0xD9);
  EXPECT_EQ(this->bus_.regs[CONTROL], 0x19);
  EXPECT_EQ(this->bus_.regs[FLAG] & 0x1C, 0x1C);
  unsigned calls = 0;
  this->rtc_.add_on_timer_callback([&calls]() { calls++; });
  this->rtc_.update();  // Report a retained wake-up event once.
  EXPECT_EQ(calls, 1);
  ASSERT_TRUE(this->rtc_.clear_timer_flag());
  EXPECT_EQ(this->bus_.regs[FLAG] & 0x1C, 0x0C);
  this->bus_.regs[FLAG] |= 0x10;
  this->rtc_.update();
  this->rtc_.update();
  EXPECT_EQ(calls, 2);
  EXPECT_TRUE(this->rtc_.clear_timer_flag());
  this->bus_.regs[FLAG] |= 0x10;
  this->rtc_.update();
  EXPECT_EQ(calls, 3);
}

TEST_F(RX8111Test, ExplicitDisabledTimerStopsBatteryBackedCounter) {
  this->bus_.regs[EXTENSION] = 0xD9;
  this->bus_.regs[CONTROL] = 0xB7;
  this->bus_.regs[FLAG] = 0x10;
  this->rtc_.set_timer_configured(true);
  this->rtc_.set_timer_enabled(false);
  this->rtc_.setup();
  EXPECT_EQ(this->bus_.regs[EXTENSION], 0xC9);
  EXPECT_EQ(this->bus_.regs[CONTROL], 0xA7);
  EXPECT_EQ(this->bus_.regs[FLAG], 0x10);
  EXPECT_TRUE(this->rtc_.stop_timer());
  EXPECT_EQ(this->bus_.regs[EXTENSION], 0xC9);
  this->bus_.fail_write = CONTROL;
  EXPECT_FALSE(this->rtc_.stop_timer());
  EXPECT_TRUE(this->rtc_.status_has_warning());
}

TEST_F(RX8111Test, ExplicitDisabledAlarmClearsOnlyAIEAndPreservesAF) {
  this->bus_.regs[CONTROL] = 0x5C;
  this->bus_.regs[FLAG] = 0x08;
  this->rtc_.set_alarm_explicitly_configured(true);
  this->rtc_.set_alarm_enabled(false);
  this->rtc_.setup();
  EXPECT_EQ(this->bus_.regs[CONTROL], 0x54);
  EXPECT_EQ(this->bus_.regs[FLAG], 0x08);
  this->bus_.regs[CONTROL] = 0x5C;
  this->rtc_.set_alarm_explicitly_configured(false);
  this->rtc_.setup();
  EXPECT_EQ(this->bus_.regs[CONTROL], 0x5C);
}

TEST_F(RX8111Test, DisabledTimerReportsRetainedFlagOnceViaScheduler) {
  this->rtc_.set_timer_count(2);
  this->rtc_.set_timer_configured(true);
  this->rtc_.set_timer_enabled(false);
  this->rtc_.set_event_poll_interval(1);
  this->bus_.regs[FLAG] = 0x10;
  unsigned calls = 0;
  this->rtc_.add_on_timer_callback([&calls]() { calls++; });
  this->rtc_.setup();
  App.scheduler.call(millis());
  delay(20);
  App.scheduler.call(millis());
  EXPECT_EQ(calls, 1);
  EXPECT_EQ(this->bus_.regs[EXTENSION] & 0x10, 0);
  EXPECT_EQ(this->bus_.regs[CONTROL] & 0x10, 0);
  const unsigned flag_reads = this->bus_.flag_reads;
  delay(20);
  App.scheduler.call(millis());
  EXPECT_EQ(this->bus_.flag_reads, flag_reads);  // No repeated FLAG reads after the retained event.
  this->rtc_.update();
  EXPECT_EQ(calls, 1);
  EXPECT_TRUE(this->rtc_.start_timer());  // Restart polling when the timer is activated later.
  this->bus_.regs[FLAG] = 0x10;
  delay(20);
  App.scheduler.call(millis());
  EXPECT_EQ(calls, 2);
}

TEST_F(RX8111Test, DisabledTimerStopsPollingAfterCheckingForRetainedFlag) {
  this->rtc_.set_timer_count(2);
  this->rtc_.set_timer_configured(true);
  this->rtc_.set_timer_enabled(false);
  this->rtc_.set_event_poll_interval(1);
  this->rtc_.setup();
  App.scheduler.call(millis());
  delay(20);
  App.scheduler.call(millis());
  const unsigned flag_reads = this->bus_.flag_reads;
  EXPECT_GT(flag_reads, 0);
  delay(20);
  App.scheduler.call(millis());
  EXPECT_EQ(this->bus_.flag_reads, flag_reads);
}

TEST_F(RX8111Test, DisabledTimerRetriesFailedRetainedFlagReadBeforeStoppingPoll) {
  this->rtc_.set_timer_count(2);
  this->rtc_.set_timer_configured(true);
  this->rtc_.set_timer_enabled(false);
  this->rtc_.set_event_poll_interval(1);
  this->rtc_.setup();
  unsigned calls = 0;
  this->rtc_.add_on_timer_callback([&calls]() { calls++; });
  this->bus_.fail_read = FLAG;
  delay(20);
  App.scheduler.call(millis());
  this->bus_.fail_read = -1;
  this->bus_.regs[FLAG] = 0x10;
  delay(20);
  App.scheduler.call(millis());
  EXPECT_EQ(calls, 1);
  const unsigned flag_reads = this->bus_.flag_reads;
  delay(20);
  App.scheduler.call(millis());
  EXPECT_EQ(this->bus_.flag_reads, flag_reads);
}

TEST_F(RX8111Test, RearmingAlarmAfterCallbackStopsPollingRestartsInterval) {
  this->rtc_.set_alarm_enabled(true);
  this->rtc_.set_event_poll_interval(1);
  ASSERT_TRUE(this->rtc_.set_alarm(0, 0, 0, 0xFF, 0xFF));
  this->rtc_.setup();
  unsigned calls = 0;
  this->rtc_.add_on_alarm_callback([this, &calls]() {
    calls++;
    EXPECT_TRUE(this->rtc_.clear_alarm_flag());
    EXPECT_TRUE(this->rtc_.unset_alarm());
  });
  this->bus_.regs[FLAG] = 0x08;
  delay(20);
  App.scheduler.call(millis());
  EXPECT_EQ(calls, 1);
  ASSERT_TRUE(this->rtc_.set_alarm(0, 1, 0, 0xFF, 0xFF));
  this->bus_.regs[FLAG] = 0x08;
  delay(20);
  App.scheduler.call(millis());
  EXPECT_EQ(calls, 2);
}

TEST_F(RX8111Test, EventPollWarningRecoversButCalendarWarningRemains) {
  this->rtc_.set_timer_count(1);
  this->rtc_.set_timer_configured(true);
  this->rtc_.set_timer_enabled(true);
  this->rtc_.setup();
  // Provide a valid calendar so a failed FLAG poll is the only warning source.
  this->rtc_.write_time();
  this->rtc_.read_time();
  this->bus_.fail_read = FLAG;
  this->rtc_.update();
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->bus_.fail_read = -1;
  this->rtc_.update();
  EXPECT_FALSE(this->rtc_.status_has_warning());
  this->bus_.regs[FLAG] = 0x80;
  this->rtc_.update();
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->bus_.fail_read = FLAG;
  this->rtc_.update();
  this->bus_.fail_read = -1;
  // Event polling succeeds, but the invalid calendar still owns the warning.
  this->rtc_.update();
  EXPECT_TRUE(this->rtc_.status_has_warning());
}

TEST_F(RX8111Test, TrustedWriteClearsCalendarWarningButNotFailedEventPoll) {
  this->rtc_.set_alarm_enabled(true);
  this->rtc_.set_event_poll_interval(1);
  ASSERT_TRUE(this->rtc_.set_alarm(0, 0, 0, 0xFF, 0xFF));
  this->rtc_.setup();
  this->bus_.regs[FLAG] = 0x80;  // Calendar is invalid until trusted write.
  this->rtc_.read_time();
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->bus_.fail_read = FLAG;
  delay(20);
  App.scheduler.call(millis());  // Failed FLAG poll while calendar owns the warning.
  this->bus_.fail_read = -1;
  this->rtc_.write_time();
  EXPECT_TRUE(this->rtc_.status_has_warning());  // Outstanding poll failure survives the write.
  this->bus_.fail_read = FLAG;
  delay(20);
  App.scheduler.call(millis());  // Another failed FLAG poll, after trusted write.
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->bus_.fail_read = -1;
  delay(20);
  App.scheduler.call(millis());
  EXPECT_FALSE(this->rtc_.status_has_warning());
  this->bus_.regs[FLAG] = 0x80;
  this->rtc_.read_time();
  this->rtc_.write_time();
  EXPECT_FALSE(this->rtc_.status_has_warning());
  this->bus_.fail_read = FLAG;
  delay(20);
  App.scheduler.call(millis());  // New poll failure is visible without another calendar read.
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->bus_.fail_read = -1;
  delay(20);
  App.scheduler.call(millis());
  EXPECT_FALSE(this->rtc_.status_has_warning());
}

TEST_F(RX8111Test, OmittedTimerConfigurationLeavesBatteryBackedStateAlone) {
  this->bus_.regs[EXTENSION] = 0xD9;
  this->bus_.regs[CONTROL] = 0xB7;
  this->rtc_.setup();
  EXPECT_EQ(this->bus_.regs[EXTENSION], 0xD9);
  EXPECT_EQ(this->bus_.regs[CONTROL], 0xB7);
}

TEST_F(RX8111Test, StartTimerRestartsFullCountAndClearsOnlyStaleTimerFlag) {
  this->rtc_.set_timer_count(60);
  this->rtc_.set_timer_clock(2);
  this->rtc_.set_timer_configured(true);
  this->rtc_.set_timer_enabled(true);
  this->rtc_.setup();
  unsigned calls = 0;
  this->rtc_.add_on_timer_callback([&calls]() { calls++; });
  this->bus_.regs[FLAG] = 0x1C;
  ASSERT_TRUE(this->rtc_.stop_timer());
  this->bus_.writes.clear();
  ASSERT_TRUE(this->rtc_.start_timer());
  EXPECT_EQ(this->bus_.regs[FLAG], 0x0C);
  EXPECT_EQ(this->bus_.regs[0x1A], 60);
  EXPECT_EQ(this->bus_.regs[0x1B], 0);
  EXPECT_EQ(this->bus_.regs[0x1C], 0);
  EXPECT_EQ(this->bus_.regs[EXTENSION] & 0x10, 0x10);
  EXPECT_EQ(this->bus_.regs[CONTROL] & 0x10, 0x10);
  EXPECT_EQ(this->bus_.writes,
            (std::vector<uint8_t>{EXTENSION, CONTROL, TIMER_CONTROL, 0x1A, EXTENSION, FLAG, CONTROL, EXTENSION}));
  this->rtc_.update();
  EXPECT_EQ(calls, 0);
  this->bus_.regs[FLAG] |= 0x10;
  this->rtc_.update();
  EXPECT_EQ(calls, 1);
  ASSERT_TRUE(this->rtc_.stop_timer());
  ASSERT_TRUE(this->rtc_.start_timer());
  this->bus_.regs[FLAG] |= 0x10;
  this->rtc_.update();
  EXPECT_EQ(calls, 2);
}

TEST_F(RX8111Test, StartTimerAfterDisabledBootAndRejectsMissingOrFailedConfiguration) {
  this->rtc_.set_timer_count(2);
  this->rtc_.set_timer_configured(true);
  this->rtc_.set_timer_enabled(false);
  this->rtc_.set_event_poll_interval(1);
  this->rtc_.setup();
  unsigned calls = 0;
  this->rtc_.add_on_timer_callback([&calls]() { calls++; });
  this->bus_.regs[FLAG] = 0x10;
  ASSERT_TRUE(this->rtc_.start_timer());
  EXPECT_EQ(this->bus_.regs[FLAG] & 0x10, 0);
  EXPECT_EQ(this->bus_.regs[EXTENSION] & 0x10, 0x10);
  this->bus_.regs[FLAG] = 0x10;
  App.scheduler.call(millis());
  delay(20);
  App.scheduler.call(millis());
  EXPECT_EQ(calls, 1);
  ASSERT_TRUE(this->rtc_.stop_timer());
  this->bus_.fail_write = FLAG;
  EXPECT_FALSE(this->rtc_.start_timer());
  EXPECT_TRUE(this->rtc_.status_has_warning());
  EXPECT_EQ(this->bus_.regs[EXTENSION] & 0x10, 0);
  EXPECT_EQ(this->bus_.regs[CONTROL] & 0x10, 0);
  this->bus_.fail_write = -1;
  EXPECT_TRUE(this->rtc_.start_timer());
}

TEST_F(RX8111Test, StartTimerWithoutCountDoesNotTouchHardware) {
  this->rtc_.setup();
  this->rtc_.status_clear_warning();
  this->bus_.writes.clear();
  EXPECT_FALSE(this->rtc_.start_timer());
  EXPECT_FALSE(this->rtc_.status_has_warning());
  EXPECT_TRUE(this->bus_.writes.empty());
}

TEST_F(RX8111Test, InitialTimeSyncAlarmActionOverridesBootAlarm) {
  this->rtc_.write_time();
  this->rtc_.set_alarm_enabled(true);
  this->rtc_.set_alarm_second(5);
  unsigned actions = 0;
  this->rtc_.add_on_time_sync_callback([this, &actions]() {
    actions++;
    EXPECT_TRUE(this->rtc_.set_alarm(12, 34, 6, 0xFF, 0xFF));
  });
  this->rtc_.setup();
  EXPECT_EQ(actions, 1);
  EXPECT_EQ(this->bus_.regs[SEC_ALARM], 0x12);
  EXPECT_EQ(this->bus_.regs[MIN_ALARM], 0x34);
  EXPECT_EQ(this->bus_.regs[CONTROL] & 0x08, 0x08);
}

TEST_F(RX8111Test, InitialTimeSyncUnsetAlarmActionOverridesBootAlarm) {
  this->rtc_.write_time();
  this->rtc_.set_alarm_enabled(true);
  this->rtc_.set_alarm_second(5);
  unsigned actions = 0;
  this->rtc_.add_on_time_sync_callback([this, &actions]() {
    actions++;
    EXPECT_TRUE(this->rtc_.unset_alarm());
  });
  this->rtc_.setup();
  EXPECT_EQ(actions, 1);
  EXPECT_EQ(this->bus_.regs[SEC_ALARM], 0x80);
  EXPECT_EQ(this->bus_.regs[CONTROL] & 0x08, 0);
}

TEST_F(RX8111Test, BootAlarmIsPreservedAndLatchedEventIsDelivered) {
  this->bus_.regs[CONTROL] = 0x08;
  this->bus_.regs[FLAG] = 0x08;
  this->rtc_.set_alarm_enabled(true);
  unsigned calls = 0;
  this->rtc_.add_on_alarm_callback([this, &calls]() {
    calls++;
    EXPECT_TRUE(this->rtc_.clear_alarm_flag());
  });
  this->rtc_.setup();
  EXPECT_EQ(this->bus_.regs[CONTROL] & 0x08, 0x08);
  this->rtc_.update();
  EXPECT_EQ(calls, 1);
  this->bus_.regs[FLAG] |= 0x08;
  this->rtc_.update();
  EXPECT_EQ(calls, 2);
}

TEST_F(RX8111Test, BootAlarmRetriesPartialWriteAndPreservesLatchedFlag) {
  this->rtc_.set_alarm_enabled(true);
  this->rtc_.set_alarm_second(5);
  this->bus_.regs[FLAG] = 0x88;
  this->bus_.fail_write = MIN_ALARM;
  unsigned calls = 0;
  this->rtc_.add_on_alarm_callback([&calls]() { calls++; });
  this->rtc_.setup();
  EXPECT_TRUE(this->rtc_.status_has_warning());
  EXPECT_EQ(this->bus_.regs[CONTROL] & 0x08, 0);
  this->bus_.fail_write = -1;
  this->advance_retry_();
  EXPECT_EQ(this->bus_.regs[CONTROL] & 0x08, 0x08);
  EXPECT_EQ(this->bus_.regs[FLAG] & 0x08, 0x08);
  EXPECT_TRUE(this->rtc_.status_has_warning());  // POR still owns the calendar warning.
  this->rtc_.update();
  EXPECT_EQ(calls, 1);
}

TEST_F(RX8111Test, BootAlarmRetryDoesNotRedeliverWakeFlag) {
  this->rtc_.set_alarm_enabled(true);
  this->rtc_.set_alarm_second(5);
  this->bus_.regs[CONTROL] = 0x08;
  this->bus_.regs[FLAG] = 0x08;
  this->bus_.fail_write = CONTROL;
  unsigned calls = 0;
  this->rtc_.add_on_alarm_callback([&calls]() { calls++; });
  this->rtc_.setup();
  this->bus_.fail_write = -1;
  this->advance_retry_();
  this->rtc_.update();
  this->rtc_.update();
  EXPECT_EQ(calls, 1);
}

TEST_F(RX8111Test, BootAlarmRetryNeverHidesFirstWakeFlag) {
  this->rtc_.set_alarm_enabled(true);
  this->rtc_.set_alarm_second(5);
  this->bus_.regs[FLAG] = 0x08;
  this->bus_.fail_write = MIN_ALARM;
  unsigned calls = 0;
  this->rtc_.add_on_alarm_callback([&calls]() { calls++; });
  this->rtc_.setup();
  this->bus_.fail_write = -1;
  this->advance_retry_();
  this->rtc_.update();
  EXPECT_EQ(calls, 1);
}

TEST_F(RX8111Test, AlarmWakeFlagObservedBeforeRetryDoesNotRepeat) {
  this->rtc_.set_alarm_enabled(true);
  this->rtc_.set_alarm_second(5);
  ASSERT_TRUE(this->rtc_.set_alarm(5, 0xFF, 0xFF, 0xFF, 0xFF));
  this->bus_.regs[FLAG] = 0x08;
  this->bus_.fail_read = EXTENSION;
  unsigned calls = 0;
  this->rtc_.add_on_alarm_callback([&calls]() { calls++; });
  this->rtc_.setup();
  this->rtc_.update();
  EXPECT_EQ(calls, 1);
  this->bus_.fail_read = -1;
  this->advance_retry_();
  this->rtc_.update();
  EXPECT_EQ(calls, 1);
}

TEST_F(RX8111Test, BootAlarmRetriesExhaustAndCalendarReadDoesNotHideFailure) {
  this->rtc_.write_time();
  this->rtc_.set_alarm_enabled(true);
  this->rtc_.set_alarm_second(5);
  this->bus_.fail_write = MIN_ALARM;
  this->rtc_.setup();
  this->rtc_.update();
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->advance_retry_();
  this->advance_retry_();
  this->bus_.fail_write = -1;
  this->advance_retry_();
  EXPECT_EQ(this->bus_.regs[CONTROL] & 0x08, 0);
  EXPECT_TRUE(this->rtc_.status_has_warning());
}

TEST_F(RX8111Test, BootAlarmRetryClearsOnlyItsOwnWarning) {
  this->rtc_.write_time();
  this->rtc_.set_alarm_enabled(true);
  this->rtc_.set_alarm_second(5);
  this->bus_.fail_write = MIN_ALARM;
  this->rtc_.setup();
  this->bus_.fail_write = -1;
  this->rtc_.read_time();
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->advance_retry_();
  EXPECT_FALSE(this->rtc_.status_has_warning());
  this->bus_.regs[FLAG] = 0x80;
  this->rtc_.read_time();
  this->bus_.fail_write = MIN_ALARM;
  // A runtime failure must not consume another boot retry or erase the calendar warning.
  EXPECT_FALSE(this->rtc_.set_alarm(6, 0xFF, 0xFF, 0xFF, 0xFF));
  this->bus_.fail_write = -1;
  this->advance_retry_();
  EXPECT_TRUE(this->rtc_.status_has_warning());
}

TEST_F(RX8111Test, RuntimeAlarmClearsSupersededBootWarningAndCancelsRetry) {
  this->rtc_.write_time();
  this->rtc_.set_alarm_enabled(true);
  this->rtc_.set_alarm_second(5);
  this->bus_.fail_write = MIN_ALARM;
  this->rtc_.setup();
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->bus_.fail_write = -1;
  ASSERT_TRUE(this->rtc_.set_alarm(6, 0xFF, 0xFF, 0xFF, 0xFF));
  EXPECT_FALSE(this->rtc_.status_has_warning());
  this->bus_.writes.clear();
  this->advance_retry_();
  EXPECT_TRUE(this->bus_.writes.empty());
}

TEST_F(RX8111Test, RelativeAlarmClearsSupersededBootWarningAndCancelsRetry) {
  this->rtc_.write_time();
  this->rtc_.set_alarm_enabled(true);
  this->rtc_.set_alarm_second(5);
  this->bus_.fail_write = MIN_ALARM;
  this->rtc_.setup();
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->bus_.fail_write = -1;
  ASSERT_TRUE(this->rtc_.schedule_alarm_in(60));
  EXPECT_FALSE(this->rtc_.status_has_warning());
  this->bus_.writes.clear();
  this->advance_retry_();
  EXPECT_TRUE(this->bus_.writes.empty());
}

TEST_F(RX8111Test, RuntimeAlarmClearsOnlyBootAlarmWarning) {
  this->rtc_.write_time();
  this->rtc_.set_alarm_enabled(true);
  this->rtc_.set_alarm_second(5);
  this->rtc_.set_timer_count(2);
  this->rtc_.set_timer_configured(true);
  this->rtc_.set_timer_enabled(true);
  this->bus_.fail_read = EXTENSION;
  this->rtc_.setup();
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->bus_.fail_read = -1;
  ASSERT_TRUE(this->rtc_.set_alarm(6, 0xFF, 0xFF, 0xFF, 0xFF));
  EXPECT_TRUE(this->rtc_.status_has_warning());  // The timer still owns a boot warning.
  this->advance_retry_();
  EXPECT_FALSE(this->rtc_.status_has_warning());
}

TEST_F(RX8111Test, BootAlarmAndTimerHaveIndependentRetryBudgets) {
  this->rtc_.write_time();
  this->rtc_.set_alarm_enabled(true);
  this->rtc_.set_alarm_second(5);
  this->rtc_.set_timer_count(2);
  this->rtc_.set_timer_configured(true);
  this->rtc_.set_timer_enabled(true);
  this->bus_.fail_read = EXTENSION;
  this->rtc_.setup();
  this->bus_.fail_read = -1;
  this->advance_retry_();
  EXPECT_EQ(this->bus_.regs[CONTROL] & 0x18, 0x18);
  EXPECT_FALSE(this->rtc_.status_has_warning());
}

TEST_F(RX8111Test, TimerPollBeforeRetainedAlarmInspectionRetryDeliversWakeFlag) {
  this->rtc_.write_time();
  this->rtc_.set_alarm_enabled(true);
  this->rtc_.set_timer_count(2);
  this->rtc_.set_timer_configured(true);
  this->rtc_.set_timer_enabled(true);
  this->bus_.regs[CONTROL] = 0x08;
  this->bus_.fail_read_once = CONTROL;
  this->rtc_.setup();
  unsigned calls = 0;
  this->rtc_.add_on_alarm_callback([&calls]() { calls++; });
  this->bus_.regs[FLAG] = 0x08;
  this->rtc_.update();  // Timer polling must not consume an alarm that is not armed yet.
  EXPECT_EQ(calls, 0);
  this->advance_retry_();
  this->rtc_.update();
  this->rtc_.update();
  EXPECT_EQ(calls, 1);
}

TEST_F(RX8111Test, AlarmPollBeforeDisabledTimerStopRetryDeliversRetainedFlag) {
  this->rtc_.write_time();
  this->rtc_.set_alarm_enabled(true);
  this->bus_.regs[CONTROL] = 0x08;
  this->rtc_.set_timer_count(2);
  this->rtc_.set_timer_configured(true);
  this->rtc_.set_timer_enabled(false);
  this->bus_.fail_read_once = EXTENSION;
  this->rtc_.setup();
  unsigned calls = 0;
  this->rtc_.add_on_timer_callback([&calls]() { calls++; });
  this->bus_.regs[FLAG] = 0x10;
  this->rtc_.update();  // Alarm polling must not consume an ineligible timer flag.
  EXPECT_EQ(calls, 0);
  this->advance_retry_();
  this->rtc_.update();
  this->rtc_.update();
  EXPECT_EQ(calls, 1);
}

TEST_F(RX8111Test, RetainedAlarmInspectionRetriesAndDeliversWakeFlag) {
  this->rtc_.write_time();
  this->rtc_.set_alarm_enabled(true);
  this->bus_.regs[CONTROL] = 0x08;
  this->bus_.fail_read = CONTROL;
  this->rtc_.setup();
  this->bus_.fail_read = -1;
  unsigned calls = 0;
  this->rtc_.add_on_alarm_callback([&calls]() { calls++; });
  this->bus_.regs[FLAG] = 0x08;
  this->advance_retry_();
  // Alarm recovery cannot clear the calendar's independent failed CONTROL-read warning.
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->rtc_.update();
  EXPECT_FALSE(this->rtc_.status_has_warning());
  this->rtc_.update();
  EXPECT_EQ(calls, 1);
}

TEST_F(RX8111Test, BootRetryDoesNotClearOutstandingEventPollWarning) {
  this->rtc_.write_time();
  this->rtc_.set_alarm_enabled(true);
  this->rtc_.set_alarm_second(5);
  this->bus_.fail_write = MIN_ALARM;
  this->rtc_.setup();
  this->bus_.fail_read = FLAG;
  this->rtc_.update();
  this->bus_.fail_read = -1;
  this->bus_.fail_write = -1;
  this->advance_retry_();
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->rtc_.update();
  EXPECT_FALSE(this->rtc_.status_has_warning());
}

TEST_F(RX8111Test, BootTimerRetriesPartialWriteAndRetainedFlagOnce) {
  this->rtc_.set_timer_count(2);
  this->rtc_.set_timer_configured(true);
  this->rtc_.set_timer_enabled(true);
  this->bus_.regs[FLAG] = 0x10;
  this->bus_.fail_write = 0x1A;
  this->rtc_.setup();
  this->bus_.fail_write = -1;
  unsigned calls = 0;
  this->rtc_.add_on_timer_callback([&calls]() { calls++; });
  this->advance_retry_();
  EXPECT_EQ(this->bus_.regs[EXTENSION] & 0x10, 0x10);
  EXPECT_EQ(this->bus_.regs[FLAG] & 0x10, 0x10);
  this->rtc_.update();
  this->rtc_.update();
  EXPECT_EQ(calls, 1);
}

TEST_F(RX8111Test, ExplicitBootDisableRetriesAndRuntimeRequestSupersedesRetry) {
  this->rtc_.set_alarm_enabled(false);
  this->rtc_.set_alarm_explicitly_configured(true);
  this->bus_.regs[CONTROL] = 0x08;
  this->bus_.fail_write = CONTROL;
  this->rtc_.setup();
  this->bus_.fail_write = -1;
  this->advance_retry_();
  EXPECT_EQ(this->bus_.regs[CONTROL] & 0x08, 0);
  this->rtc_.set_timer_count(2);
  this->rtc_.set_timer_configured(true);
  this->rtc_.set_timer_enabled(false);
  this->bus_.fail_read = EXTENSION;
  this->rtc_.setup();
  this->bus_.fail_read = -1;
  ASSERT_TRUE(this->rtc_.start_timer());
  this->bus_.writes.clear();
  this->advance_retry_();
  EXPECT_TRUE(this->bus_.writes.empty());
  EXPECT_EQ(this->bus_.regs[EXTENSION] & 0x10, 0x10);
}

TEST_F(RX8111Test, AlarmAndTimerFlagsAreReadTogetherAndCallbacksCanClearBoth) {
  this->rtc_.set_alarm_enabled(true);
  this->rtc_.set_timer_enabled(true);
  this->rtc_.set_timer_configured(true);
  this->rtc_.set_timer_count(2);
  ASSERT_TRUE(this->rtc_.set_alarm(0, 0, 0, 0xFF, 0xFF));
  this->rtc_.setup();
  unsigned alarms = 0;
  unsigned timers = 0;
  this->rtc_.add_on_alarm_callback([this, &alarms]() {
    alarms++;
    EXPECT_TRUE(this->rtc_.clear_alarm_flag());
  });
  this->rtc_.add_on_timer_callback([this, &timers]() {
    timers++;
    EXPECT_TRUE(this->rtc_.clear_timer_flag());
  });
  this->bus_.regs[FLAG] = 0x18;
  this->rtc_.update();
  EXPECT_EQ(alarms, 1);
  EXPECT_EQ(timers, 1);
  EXPECT_EQ(this->bus_.regs[FLAG], 0);
  this->bus_.regs[FLAG] = 0x18;
  this->rtc_.update();
  EXPECT_EQ(alarms, 2);
  EXPECT_EQ(timers, 2);
}

TEST_F(RX8111Test, AlarmCallbackCanStopTimerBeforeTimerCallback) {
  this->rtc_.set_alarm_enabled(true);
  this->rtc_.set_timer_enabled(true);
  this->rtc_.set_timer_configured(true);
  this->rtc_.set_timer_count(2);
  ASSERT_TRUE(this->rtc_.set_alarm(0, 0, 0, 0xFF, 0xFF));
  this->rtc_.setup();
  unsigned timers = 0;
  this->rtc_.add_on_alarm_callback([this]() { EXPECT_TRUE(this->rtc_.stop_timer()); });
  this->rtc_.add_on_timer_callback([&timers]() { timers++; });
  this->bus_.regs[FLAG] = 0x18;
  this->rtc_.update();
  EXPECT_EQ(timers, 0);
  EXPECT_EQ(this->bus_.regs[EXTENSION] & 0x10, 0);
  EXPECT_EQ(this->bus_.regs[CONTROL] & 0x10, 0);
}

TEST_F(RX8111Test, AlarmCallbackRestartInvalidatesPendingTimerEvent) {
  this->rtc_.set_alarm_enabled(true);
  this->rtc_.set_timer_enabled(true);
  this->rtc_.set_timer_configured(true);
  this->rtc_.set_timer_count(2);
  ASSERT_TRUE(this->rtc_.set_alarm(0, 0, 0, 0xFF, 0xFF));
  this->rtc_.setup();
  unsigned timers = 0;
  this->rtc_.add_on_alarm_callback([this]() { EXPECT_TRUE(this->rtc_.start_timer()); });
  this->rtc_.add_on_timer_callback([&timers]() { timers++; });
  this->bus_.regs[FLAG] = 0x18;
  this->rtc_.update();
  EXPECT_EQ(timers, 0);
  EXPECT_EQ(this->bus_.regs[FLAG] & 0x10, 0);
  this->bus_.regs[FLAG] |= 0x10;
  this->rtc_.update();
  EXPECT_EQ(timers, 1);
}

TEST_F(RX8111Test, TimerCallbackCanClearTFAndFailedProgrammingDoesNotStartTimer) {
  this->rtc_.set_timer_enabled(true);
  this->rtc_.set_timer_configured(true);
  this->rtc_.set_timer_count(1);
  this->bus_.fail_write = 0x1A;
  this->rtc_.setup();
  EXPECT_EQ(this->bus_.regs[EXTENSION] & 0x10, 0);
  EXPECT_EQ(this->bus_.regs[CONTROL] & 0x10, 0);
  EXPECT_TRUE(this->rtc_.status_has_warning());

  this->bus_.fail_write = -1;
  this->bus_.regs[FLAG] = 0;
  unsigned calls = 0;
  this->rtc_.add_on_timer_callback([this, &calls]() {
    calls++;
    EXPECT_TRUE(this->rtc_.clear_timer_flag());
  });
  this->rtc_.update();
  this->bus_.regs[FLAG] = 0x10;
  this->rtc_.update();
  EXPECT_EQ(calls, 1);
  EXPECT_EQ(this->bus_.regs[FLAG], 0);
  this->bus_.regs[FLAG] = 0x10;
  this->rtc_.update();
  EXPECT_EQ(calls, 2);
}

TEST_F(RX8111Test, StopTimerOutsideCallbackCancelsEventPolling) {
  this->rtc_.set_timer_enabled(true);
  this->rtc_.set_timer_configured(true);
  this->rtc_.set_timer_count(2);
  this->rtc_.set_event_poll_interval(1);
  this->rtc_.setup();
  unsigned calls = 0;
  this->rtc_.add_on_timer_callback([&calls]() { calls++; });
  App.scheduler.call(millis());
  delay(20);
  App.scheduler.call(millis());
  EXPECT_GT(this->bus_.flag_reads, 0);
  EXPECT_TRUE(this->polling_started_());
  ASSERT_TRUE(this->rtc_.stop_timer());
  delay(20);
  App.scheduler.call(millis());
  EXPECT_FALSE(this->polling_started_());
  const unsigned flag_reads = this->bus_.flag_reads;
  delay(20);
  App.scheduler.call(millis());
  EXPECT_EQ(this->bus_.flag_reads, flag_reads);
  ASSERT_TRUE(this->rtc_.start_timer());
  EXPECT_TRUE(this->polling_started_());
  this->bus_.regs[FLAG] = 0x10;
  delay(20);
  App.scheduler.call(millis());
  EXPECT_EQ(calls, 1);
}

TEST_F(RX8111Test, FailedRestartWithUnreadableBusKeepsRunningTimerWatched) {
  this->rtc_.set_timer_enabled(true);
  this->rtc_.set_timer_configured(true);
  this->rtc_.set_timer_count(2);
  this->rtc_.setup();
  EXPECT_EQ(this->bus_.regs[EXTENSION] & 0x10, 0x10);
  unsigned calls = 0;
  this->rtc_.add_on_timer_callback([&calls]() { calls++; });
  this->bus_.fail_read = EXTENSION;
  EXPECT_FALSE(this->rtc_.start_timer());
  EXPECT_TRUE(this->rtc_.status_has_warning());
  EXPECT_TRUE(this->timer_enabled_());
  EXPECT_EQ(this->bus_.regs[EXTENSION] & 0x10, 0x10);
  this->bus_.fail_read = -1;
  this->bus_.regs[FLAG] = 0x10;
  this->rtc_.update();
  EXPECT_EQ(calls, 1);
}

}  // namespace esphome::rx8111::testing
