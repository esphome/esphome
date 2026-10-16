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

class TestATM90E32Component : public ATM90E32Component {
 public:
  void spi_setup() override {}
  using ATM90E32Component::delegate_;
  using ATM90E32Component::enable_gain_calibration_;
  using ATM90E32Component::enable_offset_calibration_;
  using ATM90E32Component::finish_gain_calibration_;
  using ATM90E32Component::finish_offset_calibration_;
  using ATM90E32Component::gain_calibration_mismatch_;
  using ATM90E32Component::gain_calibration_pref_;
  using ATM90E32Component::gain_phase_;
  using ATM90E32Component::has_stored_gain_calibration_;
  using ATM90E32Component::has_stored_offset_calibration_;
  using ATM90E32Component::has_stored_power_offset_calibration_;
  using ATM90E32Component::instance_id_;
  using ATM90E32Component::offset_calibration_mismatch_;
  using ATM90E32Component::offset_phase_;
  using ATM90E32Component::offset_pref_;
  using ATM90E32Component::power_offset_calibration_mismatch_;
  using ATM90E32Component::power_offset_phase_;
  using ATM90E32Component::power_offset_pref_;
  using ATM90E32Component::read16_;
  using ATM90E32Component::restore_offset_calibrations_;
  using ATM90E32Component::restore_gain_calibrations_;
  using ATM90E32Component::restored_gain_calibration_;
  using ATM90E32Component::restored_offset_calibration_;
  using ATM90E32Component::restored_power_offset_calibration_;
  using ATM90E32Component::using_saved_calibrations_;
  using ATM90E32Component::write16_;
  using ATM90E32Component::update_using_saved_calibrations_;
  using ATM90E32Component::write_offsets_to_registers_;
};

TEST(ATM90E32OffsetRegisterVerification, AcceptsExactSignedReadback) {
  EXPECT_TRUE(offset_register_value_matches(0x007B, 123));
  EXPECT_TRUE(offset_register_value_matches(0xFF85, -123));
}

TEST(ATM90E32OffsetRegisterVerification, RejectsMismatchedReadback) {
  EXPECT_FALSE(offset_register_value_matches(0x007C, 123));
  EXPECT_FALSE(offset_register_value_matches(0xFF84, -123));
}

template<typename F> static std::string capture_calibration_logs(F action) {
  const auto previous_baud_rate = logger::global_logger->get_baud_rate();
  ::testing::internal::CaptureStdout();
  logger::global_logger->set_baud_rate(115200);
  action();
  logger::global_logger->set_baud_rate(previous_baud_rate);
  return ::testing::internal::GetCapturedStdout();
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
        TestATM90E32Component component;
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

        component.finish_gain_calibration_(previous, true);

        EXPECT_FALSE(component.restored_gain_calibration_);
        EXPECT_TRUE(component.has_stored_gain_calibration_);
        EXPECT_EQ(component.using_saved_calibrations_, offsets != 0);
        EXPECT_EQ(component.restored_offset_calibration_, (offsets & 1) != 0);
        EXPECT_EQ(component.restored_power_offset_calibration_, (offsets & 2) != 0);
        for (const bool phase : component.gain_calibration_mismatch_)
          EXPECT_FALSE(phase);

        const auto output = capture_calibration_logs([&]() { component.dump_config(); });
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
    if ((address == ATM90E32_REGISTER_SAGTH || address == ATM90E32_REGISTER_OVTH ||
         (address >= ATM90E32_REGISTER_UGAINA && address <= ATM90E32_REGISTER_IOFFSETC)) &&
        this->registers_[ATM90E32_REGISTER_CFGREGACCEN] != 0x55AA) {
      this->registers_[ATM90E32_REGISTER_LASTSPIDATA] = requested;
      return;
    }
    uint16_t value = requested;
    if (this->is_calibration_register_(address)) {
      if (this->drop_calibration_writes_ > 0) {
        --this->drop_calibration_writes_;
        this->registers_[ATM90E32_REGISTER_LASTSPIDATA] = requested;
        return;
      } else if (this->corrupt_calibration_writes_) {
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
                                      ? POWER_OFFSET_REGISTERS
                                      : VOLTAGE_OFFSET_REGISTERS;
    const auto &second_registers = type == OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_POWER
                                       ? REACTIVE_POWER_OFFSET_REGISTERS
                                       : CURRENT_OFFSET_REGISTERS;
    this->registers_[first_registers[phase]] = static_cast<uint16_t>(first);
    this->registers_[second_registers[phase]] = static_cast<uint16_t>(second);
  }

  void drop_next_calibration_write() { this->drop_calibration_writes_++; }
  void corrupt_calibration_writes() { this->corrupt_calibration_writes_ = true; }

 private:
  bool is_calibration_register_(uint16_t address) const {
    for (uint8_t phase = 0; phase < 3; ++phase) {
      if (address == ATM90E32_REGISTER_UGAINA + 4 * phase || address == ATM90E32_REGISTER_IGAINA + 4 * phase)
        return true;
    }
    for (uint8_t phase = 0; phase < 3; ++phase) {
      if (address == VOLTAGE_OFFSET_REGISTERS[phase] || address == CURRENT_OFFSET_REGISTERS[phase] ||
          address == POWER_OFFSET_REGISTERS[phase] || address == REACTIVE_POWER_OFFSET_REGISTERS[phase]) {
        return true;
      }
    }
    return false;
  }

  static constexpr std::array<uint16_t, 3> VOLTAGE_OFFSET_REGISTERS{
      ATM90E32_REGISTER_UOFFSETA, ATM90E32_REGISTER_UOFFSETB, ATM90E32_REGISTER_UOFFSETC};
  static constexpr std::array<uint16_t, 3> CURRENT_OFFSET_REGISTERS{
      ATM90E32_REGISTER_IOFFSETA, ATM90E32_REGISTER_IOFFSETB, ATM90E32_REGISTER_IOFFSETC};
  static constexpr std::array<uint16_t, 3> POWER_OFFSET_REGISTERS{
      ATM90E32_REGISTER_POFFSETA, ATM90E32_REGISTER_POFFSETB, ATM90E32_REGISTER_POFFSETC};
  static constexpr std::array<uint16_t, 3> REACTIVE_POWER_OFFSET_REGISTERS{
      ATM90E32_REGISTER_QOFFSETA, ATM90E32_REGISTER_QOFFSETB, ATM90E32_REGISTER_QOFFSETC};
  std::array<uint16_t, 1024> registers_{};
  uint8_t drop_calibration_writes_{0};
  bool corrupt_calibration_writes_{false};
};

TEST(ATM90E32RegisterAccess, HandlesFullTenBitAddress) {
  CalibrationRegisterDelegate delegate;
  TestATM90E32Component component;
  component.delegate_ = &delegate;
  component.set_instance_id("test");

  component.write16_(0x3FF, 0xABCD);

  EXPECT_EQ(component.read16_(0x3FF), 0xABCD);
}

TEST(ATM90E32Setup, ConfiguresGainsAndThresholdsWithCalibrationEnabledOrDisabled) {
  for (const bool offsets : {false, true}) {
    for (const bool gains : {false, true}) {
      SCOPED_TRACE(::testing::Message() << "offsets=" << offsets << ", gains=" << gains);
      ScopedATM90E32Preferences preferences(false);
      CalibrationRegisterDelegate delegate;
      TestATM90E32Component component;
      component.delegate_ = &delegate;
      spi::NullPin cs;
      component.set_cs_pin(&cs);
      component.set_instance_id("test");
      component.set_enable_offset_calibration(offsets);
      component.set_enable_gain_calibration(gains);
      for (uint8_t phase = 0; phase < 3; ++phase) {
        component.set_volt_gain(phase, 7305);
        component.set_ct_gain(phase, 27518);
      }

      component.setup();

      ASSERT_FALSE(component.is_failed());
      for (uint8_t phase = 0; phase < 3; ++phase) {
        EXPECT_EQ(component.read16_(component.voltage_gain_registers[phase]), 7305);
        EXPECT_EQ(component.read16_(component.current_gain_registers[phase]), 27518);
      }
      EXPECT_NE(component.read16_(ATM90E32_REGISTER_SAGTH), 0);
      EXPECT_NE(component.read16_(ATM90E32_REGISTER_OVTH), 0);
      EXPECT_EQ(component.read16_(ATM90E32_REGISTER_CFGREGACCEN), 0);
    }
  }
}

struct CalibrationTraits {
  const char *name;
  bool gain;
  ESPPreferenceObject TestATM90E32Component::*preference;
  bool TestATM90E32Component::*has_stored;
  bool TestATM90E32Component::*restored;
  bool (TestATM90E32Component::*mismatches)[3];
  OffsetCalibration (TestATM90E32Component::*offsets)[3];
  OffsetCalibrationType offset_type;
  void (ATM90E32Component::*clear)();
};

static constexpr CalibrationTraits CALIBRATIONS[]{
    {"Gain", true, &TestATM90E32Component::gain_calibration_pref_, &TestATM90E32Component::has_stored_gain_calibration_,
     &TestATM90E32Component::restored_gain_calibration_, &TestATM90E32Component::gain_calibration_mismatch_, nullptr,
     OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_VOLTAGE_CURRENT, &ATM90E32Component::clear_gain_calibrations},
    {"Offset", false, &TestATM90E32Component::offset_pref_, &TestATM90E32Component::has_stored_offset_calibration_,
     &TestATM90E32Component::restored_offset_calibration_, &TestATM90E32Component::offset_calibration_mismatch_,
     &TestATM90E32Component::offset_phase_, OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_VOLTAGE_CURRENT,
     &ATM90E32Component::clear_offset_calibrations},
    {"PowerOffset", false, &TestATM90E32Component::power_offset_pref_,
     &TestATM90E32Component::has_stored_power_offset_calibration_,
     &TestATM90E32Component::restored_power_offset_calibration_,
     &TestATM90E32Component::power_offset_calibration_mismatch_, &TestATM90E32Component::power_offset_phase_,
     OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_POWER, &ATM90E32Component::clear_power_offset_calibrations},
};

class ATM90E32CalibrationPersistence : public ::testing::TestWithParam<CalibrationTraits> {};
class ATM90E32OffsetPersistence : public ATM90E32CalibrationPersistence {};

INSTANTIATE_TEST_SUITE_P(CalibrationKinds, ATM90E32CalibrationPersistence, ::testing::ValuesIn(CALIBRATIONS),
                         [](const auto &info) { return info.param.name; });
INSTANTIATE_TEST_SUITE_P(CalibrationKinds, ATM90E32OffsetPersistence,
                         ::testing::Values(CALIBRATIONS[1], CALIBRATIONS[2]),
                         [](const auto &info) { return info.param.name; });

enum class SavedValue : uint8_t { SAVED_VALUE_OLD, SAVED_VALUE_NEW, SAVED_VALUE_ZERO };

static uint32_t next_preference_key() {
  static uint32_t key = 0x600000;
  return key++;
}

static GainCalibration gain_values(SavedValue value, uint8_t phase) {
  switch (value) {
    case SavedValue::SAVED_VALUE_OLD:
      return {static_cast<uint16_t>(1000 + phase), static_cast<uint16_t>(2000 + phase)};
    case SavedValue::SAVED_VALUE_NEW:
      return {static_cast<uint16_t>(300 + phase), static_cast<uint16_t>(400 + phase)};
    case SavedValue::SAVED_VALUE_ZERO:
      return {0, 0};
  }
  return {0, 0};
}

static GainCalibration previous_gain_values(bool restored, uint8_t phase) {
  if (restored)
    return gain_values(SavedValue::SAVED_VALUE_OLD, phase);
  return {static_cast<uint16_t>(100 + phase), static_cast<uint16_t>(200 + phase)};
}

static OffsetCalibration offset_values(SavedValue value, uint8_t phase) {
  switch (value) {
    case SavedValue::SAVED_VALUE_OLD:
      return {static_cast<int16_t>(11 + phase), static_cast<int16_t>(-21 - phase)};
    case SavedValue::SAVED_VALUE_NEW:
      return {static_cast<int16_t>(31 + phase), static_cast<int16_t>(-41 - phase)};
    case SavedValue::SAVED_VALUE_ZERO:
      return {0, 0};
  }
  return {0, 0};
}

static OffsetCalibration previous_offset_values(bool restored, uint8_t phase) {
  if (restored)
    return offset_values(SavedValue::SAVED_VALUE_OLD, phase);
  return {static_cast<int16_t>(101 + phase), static_cast<int16_t>(-201 - phase)};
}

static ESPPreferenceObject make_calibration_preference(const CalibrationTraits &kind, uint32_t key) {
  if (kind.gain)
    return global_preferences->make_preference<GainCalibration[3]>(key, true);
  return global_preferences->make_preference<OffsetCalibration[3]>(key, true);
}

static ESPPreferenceObject &component_preference(TestATM90E32Component &component, const CalibrationTraits &kind) {
  return component.*kind.preference;
}

static void set_calibration_state(TestATM90E32Component &component, const CalibrationTraits &kind, bool has_stored,
                                  bool restored) {
  component.*kind.has_stored = has_stored;
  component.*kind.restored = restored;
  component.update_using_saved_calibrations_();
}

static void set_new_calibration(TestATM90E32Component &component, CalibrationRegisterDelegate &delegate,
                                const CalibrationTraits &kind) {
  if (kind.gain) {
    for (uint8_t phase = 0; phase < 3; ++phase) {
      const auto value = gain_values(SavedValue::SAVED_VALUE_NEW, phase);
      component.gain_phase_[phase] = value;
      delegate.set_gain_values(phase, value.voltage_gain, value.current_gain);
    }
    return;
  }

  const auto type = kind.offset_type;
  for (uint8_t phase = 0; phase < 3; ++phase) {
    const auto value = offset_values(SavedValue::SAVED_VALUE_NEW, phase);
    component.write_offsets_to_registers_(phase, value.first_offset, value.second_offset, type);
    delegate.set_offset_values(type, phase, value.first_offset, value.second_offset);
  }
}

static void prepare_finish(TestATM90E32Component &component, CalibrationRegisterDelegate &delegate,
                           const CalibrationTraits &kind, uint32_t key) {
  component.delegate_ = &delegate;
  component.instance_id_ = "test";
  component_preference(component, kind) = make_calibration_preference(kind, key);
  set_new_calibration(component, delegate, kind);
}

static void save_values(ESPPreferenceObject &preference, const CalibrationTraits &kind, SavedValue value) {
  if (kind.gain) {
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

static void expect_values(ESPPreferenceObject &preference, const CalibrationTraits &kind, SavedValue value) {
  if (kind.gain) {
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

static void finish_calibration(TestATM90E32Component &component, const CalibrationTraits &kind,
                               bool previous_restored) {
  if (kind.gain) {
    GainCalibration previous[3];
    for (uint8_t phase = 0; phase < 3; ++phase)
      previous[phase] = previous_gain_values(previous_restored, phase);
    component.finish_gain_calibration_(previous, previous_restored);
    return;
  }

  OffsetCalibration previous[3];
  for (uint8_t phase = 0; phase < 3; ++phase)
    previous[phase] = previous_offset_values(previous_restored, phase);
  component.finish_offset_calibration_(previous, previous_restored, kind.offset_type);
}

static void expect_has_stored(const TestATM90E32Component &component, const CalibrationTraits &kind, bool expected) {
  EXPECT_EQ(component.*kind.has_stored, expected);
}

static void expect_restored(const TestATM90E32Component &component, const CalibrationTraits &kind, bool expected) {
  EXPECT_EQ(component.*kind.restored, expected);
}

static void expect_mismatches_cleared(const TestATM90E32Component &component, const CalibrationTraits &kind) {
  for (const bool mismatch : component.*kind.mismatches)
    EXPECT_FALSE(mismatch);
}

static void expect_previous_runtime_values(const TestATM90E32Component &component, const CalibrationTraits &kind,
                                           bool restored = false) {
  for (uint8_t phase = 0; phase < 3; ++phase) {
    if (kind.gain) {
      const auto expected = previous_gain_values(restored, phase);
      EXPECT_EQ(component.gain_phase_[phase].voltage_gain, expected.voltage_gain);
      EXPECT_EQ(component.gain_phase_[phase].current_gain, expected.current_gain);
    } else {
      const auto expected = previous_offset_values(restored, phase);
      const auto &actual = (component.*kind.offsets)[phase];
      EXPECT_EQ(actual.first_offset, expected.first_offset);
      EXPECT_EQ(actual.second_offset, expected.second_offset);
    }
  }
}

static void restore_calibration(TestATM90E32Component &component, const CalibrationTraits &kind) {
  if (kind.gain)
    component.restore_gain_calibrations_();
  else
    component.restore_offset_calibrations_(kind.offset_type);
}

TEST_P(ATM90E32CalibrationPersistence, RestoreVerifiesStoredValuesAndConfigFallback) {
  const auto &kind = GetParam();
  for (const bool stored : {false, true}) {
    for (const bool corrupt : {false, true}) {
      for (const bool drop : {false, true}) {
        SCOPED_TRACE(::testing::Message() << "stored=" << stored << ", corrupt=" << corrupt << ", drop=" << drop);
        ScopedATM90E32Preferences preferences(false);
        TestATM90E32Component component;
        CalibrationRegisterDelegate delegate;
        component.delegate_ = &delegate;
        component.set_instance_id("test");
        auto &preference = component_preference(component, kind);
        preference = make_calibration_preference(kind, next_preference_key());
        save_values(preference, kind, stored ? SavedValue::SAVED_VALUE_OLD : SavedValue::SAVED_VALUE_ZERO);
        for (uint8_t phase = 0; phase < 3; ++phase) {
          if (kind.gain) {
            component.set_volt_gain(phase, 100 + phase);
            component.set_ct_gain(phase, 200 + phase);
          } else {
            (component.*kind.offsets)[phase] = previous_offset_values(false, phase);
          }
        }
        if (corrupt)
          delegate.corrupt_calibration_writes();
        if (drop)
          delegate.drop_next_calibration_write();

        const auto output = capture_calibration_logs([&]() { restore_calibration(component, kind); });

        const bool restored = stored && !corrupt && !drop;
        expect_has_stored(component, kind, stored);
        expect_restored(component, kind, restored);
        EXPECT_EQ(component.using_saved_calibrations_, restored);
        expect_values(preference, kind, stored ? SavedValue::SAVED_VALUE_OLD : SavedValue::SAVED_VALUE_ZERO);
        if (!restored)
          expect_previous_runtime_values(component, kind);
        if (corrupt) {
          EXPECT_NE(output.find(kind.gain ? "config readback verification failed"
                                          : "config fallback both failed verification"),
                    std::string::npos);
        } else if (drop) {
          EXPECT_NE(output.find("config values verified"), std::string::npos);
        }
      }
    }
  }
}

TEST_P(ATM90E32CalibrationPersistence, FailedPersistencePreservesStoredPayloadAndPreviousRuntimeState) {
  for (const bool restored : {false, true}) {
    SCOPED_TRACE(restored);
    ScopedATM90E32Preferences preferences(true);
    const auto &kind = GetParam();
    TestATM90E32Component component;
    CalibrationRegisterDelegate delegate;
    prepare_finish(component, delegate, kind, next_preference_key());
    auto &preference = component_preference(component, kind);
    save_values(preference, kind, SavedValue::SAVED_VALUE_OLD);
    set_calibration_state(component, kind, true, restored);

    const auto output = capture_calibration_logs([&]() { finish_calibration(component, kind, restored); });

    expect_values(preference, kind, SavedValue::SAVED_VALUE_OLD);
    expect_previous_runtime_values(component, kind, restored);
    expect_has_stored(component, kind, true);
    expect_restored(component, kind, restored);
    EXPECT_EQ(component.using_saved_calibrations_, restored);
    EXPECT_EQ(output.find("completed and verified"), std::string::npos);
  }
}

TEST_P(ATM90E32CalibrationPersistence, StoredPayloadLoadFailureRefusesOverwrite) {
  ScopedATM90E32Preferences preferences(false);
  const auto &kind = GetParam();
  TestATM90E32Component component;
  CalibrationRegisterDelegate delegate;
  const uint32_t key = next_preference_key();
  prepare_finish(component, delegate, kind, key);
  auto raw_preference = global_preferences->make_preference<uint32_t>(key, true);
  constexpr uint32_t raw_value = 0xA5A55A5A;
  ASSERT_TRUE(raw_preference.save(&raw_value));
  ASSERT_TRUE(global_preferences->sync());
  set_calibration_state(component, kind, true, false);

  const auto output = capture_calibration_logs([&]() { finish_calibration(component, kind, false); });

  uint32_t actual = 0;
  ASSERT_TRUE(raw_preference.load(&actual));
  EXPECT_EQ(actual, raw_value);
  expect_has_stored(component, kind, true);
  expect_restored(component, kind, false);
  EXPECT_FALSE(component.using_saved_calibrations_);
  EXPECT_NE(output.find("Failed to load stored"), std::string::npos);
  EXPECT_NE(output.find("Clear stored"), std::string::npos);
  EXPECT_NE(output.find("and retry"), std::string::npos);
  EXPECT_EQ(output.find("completed and verified"), std::string::npos);
}

TEST_P(ATM90E32CalibrationPersistence, FreshPersistenceUsesZeroSentinelAndCanRetry) {
  const auto &kind = GetParam();
  ScopedATM90E32Preferences preferences(true);
  TestATM90E32Component component;
  CalibrationRegisterDelegate delegate;
  prepare_finish(component, delegate, kind, next_preference_key());
  set_calibration_state(component, kind, false, false);

  const auto output = capture_calibration_logs([&]() { finish_calibration(component, kind, false); });

  expect_values(component_preference(component, kind), kind, SavedValue::SAVED_VALUE_ZERO);
  expect_has_stored(component, kind, false);
  expect_restored(component, kind, false);
  EXPECT_FALSE(component.using_saved_calibrations_);
  EXPECT_EQ(output.find("completed and verified"), std::string::npos);

  preferences.allow_sync();
  set_new_calibration(component, delegate, kind);
  const auto retry_output = capture_calibration_logs([&]() { finish_calibration(component, kind, false); });

  expect_values(component_preference(component, kind), kind, SavedValue::SAVED_VALUE_NEW);
  expect_has_stored(component, kind, true);
  expect_restored(component, kind, true);
  EXPECT_TRUE(component.using_saved_calibrations_);
  EXPECT_NE(retry_output.find("completed and verified"), std::string::npos);
}

static void check_clear_retry(const CalibrationTraits &kind, bool other_restored) {
  ScopedATM90E32Preferences preferences(true);
  TestATM90E32Component component;
  CalibrationRegisterDelegate delegate;
  prepare_finish(component, delegate, kind, next_preference_key());
  auto &preference = component_preference(component, kind);
  save_values(preference, kind, SavedValue::SAVED_VALUE_OLD);
  set_calibration_state(component, kind, true, true);
  if (kind.offset_type == OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_VOLTAGE_CURRENT) {
    component.restored_power_offset_calibration_ = other_restored;
  } else {
    component.restored_offset_calibration_ = other_restored;
  }
  bool *mismatches = component.*kind.mismatches;
  mismatches[1] = true;

  const auto first_output = capture_calibration_logs([&]() { (component.*kind.clear)(); });

  expect_has_stored(component, kind, true);
  expect_restored(component, kind, false);
  EXPECT_EQ(component.using_saved_calibrations_, other_restored);
  expect_mismatches_cleared(component, kind);
  EXPECT_NE(first_output.find("Failed to clear"), std::string::npos);
  EXPECT_EQ(first_output.find("Offsets cleared."), std::string::npos);
  EXPECT_EQ(first_output.find("Power offsets cleared."), std::string::npos);

  preferences.allow_sync();
  const auto second_output = capture_calibration_logs([&]() { (component.*kind.clear)(); });

  expect_has_stored(component, kind, false);
  expect_restored(component, kind, false);
  EXPECT_EQ(component.using_saved_calibrations_, other_restored);
  expect_values(preference, kind, SavedValue::SAVED_VALUE_ZERO);
  EXPECT_EQ(second_output.find("Failed to clear"), std::string::npos);
}

TEST_P(ATM90E32CalibrationPersistence, ClearFailureRetainsRetryEligibility) {
  check_clear_retry(GetParam(), false);
  check_clear_retry(GetParam(), true);
}

TEST_P(ATM90E32OffsetPersistence, RollbackReadbackFailureInvalidatesState) {
  const auto &kind = GetParam();
  ScopedATM90E32Preferences preferences(true);
  TestATM90E32Component component;
  CalibrationRegisterDelegate delegate;
  prepare_finish(component, delegate, kind, next_preference_key());
  auto &preference = component_preference(component, kind);
  save_values(preference, kind, SavedValue::SAVED_VALUE_OLD);
  set_calibration_state(component, kind, true, true);
  if (kind.offset_type == OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_VOLTAGE_CURRENT)
    component.restored_power_offset_calibration_ = true;
  else
    component.restored_offset_calibration_ = true;
  bool *mismatches = component.*kind.mismatches;
  for (uint8_t phase = 0; phase < 3; ++phase)
    mismatches[phase] = true;
  delegate.corrupt_calibration_writes();

  const auto output = capture_calibration_logs([&]() { finish_calibration(component, kind, true); });

  expect_has_stored(component, kind, true);
  expect_restored(component, kind, false);
  EXPECT_TRUE(component.using_saved_calibrations_);
  expect_mismatches_cleared(component, kind);
  EXPECT_NE(output.find("rollback readback verification failed"), std::string::npos);
  EXPECT_EQ(output.find("completed and verified"), std::string::npos);
}

TEST(ATM90E32CalibrationPersistence, OffsetRestoreFallbackPreservesOtherSavedCategories) {
  ScopedATM90E32Preferences preferences(false);
  TestATM90E32Component component;
  CalibrationRegisterDelegate delegate;
  component.delegate_ = &delegate;
  component.instance_id_ = "test";
  component.offset_pref_ = global_preferences->make_preference<OffsetCalibration[3]>(next_preference_key(), true);
  save_values(component.offset_pref_, CALIBRATIONS[1], SavedValue::SAVED_VALUE_OLD);
  component.restored_power_offset_calibration_ = true;
  component.restored_gain_calibration_ = true;
  component.using_saved_calibrations_ = false;
  for (uint8_t phase = 0; phase < 3; ++phase) {
    component.set_voltage_offset(phase, static_cast<int16_t>(101 + phase));
    component.set_current_offset(phase, static_cast<int16_t>(-201 - phase));
  }
  delegate.drop_next_calibration_write();

  const auto output = capture_calibration_logs([&]() {
    component.restore_offset_calibrations_(OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_VOLTAGE_CURRENT);
  });

  EXPECT_FALSE(component.restored_offset_calibration_);
  EXPECT_TRUE(component.restored_power_offset_calibration_);
  EXPECT_TRUE(component.restored_gain_calibration_);
  EXPECT_TRUE(component.using_saved_calibrations_);
  expect_mismatches_cleared(component, CALIBRATIONS[1]);
  EXPECT_NE(output.find("config values verified"), std::string::npos);
}

#endif  // USE_HOST

}  // namespace esphome::atm90e32::testing
