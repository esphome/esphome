#include <gtest/gtest.h>

#include <string>

#include "esphome/components/atm90e32/atm90e32.h"
#ifdef USE_HOST
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include "esphome/components/host/preferences.h"
#endif
#include "esphome/components/logger/logger.h"

namespace esphome::atm90e32::testing {

TEST(ATM90E32OffsetRegisterVerification, AcceptsExactSignedReadback) {
  EXPECT_TRUE(offset_register_value_matches(0x007B, 123));
  EXPECT_TRUE(offset_register_value_matches(0xFF85, -123));
}

TEST(ATM90E32OffsetRegisterVerification, RejectsMismatchedReadback) {
  EXPECT_FALSE(offset_register_value_matches(0x007C, 123));
  EXPECT_FALSE(offset_register_value_matches(0xFF84, -123));
}

TEST(ATM90E32OffsetRestoreState, ReportsVerifiedStoredValuesAsRestored) {
  const auto state = resolve_calibration_restore_state(true, true, false);

  EXPECT_TRUE(state.restored);
  EXPECT_TRUE(state.values_verified);
}

TEST(ATM90E32OffsetRestoreState, ReportsVerifiedConfigFallbackAsNotRestored) {
  const auto state = resolve_calibration_restore_state(true, false, true);

  EXPECT_FALSE(state.restored);
  EXPECT_TRUE(state.values_verified);
}

TEST(ATM90E32OffsetRestoreState, ReportsFailedConfigFallbackAsUnverified) {
  const auto state = resolve_calibration_restore_state(true, false, false);

  EXPECT_FALSE(state.restored);
  EXPECT_FALSE(state.values_verified);
}

TEST(ATM90E32OffsetRestoreState, ReportsConfigWithoutStoredValuesAsNotRestored) {
  const auto state = resolve_calibration_restore_state(false, true, false);

  EXPECT_FALSE(state.restored);
  EXPECT_TRUE(state.values_verified);
}

TEST(ATM90E32OffsetPersistence, RollsBackStoredValuesOrZeroSentinel) {
  const OffsetCalibration previous[3]{{1, -1}, {2, -2}, {3, -3}};
  OffsetCalibration rollback[3]{};

  prepare_calibration_rollback(previous, true, OffsetCalibration{}, rollback);
  for (uint8_t phase = 0; phase < 3; phase++) {
    EXPECT_EQ(rollback[phase].first_offset, previous[phase].first_offset);
    EXPECT_EQ(rollback[phase].second_offset, previous[phase].second_offset);
  }

  prepare_calibration_rollback(previous, false, OffsetCalibration{}, rollback);
  for (const auto &phase : rollback) {
    EXPECT_EQ(phase.first_offset, 0);
    EXPECT_EQ(phase.second_offset, 0);
  }
}

TEST(ATM90E32GainPersistence, RollsBackStoredValuesOrZeroSentinel) {
  const GainCalibration previous[3]{{101, 201}, {102, 202}, {103, 203}};
  GainCalibration rollback[3]{};

  prepare_calibration_rollback(previous, true, GainCalibration{0, 0}, rollback);
  for (uint8_t phase = 0; phase < 3; phase++) {
    EXPECT_EQ(rollback[phase].voltage_gain, previous[phase].voltage_gain);
    EXPECT_EQ(rollback[phase].current_gain, previous[phase].current_gain);
  }

  prepare_calibration_rollback(previous, false, GainCalibration{0, 0}, rollback);
  for (const auto &phase : rollback) {
    EXPECT_EQ(phase.voltage_gain, 0);
    EXPECT_EQ(phase.current_gain, 0);
  }
}

class FailedGainRollbackDelegate final : public spi::SPIDelegate {
 public:
  uint8_t transfer(uint8_t data) override { return 0; }
};

class ATM90E32GainCalibrationTest : public ::testing::Test {
 protected:
  void check_failed_rollback_() {
    for (const bool mismatch : {false, true}) {
      for (uint8_t offsets = 0; offsets < 4; offsets++) {
        SCOPED_TRACE(::testing::Message() << "mismatch=" << mismatch << ", offsets=" << +offsets);
        FailedGainRollbackDelegate delegate;
        ATM90E32Component component;
        component.delegate_ = &delegate;
        component.instance_id_ = "test";
        component.enable_gain_calibration_ = true;
        component.enable_offset_calibration_ = true;
        component.restored_gain_calibration_ = true;
        component.has_stored_gain_calibration_ = true;
        component.using_saved_calibrations_ = true;
        component.restored_offset_calibration_ = (offsets & 1) != 0;
        component.restored_power_offset_calibration_ = (offsets & 2) != 0;
        for (bool &phase : component.gain_calibration_mismatch_)
          phase = mismatch;
        const GainCalibration previous[3]{{7305, 27518}, {7305, 27518}, {7305, 27518}};

        component.finish_gain_calibration_(previous, true, true);

        EXPECT_FALSE(component.restored_gain_calibration_);
        EXPECT_TRUE(component.has_stored_gain_calibration_);
        EXPECT_EQ(component.using_saved_calibrations_, offsets != 0);
        EXPECT_EQ(component.restored_offset_calibration_, (offsets & 1) != 0);
        EXPECT_EQ(component.restored_power_offset_calibration_, (offsets & 2) != 0);
        for (const bool phase : component.gain_calibration_mismatch_)
          EXPECT_FALSE(phase);

        const auto previous_baud_rate = logger::global_logger->get_baud_rate();
        ::testing::internal::CaptureStdout();
        logger::global_logger->set_baud_rate(115200);
        component.dump_config();
        logger::global_logger->set_baud_rate(previous_baud_rate);
        const auto output = ::testing::internal::GetCapturedStdout();
        EXPECT_EQ(output.find("Gain calibration loaded and verified successfully."), std::string::npos);
        EXPECT_EQ(output.find("Gain mismatch: using flash values"), std::string::npos);
        EXPECT_EQ(output.find("Restoring saved gain calibrations to registers"), std::string::npos);
      }
    }
  }
};

TEST_F(ATM90E32GainCalibrationTest, FailedRollbackDoesNotReportVerifiedGainsOnReconnect) {
  this->check_failed_rollback_();
}

#ifdef USE_HOST

namespace fs = std::filesystem;

class ScopedATM90E32Preferences {
 public:
  explicit ScopedATM90E32Preferences(bool fail_sync)
      : previous_preferences_(global_preferences), previous_host_preferences_(host::host_preferences) {
    const char *prefdir = getenv("ESPHOME_PREFDIR");
    if (prefdir != nullptr) {
      this->had_prefdir_ = true;
      this->previous_prefdir_ = prefdir;
    }

    static uint32_t test_id = 0;
    char directory[48];
    snprintf(directory, sizeof(directory), "esphome_atm90e32_%u", ++test_id);
    this->prefdir_ = fs::temp_directory_path() / directory;
    fs::create_directories(this->prefdir_);
    this->set_prefdir_(fail_sync);
    global_preferences = &this->preferences_;
    host::host_preferences = &this->preferences_;
  }

  ~ScopedATM90E32Preferences() {
    global_preferences = this->previous_preferences_;
    host::host_preferences = this->previous_host_preferences_;
    if (this->had_prefdir_) {
      setenv("ESPHOME_PREFDIR", this->previous_prefdir_.c_str(), 1);
    } else {
      unsetenv("ESPHOME_PREFDIR");
    }
    std::error_code ec;
    fs::remove_all(this->prefdir_, ec);
  }

  void allow_sync() { this->set_prefdir_(false); }

 private:
  void set_prefdir_(bool fail_sync) {
    if (fail_sync) {
      setenv("ESPHOME_PREFDIR", "/dev/null", 1);
    } else {
      setenv("ESPHOME_PREFDIR", this->prefdir_.string().c_str(), 1);
    }
  }

  host::HostPreferences preferences_;
  ESPPreferences *previous_preferences_;
  host::HostPreferences *previous_host_preferences_;
  fs::path prefdir_;
  std::string previous_prefdir_;
  bool had_prefdir_{false};
};

class CalibrationRegisterDelegate final : public spi::SPIDelegate {
 public:
  uint8_t transfer(uint8_t) override { return 0; }

  void transfer(uint8_t *data, size_t length) override {
    if (length != 4 || (data[0] & 0x80) == 0)
      return;

    const uint16_t address = static_cast<uint16_t>(((data[0] & 0x03) << 8) | data[1]);
    const uint16_t value = this->registers_[address];
    data[2] = static_cast<uint8_t>(value >> 8);
    data[3] = static_cast<uint8_t>(value);
  }

  void write_array(const uint8_t *data, size_t length) override {
    if (length != 4)
      return;

    const uint16_t address = static_cast<uint16_t>(((data[0] & 0x03) << 8) | data[1]);
    const uint16_t requested = static_cast<uint16_t>((data[2] << 8) | data[3]);
    uint16_t value = requested;
    if (this->is_offset_register_(address)) {
      if (this->drop_offset_writes_ > 0) {
        --this->drop_offset_writes_;
        this->registers_[ATM90E32_REGISTER_LASTSPIDATA] = requested;
        return;
      } else if (this->corrupt_offset_writes_) {
        value++;
      }
    }
    this->registers_[address] = value;
    this->registers_[ATM90E32_REGISTER_LASTSPIDATA] = requested;
  }

  void set_gain_values(uint8_t phase, uint16_t voltage, uint16_t current) {
    this->registers_[ATM90E32_REGISTER_UGAINA + 4 * phase] = voltage;
    this->registers_[ATM90E32_REGISTER_IGAINA + 4 * phase] = current;
  }

  void set_offset_values(OffsetCalibrationType type, uint8_t phase, int16_t first, int16_t second) {
    const auto &first_registers = type == OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_POWER
                                      ? power_offset_registers_
                                      : voltage_offset_registers_;
    const auto &second_registers = type == OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_POWER
                                       ? reactive_power_offset_registers_
                                       : current_offset_registers_;
    this->registers_[first_registers[phase]] = static_cast<uint16_t>(first);
    this->registers_[second_registers[phase]] = static_cast<uint16_t>(second);
  }

  void drop_next_offset_write() { this->drop_offset_writes_++; }
  void corrupt_offset_writes() { this->corrupt_offset_writes_ = true; }

 private:
  bool is_offset_register_(uint16_t address) const {
    for (uint8_t phase = 0; phase < 3; ++phase) {
      if (address == voltage_offset_registers_[phase] || address == current_offset_registers_[phase] ||
          address == power_offset_registers_[phase] || address == reactive_power_offset_registers_[phase]) {
        return true;
      }
    }
    return false;
  }

  static constexpr std::array<uint16_t, 3> voltage_offset_registers_{
      ATM90E32_REGISTER_UOFFSETA, ATM90E32_REGISTER_UOFFSETB, ATM90E32_REGISTER_UOFFSETC};
  static constexpr std::array<uint16_t, 3> current_offset_registers_{
      ATM90E32_REGISTER_IOFFSETA, ATM90E32_REGISTER_IOFFSETB, ATM90E32_REGISTER_IOFFSETC};
  static constexpr std::array<uint16_t, 3> power_offset_registers_{
      ATM90E32_REGISTER_POFFSETA, ATM90E32_REGISTER_POFFSETB, ATM90E32_REGISTER_POFFSETC};
  static constexpr std::array<uint16_t, 3> reactive_power_offset_registers_{
      ATM90E32_REGISTER_QOFFSETA, ATM90E32_REGISTER_QOFFSETB, ATM90E32_REGISTER_QOFFSETC};
  std::array<uint16_t, 256> registers_{};
  uint8_t drop_offset_writes_{0};
  bool corrupt_offset_writes_{false};
};

enum class CalibrationKind : uint8_t { GAIN, OFFSET, POWER_OFFSET };
enum class SavedValue : uint8_t { OLD, NEW, ZERO };

static uint32_t next_preference_key() {
  static uint32_t key = 0x600000;
  return key++;
}

static GainCalibration gain_values(SavedValue value, uint8_t phase) {
  switch (value) {
    case SavedValue::OLD:
      return {static_cast<uint16_t>(1000 + phase), static_cast<uint16_t>(2000 + phase)};
    case SavedValue::NEW:
      return {static_cast<uint16_t>(300 + phase), static_cast<uint16_t>(400 + phase)};
    case SavedValue::ZERO:
      return {0, 0};
  }
  return {0, 0};
}

static GainCalibration previous_gain_values(bool restored, uint8_t phase) {
  if (restored)
    return gain_values(SavedValue::OLD, phase);
  return {static_cast<uint16_t>(100 + phase), static_cast<uint16_t>(200 + phase)};
}

static OffsetCalibration offset_values(SavedValue value, uint8_t phase) {
  switch (value) {
    case SavedValue::OLD:
      return {static_cast<int16_t>(11 + phase), static_cast<int16_t>(-21 - phase)};
    case SavedValue::NEW:
      return {static_cast<int16_t>(31 + phase), static_cast<int16_t>(-41 - phase)};
    case SavedValue::ZERO:
      return {0, 0};
  }
  return {0, 0};
}

static OffsetCalibration previous_offset_values(bool restored, uint8_t phase) {
  if (restored)
    return offset_values(SavedValue::OLD, phase);
  return {static_cast<int16_t>(101 + phase), static_cast<int16_t>(-201 - phase)};
}

static ESPPreferenceObject make_calibration_preference(CalibrationKind kind, uint32_t key) {
  if (kind == CalibrationKind::GAIN)
    return global_preferences->make_preference<GainCalibration[3]>(key, true);
  if (kind == CalibrationKind::OFFSET)
    return global_preferences->make_preference<OffsetCalibration[3]>(key, true);
  return global_preferences->make_preference<OffsetCalibration[3]>(key, true);
}

static ESPPreferenceObject &component_preference(ATM90E32Component &component, CalibrationKind kind) {
  if (kind == CalibrationKind::GAIN)
    return component.gain_calibration_pref_;
  if (kind == CalibrationKind::OFFSET)
    return component.offset_pref_;
  return component.power_offset_pref_;
}

static void set_calibration_state(ATM90E32Component &component, CalibrationKind kind, bool has_stored, bool restored,
                                  bool using_saved) {
  if (kind == CalibrationKind::GAIN) {
    component.has_stored_gain_calibration_ = has_stored;
    component.restored_gain_calibration_ = restored;
  } else if (kind == CalibrationKind::OFFSET) {
    component.has_stored_offset_calibration_ = has_stored;
    component.restored_offset_calibration_ = restored;
  } else {
    component.has_stored_power_offset_calibration_ = has_stored;
    component.restored_power_offset_calibration_ = restored;
  }
  component.using_saved_calibrations_ = using_saved;
}

static void set_new_calibration(ATM90E32Component &component, CalibrationRegisterDelegate &delegate,
                                CalibrationKind kind) {
  if (kind == CalibrationKind::GAIN) {
    for (uint8_t phase = 0; phase < 3; ++phase) {
      const auto value = gain_values(SavedValue::NEW, phase);
      component.gain_phase_[phase] = value;
      delegate.set_gain_values(phase, value.voltage_gain, value.current_gain);
    }
    return;
  }

  const auto type = kind == CalibrationKind::OFFSET ? OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_VOLTAGE_CURRENT
                                                    : OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_POWER;
  for (uint8_t phase = 0; phase < 3; ++phase) {
    const auto value = offset_values(SavedValue::NEW, phase);
    component.write_offsets_to_registers_(phase, value.first_offset, value.second_offset, type);
    delegate.set_offset_values(type, phase, value.first_offset, value.second_offset);
  }
}

static void prepare_finish(ATM90E32Component &component, CalibrationRegisterDelegate &delegate, CalibrationKind kind,
                           uint32_t key) {
  component.delegate_ = &delegate;
  component.instance_id_ = "test";
  component_preference(component, kind) = make_calibration_preference(kind, key);
  set_new_calibration(component, delegate, kind);
}

static void save_values(ESPPreferenceObject &preference, CalibrationKind kind, SavedValue value) {
  if (kind == CalibrationKind::GAIN) {
    GainCalibration values[3];
    for (uint8_t phase = 0; phase < 3; ++phase)
      values[phase] = gain_values(value, phase);
    ASSERT_TRUE(preference.save(&values));
    return;
  }

  OffsetCalibration values[3];
  for (uint8_t phase = 0; phase < 3; ++phase)
    values[phase] = offset_values(value, phase);
  ASSERT_TRUE(preference.save(&values));
}

static void expect_values(ESPPreferenceObject &preference, CalibrationKind kind, SavedValue value) {
  if (kind == CalibrationKind::GAIN) {
    GainCalibration actual[3]{};
    ASSERT_TRUE(preference.load(&actual));
    for (uint8_t phase = 0; phase < 3; ++phase) {
      const auto expected = gain_values(value, phase);
      EXPECT_EQ(actual[phase].voltage_gain, expected.voltage_gain);
      EXPECT_EQ(actual[phase].current_gain, expected.current_gain);
    }
    return;
  }

  OffsetCalibration actual[3]{};
  ASSERT_TRUE(preference.load(&actual));
  for (uint8_t phase = 0; phase < 3; ++phase) {
    const auto expected = offset_values(value, phase);
    EXPECT_EQ(actual[phase].first_offset, expected.first_offset);
    EXPECT_EQ(actual[phase].second_offset, expected.second_offset);
  }
}

static void finish_calibration(ATM90E32Component &component, CalibrationKind kind, bool previous_restored,
                               bool previous_using_saved) {
  if (kind == CalibrationKind::GAIN) {
    GainCalibration previous[3];
    for (uint8_t phase = 0; phase < 3; ++phase)
      previous[phase] = previous_gain_values(previous_restored, phase);
    component.finish_gain_calibration_(previous, previous_restored, previous_using_saved);
    return;
  }

  OffsetCalibration previous[3];
  for (uint8_t phase = 0; phase < 3; ++phase)
    previous[phase] = previous_offset_values(previous_restored, phase);
  component.finish_offset_calibration_(previous, previous_restored, previous_using_saved,
                                       kind == CalibrationKind::OFFSET
                                           ? OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_VOLTAGE_CURRENT
                                           : OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_POWER);
}

template<typename F> static std::string capture_calibration_logs(F action) {
  const auto previous_baud_rate = logger::global_logger->get_baud_rate();
  ::testing::internal::CaptureStdout();
  logger::global_logger->set_baud_rate(115200);
  action();
  logger::global_logger->set_baud_rate(previous_baud_rate);
  return ::testing::internal::GetCapturedStdout();
}

static void expect_has_stored(const ATM90E32Component &component, CalibrationKind kind, bool expected) {
  if (kind == CalibrationKind::GAIN) {
    EXPECT_EQ(component.has_stored_gain_calibration_, expected);
  } else if (kind == CalibrationKind::OFFSET) {
    EXPECT_EQ(component.has_stored_offset_calibration_, expected);
  } else {
    EXPECT_EQ(component.has_stored_power_offset_calibration_, expected);
  }
}

static void expect_restored(const ATM90E32Component &component, CalibrationKind kind, bool expected) {
  if (kind == CalibrationKind::GAIN) {
    EXPECT_EQ(component.restored_gain_calibration_, expected);
  } else if (kind == CalibrationKind::OFFSET) {
    EXPECT_EQ(component.restored_offset_calibration_, expected);
  } else {
    EXPECT_EQ(component.restored_power_offset_calibration_, expected);
  }
}

static void expect_mismatches_cleared(const ATM90E32Component &component, CalibrationKind kind) {
  const bool *mismatches = kind == CalibrationKind::GAIN     ? component.gain_calibration_mismatch_
                           : kind == CalibrationKind::OFFSET ? component.offset_calibration_mismatch_
                                                             : component.power_offset_calibration_mismatch_;
  for (uint8_t phase = 0; phase < 3; ++phase)
    EXPECT_FALSE(mismatches[phase]);
}

static void expect_previous_runtime_values(const ATM90E32Component &component, CalibrationKind kind) {
  for (uint8_t phase = 0; phase < 3; ++phase) {
    if (kind == CalibrationKind::GAIN) {
      const auto expected = previous_gain_values(false, phase);
      EXPECT_EQ(component.gain_phase_[phase].voltage_gain, expected.voltage_gain);
      EXPECT_EQ(component.gain_phase_[phase].current_gain, expected.current_gain);
    } else {
      const auto expected = previous_offset_values(false, phase);
      const auto &actual =
          kind == CalibrationKind::OFFSET ? component.offset_phase_[phase] : component.power_offset_phase_[phase];
      EXPECT_EQ(actual.first_offset, expected.first_offset);
      EXPECT_EQ(actual.second_offset, expected.second_offset);
    }
  }
}

TEST(ATM90E32CalibrationPersistence, FailedPersistenceAfterConfigFallbackPreservesStoredPayload) {
  ScopedATM90E32Preferences preferences(true);
  for (const auto kind : {CalibrationKind::GAIN, CalibrationKind::OFFSET, CalibrationKind::POWER_OFFSET}) {
    ATM90E32Component component;
    CalibrationRegisterDelegate delegate;
    prepare_finish(component, delegate, kind, next_preference_key());
    auto &preference = component_preference(component, kind);
    save_values(preference, kind, SavedValue::OLD);
    set_calibration_state(component, kind, true, false, false);

    const auto output = capture_calibration_logs([&]() { finish_calibration(component, kind, false, false); });

    expect_values(preference, kind, SavedValue::OLD);
    expect_previous_runtime_values(component, kind);
    expect_has_stored(component, kind, true);
    expect_restored(component, kind, false);
    EXPECT_FALSE(component.using_saved_calibrations_);
    EXPECT_EQ(output.find("completed and verified"), std::string::npos);
  }
}

TEST(ATM90E32CalibrationPersistence, StoredPayloadLoadFailureRefusesOverwrite) {
  ScopedATM90E32Preferences preferences(false);
  for (const auto kind : {CalibrationKind::GAIN, CalibrationKind::OFFSET, CalibrationKind::POWER_OFFSET}) {
    ATM90E32Component component;
    CalibrationRegisterDelegate delegate;
    const uint32_t key = next_preference_key();
    prepare_finish(component, delegate, kind, key);
    auto raw_preference = global_preferences->make_preference<uint32_t>(key, true);
    constexpr uint32_t raw_value = 0xA5A55A5A;
    ASSERT_TRUE(raw_preference.save(&raw_value));
    ASSERT_TRUE(global_preferences->sync());
    set_calibration_state(component, kind, true, false, false);

    const auto output = capture_calibration_logs([&]() { finish_calibration(component, kind, false, false); });

    uint32_t actual = 0;
    ASSERT_TRUE(raw_preference.load(&actual));
    EXPECT_EQ(actual, raw_value);
    expect_has_stored(component, kind, true);
    expect_restored(component, kind, false);
    EXPECT_FALSE(component.using_saved_calibrations_);
    EXPECT_NE(output.find("Failed to load stored"), std::string::npos);
    EXPECT_EQ(output.find("completed and verified"), std::string::npos);
  }
}

TEST(ATM90E32CalibrationPersistence, FreshPersistenceUsesZeroSentinelAndCanRetry) {
  for (const auto kind : {CalibrationKind::GAIN, CalibrationKind::OFFSET, CalibrationKind::POWER_OFFSET}) {
    ScopedATM90E32Preferences preferences(true);
    ATM90E32Component component;
    CalibrationRegisterDelegate delegate;
    prepare_finish(component, delegate, kind, next_preference_key());
    set_calibration_state(component, kind, false, false, false);

    const auto output = capture_calibration_logs([&]() { finish_calibration(component, kind, false, false); });

    expect_values(component_preference(component, kind), kind, SavedValue::ZERO);
    expect_has_stored(component, kind, false);
    expect_restored(component, kind, false);
    EXPECT_FALSE(component.using_saved_calibrations_);
    EXPECT_EQ(output.find("completed and verified"), std::string::npos);

    preferences.allow_sync();
    set_new_calibration(component, delegate, kind);
    const auto retry_output = capture_calibration_logs([&]() { finish_calibration(component, kind, false, false); });

    expect_values(component_preference(component, kind), kind, SavedValue::NEW);
    expect_has_stored(component, kind, true);
    expect_restored(component, kind, true);
    EXPECT_TRUE(component.using_saved_calibrations_);
    EXPECT_NE(retry_output.find("completed and verified"), std::string::npos);
  }
}

static void check_offset_clear_retry(CalibrationKind kind, bool other_restored) {
  ScopedATM90E32Preferences preferences(true);
  ATM90E32Component component;
  CalibrationRegisterDelegate delegate;
  prepare_finish(component, delegate, kind, next_preference_key());
  auto &preference = component_preference(component, kind);
  save_values(preference, kind, SavedValue::OLD);
  set_calibration_state(component, kind, true, true, true);
  if (kind == CalibrationKind::OFFSET) {
    component.restored_power_offset_calibration_ = other_restored;
  } else {
    component.restored_offset_calibration_ = other_restored;
  }
  bool *mismatches = kind == CalibrationKind::OFFSET ? component.offset_calibration_mismatch_
                                                     : component.power_offset_calibration_mismatch_;
  mismatches[1] = true;

  const auto first_output = capture_calibration_logs([&]() {
    if (kind == CalibrationKind::OFFSET)
      component.clear_offset_calibrations();
    else
      component.clear_power_offset_calibrations();
  });

  expect_has_stored(component, kind, true);
  expect_restored(component, kind, false);
  EXPECT_EQ(component.using_saved_calibrations_, other_restored);
  expect_mismatches_cleared(component, kind);
  EXPECT_NE(first_output.find("Failed to clear"), std::string::npos);
  EXPECT_EQ(first_output.find("Offsets cleared."), std::string::npos);
  EXPECT_EQ(first_output.find("Power offsets cleared."), std::string::npos);

  preferences.allow_sync();
  const auto second_output = capture_calibration_logs([&]() {
    if (kind == CalibrationKind::OFFSET)
      component.clear_offset_calibrations();
    else
      component.clear_power_offset_calibrations();
  });

  expect_has_stored(component, kind, false);
  expect_restored(component, kind, false);
  EXPECT_EQ(component.using_saved_calibrations_, other_restored);
  expect_values(preference, kind, SavedValue::ZERO);
  EXPECT_EQ(second_output.find("Failed to clear"), std::string::npos);
  EXPECT_NE(second_output.find(kind == CalibrationKind::OFFSET ? "Offsets cleared." : "Power offsets cleared."),
            std::string::npos);
}

TEST(ATM90E32CalibrationPersistence, OffsetClearFailureRetainsRetryEligibility) {
  for (const auto kind : {CalibrationKind::OFFSET, CalibrationKind::POWER_OFFSET}) {
    check_offset_clear_retry(kind, false);
    check_offset_clear_retry(kind, true);
  }
}

static void check_offset_rollback_failure(OffsetCalibrationType type) {
  const auto kind = type == OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_VOLTAGE_CURRENT
                        ? CalibrationKind::OFFSET
                        : CalibrationKind::POWER_OFFSET;
  ScopedATM90E32Preferences preferences(true);
  ATM90E32Component component;
  CalibrationRegisterDelegate delegate;
  prepare_finish(component, delegate, kind, next_preference_key());
  auto &preference = component_preference(component, kind);
  save_values(preference, kind, SavedValue::OLD);
  set_calibration_state(component, kind, true, true, false);
  if (kind == CalibrationKind::OFFSET)
    component.restored_power_offset_calibration_ = true;
  else
    component.restored_offset_calibration_ = true;
  bool *mismatches = kind == CalibrationKind::OFFSET ? component.offset_calibration_mismatch_
                                                     : component.power_offset_calibration_mismatch_;
  for (uint8_t phase = 0; phase < 3; ++phase)
    mismatches[phase] = true;
  delegate.corrupt_offset_writes();

  const auto output = capture_calibration_logs([&]() { finish_calibration(component, kind, true, false); });

  expect_has_stored(component, kind, true);
  expect_restored(component, kind, false);
  EXPECT_TRUE(component.using_saved_calibrations_);
  expect_mismatches_cleared(component, kind);
  EXPECT_NE(output.find("rollback readback verification failed"), std::string::npos);
  EXPECT_EQ(output.find("completed and verified"), std::string::npos);
}

TEST(ATM90E32CalibrationPersistence, OffsetRollbackReadbackFailureInvalidatesState) {
  check_offset_rollback_failure(OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_VOLTAGE_CURRENT);
  check_offset_rollback_failure(OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_POWER);
}

TEST(ATM90E32CalibrationPersistence, OffsetRestoreFallbackPreservesOtherSavedCategories) {
  ScopedATM90E32Preferences preferences(false);
  ATM90E32Component component;
  CalibrationRegisterDelegate delegate;
  component.delegate_ = &delegate;
  component.instance_id_ = "test";
  component.offset_pref_ = global_preferences->make_preference<OffsetCalibration[3]>(next_preference_key(), true);
  save_values(component.offset_pref_, CalibrationKind::OFFSET, SavedValue::OLD);
  component.restored_power_offset_calibration_ = true;
  component.restored_gain_calibration_ = true;
  component.using_saved_calibrations_ = false;
  for (uint8_t phase = 0; phase < 3; ++phase) {
    component.set_voltage_offset(phase, static_cast<int16_t>(101 + phase));
    component.set_current_offset(phase, static_cast<int16_t>(-201 - phase));
  }
  delegate.drop_next_offset_write();

  const auto output = capture_calibration_logs([&]() {
    component.restore_offset_calibrations_(OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_VOLTAGE_CURRENT);
  });

  EXPECT_FALSE(component.restored_offset_calibration_);
  EXPECT_TRUE(component.restored_power_offset_calibration_);
  EXPECT_TRUE(component.restored_gain_calibration_);
  EXPECT_TRUE(component.using_saved_calibrations_);
  expect_mismatches_cleared(component, CalibrationKind::OFFSET);
  EXPECT_NE(output.find("config values verified"), std::string::npos);
}

#endif  // USE_HOST

}  // namespace esphome::atm90e32::testing
