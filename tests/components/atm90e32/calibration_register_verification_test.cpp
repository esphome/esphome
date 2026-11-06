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

}  // namespace esphome::atm90e32::testing

namespace esphome::atm90e32 {

#ifdef USE_HOST

namespace fs = std::filesystem;

template<typename F> static std::string capture_calibration_logs(F action) {
  const auto previous_baud_rate = logger::global_logger->get_baud_rate();
  ::testing::internal::CaptureStdout();
  logger::global_logger->set_baud_rate(115200);
  action();
  logger::global_logger->set_baud_rate(previous_baud_rate);
  return ::testing::internal::GetCapturedStdout();
}

class CalibrationRegisterDelegate final : public spi::SPIDelegate {
 public:
  explicit CalibrationRegisterDelegate(const ATM90E32Component &component) : component_(component) {}

  uint8_t transfer(uint8_t) override { return 0; }

  void transfer(uint8_t *data, size_t length) override {
    if (length != 4 || (data[0] & 0x80) == 0)
      return;

    const uint16_t address = static_cast<uint16_t>(((data[0] & 0x03) << 8) | data[1]);
    ++this->read_counts_[address];
    const uint16_t value = this->registers_[address];
    uint16_t received = address == this->corrupt_read_address_ ? value ^ 0x8000 : value;
    if (address == ATM90E32_REGISTER_LASTSPIDATA && this->unstable_last_spi_)
      received = value ^ this->read_counts_[address];
    data[2] = static_cast<uint8_t>(received >> 8);
    data[3] = static_cast<uint8_t>(received);
    if (address != ATM90E32_REGISTER_LASTSPIDATA)
      this->registers_[ATM90E32_REGISTER_LASTSPIDATA] = value;
    if (address >= ATM90E32_REGISTER_APENERGYA && address <= ATM90E32_REGISTER_ANENERGYC)
      this->registers_[address] = 0;
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
    this->registers_[this->component_.voltage_gain_registers[phase]] = voltage;
    this->registers_[this->component_.current_gain_registers[phase]] = current;
  }

  void set_offset_values(OffsetCalibrationType type, uint8_t phase, int16_t first, int16_t second) {
    const bool power = type == OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_POWER;
    const uint16_t *first_registers =
        power ? this->component_.power_offset_registers : this->component_.voltage_offset_registers;
    const uint16_t *second_registers =
        power ? this->component_.reactive_power_offset_registers : this->component_.current_offset_registers;
    this->registers_[first_registers[phase]] = static_cast<uint16_t>(first);
    this->registers_[second_registers[phase]] = static_cast<uint16_t>(second);
  }

  void drop_next_calibration_write() { this->drop_calibration_writes_++; }
  void corrupt_calibration_writes() { this->corrupt_calibration_writes_ = true; }
  void corrupt_read(uint16_t address) { this->corrupt_read_address_ = address; }
  void set_register(uint16_t address, uint16_t value) { this->registers_[address] = value; }
  void unstable_last_spi() { this->unstable_last_spi_ = true; }
  unsigned read_count(uint16_t address) const { return this->read_counts_[address]; }

 private:
  bool is_calibration_register_(uint16_t address) const {
    const auto &c = this->component_;
    for (const uint16_t *table :
         {c.voltage_gain_registers, c.current_gain_registers, c.voltage_offset_registers, c.current_offset_registers,
          c.power_offset_registers, c.reactive_power_offset_registers}) {
      for (uint8_t phase = 0; phase < 3; ++phase) {
        if (address == table[phase])
          return true;
      }
    }
    return false;
  }

  const ATM90E32Component &component_;
  std::array<uint16_t, 1024> registers_{};
  std::array<unsigned, 1024> read_counts_{};
  bool unstable_last_spi_{false};
  uint8_t drop_calibration_writes_{0};
  bool corrupt_calibration_writes_{false};
  uint16_t corrupt_read_address_{0xFFFF};
};

struct CalibrationTraits {
  const char *name;
  bool gain;
  ESPPreferenceObject ATM90E32Component::*preference;
  bool ATM90E32Component::*has_stored;
  bool ATM90E32Component::*restored;
  // A restored flag of another calibration kind, which this kind must leave alone.
  bool ATM90E32Component::*other_restored;
  bool (ATM90E32Component::*mismatches)[3];
  OffsetCalibration (ATM90E32Component::*offsets)[3];
  OffsetCalibrationType offset_type;
  void (ATM90E32Component::*clear)();
};

enum class SavedValue : uint8_t { SAVED_VALUE_OLD, SAVED_VALUE_NEW, SAVED_VALUE_ZERO };

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

// Owns a component wired to a register-backed SPI delegate and to scratch host preferences, and reaches its
// protected members for the tests.
class ATM90E32CalibrationTestAccess {
 public:
  using C = ATM90E32Component;
  static constexpr CalibrationTraits GAIN{"Gain",
                                          true,
                                          &C::gain_calibration_pref_,
                                          &C::has_stored_gain_calibration_,
                                          &C::restored_gain_calibration_,
                                          &C::restored_power_offset_calibration_,
                                          &C::gain_calibration_mismatch_,
                                          nullptr,
                                          OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_VOLTAGE_CURRENT,
                                          &C::clear_gain_calibrations};
  static constexpr CalibrationTraits OFFSET{"Offset",
                                            false,
                                            &C::offset_pref_,
                                            &C::has_stored_offset_calibration_,
                                            &C::restored_offset_calibration_,
                                            &C::restored_power_offset_calibration_,
                                            &C::offset_calibration_mismatch_,
                                            &C::offset_phase_,
                                            OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_VOLTAGE_CURRENT,
                                            &C::clear_offset_calibrations};
  static constexpr CalibrationTraits POWER_OFFSET{"PowerOffset",
                                                  false,
                                                  &C::power_offset_pref_,
                                                  &C::has_stored_power_offset_calibration_,
                                                  &C::restored_power_offset_calibration_,
                                                  &C::restored_offset_calibration_,
                                                  &C::power_offset_calibration_mismatch_,
                                                  &C::power_offset_phase_,
                                                  OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_POWER,
                                                  &C::clear_power_offset_calibrations};

  ATM90E32CalibrationTestAccess()
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
    this->allow_sync();
    global_preferences = &this->preferences_;
    host::host_preferences = &this->preferences_;

    this->component.delegate_ = &this->delegate;
    this->component.instance_id_ = "test";
  }

  ~ATM90E32CalibrationTestAccess() {
    if (this->component.offset_calibration_pending_)
      this->settle_calibration();
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

  // Call before the first preference is made so every sync fails until allow_sync().
  void fail_sync() { setenv("ESPHOME_PREFDIR", "/dev/null", 1); }
  void allow_sync() { setenv("ESPHOME_PREFDIR", this->prefdir_.string().c_str(), 1); }

  uint16_t read16(uint16_t reg) { return this->component.read16_(reg); }
  void write16(uint16_t reg, uint16_t value) { this->component.write16_(reg, value); }
  void configure_chip() { this->component.configure_chip_(); }
  float voltage_average() { return this->component.get_phase_voltage_avg_(0); }
  float current_average() { return this->component.get_phase_current_avg_(0); }
  float energy(bool reverse) {
    return reverse ? this->component.get_phase_reverse_active_energy_(0)
                   : this->component.get_phase_forward_active_energy_(0);
  }
  void run_offsets(const CalibrationTraits &kind) {
    if (kind.offset_type == OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_POWER)
      this->component.run_power_offset_calibrations();
    else
      this->component.run_offset_calibrations();
  }
  void advance_calibration() {
    delay(1100);  // NOLINT - The host scheduler uses real time.
    App.scheduler.call(millis());
  }
  void settle_calibration() {
    for (uint8_t step = 0; step < 3 && this->component.offset_calibration_pending_; ++step)
      this->advance_calibration();
    EXPECT_FALSE(this->component.offset_calibration_pending_);
  }

  ESPPreferenceObject &preference(const CalibrationTraits &kind) { return this->component.*kind.preference; }

  ESPPreferenceObject &make_preference(const CalibrationTraits &kind, uint32_t key = next_preference_key()) {
    auto &preference = this->preference(kind);
    if (kind.gain) {
      preference = global_preferences->make_preference<GainCalibration[3]>(key, true);
    } else {
      preference = global_preferences->make_preference<OffsetCalibration[3]>(key, true);
    }
    return preference;
  }

  void set_state(const CalibrationTraits &kind, bool has_stored, bool restored) {
    this->component.*kind.has_stored = has_stored;
    this->component.*kind.restored = restored;
  }

  void set_new_calibration(const CalibrationTraits &kind) {
    for (uint8_t phase = 0; phase < 3; ++phase) {
      if (kind.gain) {
        const auto value = gain_values(SavedValue::SAVED_VALUE_NEW, phase);
        this->component.gain_phase_[phase] = value;
        this->delegate.set_gain_values(phase, value.voltage_gain, value.current_gain);
      } else {
        const auto value = offset_values(SavedValue::SAVED_VALUE_NEW, phase);
        this->component.write_offsets_to_registers_(phase, value.first_offset, value.second_offset, kind.offset_type);
        this->delegate.set_offset_values(kind.offset_type, phase, value.first_offset, value.second_offset);
      }
    }
  }

  ESPPreferenceObject &prepare_finish(const CalibrationTraits &kind, uint32_t key = next_preference_key()) {
    auto &preference = this->make_preference(kind, key);
    this->set_new_calibration(kind);
    return preference;
  }

  void finish(const CalibrationTraits &kind, bool previous_restored) {
    if (kind.gain) {
      GainCalibration previous[3];
      for (uint8_t phase = 0; phase < 3; ++phase)
        previous[phase] = previous_gain_values(previous_restored, phase);
      this->component.finish_gain_calibration_(previous);
      return;
    }
    OffsetCalibration previous[3];
    for (uint8_t phase = 0; phase < 3; ++phase)
      previous[phase] = previous_offset_values(previous_restored, phase);
    this->component.finish_offset_calibration_(previous, kind.offset_type);
  }

  void restore(const CalibrationTraits &kind) {
    if (kind.gain) {
      this->component.restore_gain_calibrations_();
    } else {
      this->component.restore_offset_calibrations_(kind.offset_type);
    }
  }

  void expect_previous_runtime_values(const CalibrationTraits &kind, bool restored = false) {
    for (uint8_t phase = 0; phase < 3; ++phase) {
      if (kind.gain) {
        const auto expected = previous_gain_values(restored, phase);
        EXPECT_EQ(this->component.gain_phase_[phase].voltage_gain, expected.voltage_gain);
        EXPECT_EQ(this->component.gain_phase_[phase].current_gain, expected.current_gain);
      } else {
        const auto expected = previous_offset_values(restored, phase);
        const auto &actual = (this->component.*kind.offsets)[phase];
        EXPECT_EQ(actual.first_offset, expected.first_offset);
        EXPECT_EQ(actual.second_offset, expected.second_offset);
      }
    }
  }

  void expect_mismatches_cleared(const CalibrationTraits &kind) {
    for (const bool mismatch : this->component.*kind.mismatches)
      EXPECT_FALSE(mismatch);
  }

  static uint32_t next_preference_key() {
    static uint32_t key = 0x600000;
    return key++;
  }

 private:
  host::HostPreferences preferences_;
  ESPPreferences *previous_preferences_;
  host::HostPreferences *previous_host_preferences_;
  fs::path prefdir_;
  std::string previous_prefdir_;
  bool had_prefdir_{false};

 public:
  ATM90E32Component component;
  CalibrationRegisterDelegate delegate{component};
};

namespace testing {

using Access = ATM90E32CalibrationTestAccess;
static constexpr CalibrationTraits CALIBRATIONS[]{Access::GAIN, Access::OFFSET, Access::POWER_OFFSET};

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

TEST(ATM90E32GainCalibration, FailedRollbackDoesNotReportVerifiedGainsOnReconnect) {
  const auto &kind = Access::GAIN;
  for (const bool mismatch : {false, true}) {
    for (uint8_t offsets = 0; offsets < 4; offsets++) {
      SCOPED_TRACE(::testing::Message() << "mismatch=" << mismatch << ", offsets=" << +offsets);
      Access access;
      auto &component = access.component;
      component.set_enable_gain_calibration(true);
      component.set_enable_offset_calibration(true);
      access.set_state(kind, true, true);
      component.*Access::OFFSET.restored = (offsets & 1) != 0;
      component.*Access::POWER_OFFSET.restored = (offsets & 2) != 0;
      for (bool &phase : component.*kind.mismatches)
        phase = mismatch;
      access.delegate.corrupt_calibration_writes();

      access.finish(kind, true);

      EXPECT_FALSE(component.*kind.restored);
      EXPECT_TRUE(component.*kind.has_stored);
      EXPECT_EQ(component.*Access::OFFSET.restored, (offsets & 1) != 0);
      EXPECT_EQ(component.*Access::POWER_OFFSET.restored, (offsets & 2) != 0);
      access.expect_mismatches_cleared(kind);

      const auto output = capture_calibration_logs([&]() { component.dump_config(); });
      EXPECT_EQ(output.find("Gain calibration loaded and verified successfully."), std::string::npos);
      EXPECT_EQ(output.find("Gain mismatch: using flash values"), std::string::npos);
      EXPECT_EQ(output.find("Restoring saved gain calibrations to registers"), std::string::npos);
    }
  }
}

TEST(ATM90E32RegisterAccess, HandlesFullTenBitAddress) {
  Access access;

  access.write16(0x3FF, 0xABCD);

  EXPECT_EQ(access.read16(0x3FF), 0xABCD);
}

TEST(ATM90E32Measurements, RejectsCorruptSamplesAndRecovers) {
  for (const uint16_t address : {ATM90E32_REGISTER_URMS, ATM90E32_REGISTER_IRMS, ATM90E32_REGISTER_PFMEAN,
                                 ATM90E32_REGISTER_PMEAN, ATM90E32_REGISTER_PMEANLSB}) {
    SCOPED_TRACE(address);
    Access access;
    sensor::Sensor measurement;
    auto &component = access.component;
    switch (address) {
      case ATM90E32_REGISTER_URMS:
        component.set_voltage_sensor(0, &measurement);
        break;
      case ATM90E32_REGISTER_IRMS:
        component.set_current_sensor(0, &measurement);
        break;
      case ATM90E32_REGISTER_PFMEAN:
        component.set_power_factor_sensor(0, &measurement);
        break;
      default:
        component.set_power_sensor(0, &measurement);
    }
    access.delegate.set_register(ATM90E32_REGISTER_METEREN, 1);
    access.delegate.set_register(address, 100);
    access.delegate.corrupt_read(address);

    component.update();
    component.loop();

    EXPECT_TRUE(std::isnan(measurement.get_state()));
    EXPECT_TRUE(component.status_has_warning());
    access.delegate.corrupt_read(0xFFFF);
    access.delegate.set_register(address, 0);
    component.update();
    component.loop();
    EXPECT_FLOAT_EQ(measurement.get_state(), 0.0f);
    EXPECT_FALSE(component.status_has_warning());
  }
}

TEST(ATM90E32Measurements, RecoversReadClearEnergyWithoutReadingItTwice) {
  for (const bool reverse : {false, true}) {
    for (const bool corrupt : {false, true}) {
      Access access;
      const uint16_t address = reverse ? ATM90E32_REGISTER_ANENERGY : ATM90E32_REGISTER_APENERGY;
      access.delegate.set_register(address, 65535);
      if (corrupt)
        access.delegate.corrupt_read(address);
      EXPECT_FLOAT_EQ(access.energy(reverse), 65535 * (10.0f / 3200.0f));
      EXPECT_EQ(access.delegate.read_count(address), 1);
      EXPECT_EQ(access.delegate.read_count(ATM90E32_REGISTER_LASTSPIDATA), corrupt ? 3 : 1);
      EXPECT_FALSE(access.component.status_has_warning());
      access.delegate.corrupt_read(0xFFFF);
      EXPECT_FLOAT_EQ(access.energy(reverse), 65535 * (10.0f / 3200.0f));
    }
  }
}

TEST(ATM90E32Measurements, BoundsEnergyRecoveryWhenLastSpiRemainsUnstable) {
  Access access;
  access.delegate.set_register(ATM90E32_REGISTER_APENERGY, 100);
  access.delegate.unstable_last_spi();
  EXPECT_TRUE(std::isnan(access.energy(false)));
  EXPECT_TRUE(access.component.status_has_warning());
  EXPECT_EQ(access.delegate.read_count(ATM90E32_REGISTER_APENERGY), 1);
  EXPECT_EQ(access.delegate.read_count(ATM90E32_REGISTER_LASTSPIDATA), 3);
}

TEST(ATM90E32Measurements, PreservesFullRegisterRange) {
  Access access;
  sensor::Sensor voltage;
  sensor::Sensor power;
  access.component.set_voltage_sensor(0, &voltage);
  access.component.set_power_sensor(0, &power);
  access.delegate.set_register(ATM90E32_REGISTER_METEREN, 1);
  for (const uint16_t raw : {0, 1, 0x7FFF, 0x8000, 0xFFFF}) {
    access.delegate.set_register(ATM90E32_REGISTER_URMS, raw);
    access.component.update();
    access.component.loop();
    EXPECT_FLOAT_EQ(voltage.get_state(), raw / 100.0f);
    EXPECT_FALSE(access.component.status_has_warning());
  }
  for (const int32_t raw : {INT32_MIN, -1, 0, 1, INT32_MAX}) {
    access.delegate.set_register(ATM90E32_REGISTER_PMEAN, static_cast<uint32_t>(raw) >> 16);
    access.delegate.set_register(ATM90E32_REGISTER_PMEANLSB, static_cast<uint16_t>(raw));
    access.component.update();
    access.component.loop();
    EXPECT_FLOAT_EQ(power.get_state(), raw * 0.00032f);
    EXPECT_FALSE(access.component.status_has_warning());
  }
}

TEST(ATM90E32GainCalibration, RejectsInvalidAveragesWithoutChangingCalibration) {
  for (const uint16_t address : {ATM90E32_REGISTER_URMS, ATM90E32_REGISTER_IRMS}) {
    SCOPED_TRACE(address);
    Access access;
    auto &component = access.component;
    const auto &kind = Access::GAIN;
    auto &preference = access.make_preference(kind);
    save_values(preference, kind, SavedValue::SAVED_VALUE_OLD);
    access.restore(kind);
    component.set_enable_gain_calibration(true);
    for (uint8_t phase = 0; phase < 3; ++phase) {
      access.delegate.set_register(ATM90E32_REGISTER_URMS + phase, 12000);
      access.delegate.set_register(ATM90E32_REGISTER_IRMS + phase, 5000);
    }
    access.delegate.corrupt_read(address);

    EXPECT_TRUE(std::isnan(address == ATM90E32_REGISTER_URMS ? access.voltage_average() : access.current_average()));
    component.run_gain_calibrations();

    expect_values(preference, kind, SavedValue::SAVED_VALUE_OLD);
    access.expect_previous_runtime_values(kind, true);
    EXPECT_EQ(access.read16(component.voltage_gain_registers[0]), 1000);
    EXPECT_EQ(access.read16(component.current_gain_registers[0]), 2000);
  }
}

TEST(ATM90E32GainCalibration, LaterReadFailureDoesNotApplyPartialGains) {
  for (const uint16_t address :
       {ATM90E32_REGISTER_URMSC, ATM90E32_REGISTER_IRMSC, ATM90E32_REGISTER_UGAINC, ATM90E32_REGISTER_IGAINC}) {
    SCOPED_TRACE(address);
    Access access;
    auto &component = access.component;
    auto &preference = access.make_preference(Access::GAIN);
    save_values(preference, Access::GAIN, SavedValue::SAVED_VALUE_OLD);
    access.restore(Access::GAIN);
    component.set_enable_gain_calibration(true);
    for (uint8_t phase = 0; phase < 3; ++phase) {
      access.delegate.set_register(ATM90E32_REGISTER_URMS + phase, 6000);
      access.delegate.set_register(ATM90E32_REGISTER_IRMS + phase, 2500);
    }
    access.delegate.corrupt_read(address);

    component.run_gain_calibrations();

    expect_values(preference, Access::GAIN, SavedValue::SAVED_VALUE_OLD);
    access.expect_previous_runtime_values(Access::GAIN, true);
  }
}

TEST(ATM90E32GainCalibration, AppliesAllGainsOrRestoresPreviousValues) {
  for (const bool drop : {false, true}) {
    SCOPED_TRACE(drop);
    Access access;
    auto &component = access.component;
    auto &preference = access.make_preference(Access::GAIN);
    save_values(preference, Access::GAIN, SavedValue::SAVED_VALUE_OLD);
    access.restore(Access::GAIN);
    component.set_enable_gain_calibration(true);
    for (uint8_t phase = 0; phase < 3; ++phase) {
      access.delegate.set_register(ATM90E32_REGISTER_URMS + phase, 6000);
      access.delegate.set_register(ATM90E32_REGISTER_IRMS + phase, 2500);
    }
    if (drop)
      access.delegate.drop_next_calibration_write();

    component.run_gain_calibrations();

    GainCalibration saved[3];
    ASSERT_TRUE(preference.load(&saved));
    for (uint8_t phase = 0; phase < 3; ++phase) {
      const auto previous = gain_values(SavedValue::SAVED_VALUE_OLD, phase);
      EXPECT_EQ(saved[phase].voltage_gain, previous.voltage_gain * (drop ? 1 : 2));
      EXPECT_EQ(saved[phase].current_gain, previous.current_gain * (drop ? 1 : 2));
      EXPECT_EQ(access.read16(component.voltage_gain_registers[phase]), saved[phase].voltage_gain);
      EXPECT_EQ(access.read16(component.current_gain_registers[phase]), saved[phase].current_gain);
    }
  }
}

TEST(ATM90E32Setup, ConfiguresGainsAndThresholdsWithCalibrationEnabledOrDisabled) {
  for (const bool offsets : {false, true}) {
    for (const bool gains : {false, true}) {
      SCOPED_TRACE(::testing::Message() << "offsets=" << offsets << ", gains=" << gains);
      Access access;
      auto &component = access.component;
      spi::NullPin cs;
      component.set_cs_pin(&cs);
      component.set_enable_offset_calibration(offsets);
      component.set_enable_gain_calibration(gains);
      component.set_threshold_voltage_sag_v(93.6f);
      component.set_threshold_voltage_peak_v(146.4f);
      for (uint8_t phase = 0; phase < 3; ++phase) {
        component.set_volt_gain(phase, 7305);
        component.set_ct_gain(phase, 27518);
      }

      access.configure_chip();

      ASSERT_FALSE(component.is_failed());
      for (uint8_t phase = 0; phase < 3; ++phase) {
        EXPECT_EQ(access.read16(component.voltage_gain_registers[phase]), 7305);
        EXPECT_EQ(access.read16(component.current_gain_registers[phase]), 27518);
      }
      EXPECT_NE(access.read16(ATM90E32_REGISTER_SAGTH), 0);
      EXPECT_NE(access.read16(ATM90E32_REGISTER_OVTH), 0);
      EXPECT_EQ(access.read16(ATM90E32_REGISTER_CFGREGACCEN), 0);
    }
  }
}

TEST(ATM90E32Setup, ThresholdsUseRestoredGains) {
  Access access;
  auto &component = access.component;
  spi::NullPin cs;
  component.set_cs_pin(&cs);
  component.set_enable_gain_calibration(true);
  component.set_threshold_voltage_sag_v(93.6f);
  component.set_threshold_voltage_peak_v(146.4f);
  GainCalibration stored[3];
  for (uint8_t phase = 0; phase < 3; ++phase) {
    component.set_volt_gain(phase, 7305);
    component.set_ct_gain(phase, 27518);
    stored[phase] = {9000, 27518};
  }
  auto preference = global_preferences->make_preference<GainCalibration[3]>(
      fnv1_hash_extend(fnv1_hash("_gain_calibration_"), "test"), true);
  ASSERT_TRUE(preference.save(&stored));

  access.configure_chip();

  EXPECT_EQ(access.read16(component.voltage_gain_registers[0]), 9000);
  EXPECT_EQ(access.read16(ATM90E32_REGISTER_SAGTH),
            ATM90E32Component::calculate_voltage_threshold_from_volts(93.6f, 9000));
  EXPECT_EQ(access.read16(ATM90E32_REGISTER_OVTH),
            ATM90E32Component::calculate_voltage_threshold_from_volts(146.4f, 9000));
}

class ATM90E32CalibrationPersistence : public ::testing::TestWithParam<CalibrationTraits> {};
class ATM90E32OffsetPersistence : public ATM90E32CalibrationPersistence {};

INSTANTIATE_TEST_SUITE_P(CalibrationKinds, ATM90E32CalibrationPersistence, ::testing::ValuesIn(CALIBRATIONS),
                         [](const auto &info) { return info.param.name; });
INSTANTIATE_TEST_SUITE_P(CalibrationKinds, ATM90E32OffsetPersistence,
                         ::testing::Values(Access::OFFSET, Access::POWER_OFFSET),
                         [](const auto &info) { return info.param.name; });

TEST_P(ATM90E32CalibrationPersistence, FailedClearWritePreservesStoredAndRuntimeCalibration) {
  const auto &kind = GetParam();
  Access access;
  auto &preference = access.make_preference(kind);
  save_values(preference, kind, SavedValue::SAVED_VALUE_OLD);
  access.restore(kind);
  access.delegate.drop_next_calibration_write();

  (access.component.*kind.clear)();

  expect_values(preference, kind, SavedValue::SAVED_VALUE_OLD);
  access.expect_previous_runtime_values(kind, true);
  EXPECT_TRUE(access.component.*kind.has_stored);
  EXPECT_TRUE(access.component.*kind.restored);
  const auto first = kind.gain ? access.component.voltage_gain_registers[0]
                     : kind.offset_type == OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_POWER
                         ? access.component.power_offset_registers[0]
                         : access.component.voltage_offset_registers[0];
  EXPECT_EQ(access.read16(first), kind.gain ? 1000 : 11);
}

TEST_P(ATM90E32CalibrationPersistence, FailedClearPersistenceRestoresOriginalPayload) {
  const auto &kind = GetParam();
  Access access;
  access.fail_sync();
  auto &preference = access.make_preference(kind);
  save_values(preference, kind, SavedValue::SAVED_VALUE_OLD);
  access.restore(kind);

  (access.component.*kind.clear)();

  expect_values(preference, kind, SavedValue::SAVED_VALUE_OLD);
  access.expect_previous_runtime_values(kind, true);
  EXPECT_TRUE(access.component.*kind.has_stored);
  EXPECT_TRUE(access.component.*kind.restored);
}

TEST_P(ATM90E32OffsetPersistence, MinusOneOffsetRequiresValidReadback) {
  const auto &kind = GetParam();
  const bool power = kind.offset_type == OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_POWER;
  for (const bool corrupt : {false, true}) {
    SCOPED_TRACE(corrupt);
    Access access;
    auto &component = access.component;
    for (uint8_t phase = 0; phase < 3; ++phase) {
      if (power) {
        component.set_active_power_offset(phase, -1);
        component.set_reactive_power_offset(phase, -1);
      } else {
        component.set_voltage_offset(phase, -1);
        component.set_current_offset(phase, -1);
      }
    }
    auto &preference = access.make_preference(kind);
    save_values(preference, kind, SavedValue::SAVED_VALUE_OLD);
    access.restore(kind);
    if (corrupt)
      access.delegate.corrupt_read(power ? component.power_offset_registers[0] : component.voltage_offset_registers[0]);

    (component.*kind.clear)();

    EXPECT_EQ(component.*kind.has_stored, corrupt);
    expect_values(preference, kind, corrupt ? SavedValue::SAVED_VALUE_OLD : SavedValue::SAVED_VALUE_ZERO);
  }
}

TEST_P(ATM90E32OffsetPersistence, RepeatedCalibrationZerosOffsetsAndWaitsForFreshMeasurements) {
  const auto &kind = GetParam();
  const bool power = kind.offset_type == OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_POWER;
  Access access;
  auto &component = access.component;
  auto &preference = access.make_preference(kind);
  save_values(preference, kind, SavedValue::SAVED_VALUE_OLD);
  access.restore(kind);
  component.set_enable_offset_calibration(true);
  const uint16_t first_offset = power ? component.power_offset_registers[0] : component.voltage_offset_registers[0];
  const uint16_t second_offset =
      power ? component.reactive_power_offset_registers[0] : component.current_offset_registers[0];
  for (uint8_t attempt = 0; attempt < 2; ++attempt) {
    SCOPED_TRACE(attempt);
    for (uint8_t phase = 0; phase < 3; ++phase) {
      access.delegate.set_register((power ? ATM90E32_REGISTER_PMEAN : ATM90E32_REGISTER_URMS) + phase, 0);
      access.delegate.set_register((power ? ATM90E32_REGISTER_PMEANLSB : ATM90E32_REGISTER_URMSLSB) + phase, 0);
      access.delegate.set_register((power ? ATM90E32_REGISTER_QMEAN : ATM90E32_REGISTER_IRMS) + phase, 0);
      access.delegate.set_register((power ? ATM90E32_REGISTER_QMEANLSB : ATM90E32_REGISTER_IRMSLSB) + phase, 0);
    }
    access.run_offsets(kind);
    EXPECT_EQ(access.read16(first_offset), 0);
    EXPECT_EQ(access.read16(second_offset), 0);
    if (attempt == 0)
      expect_values(preference, kind, SavedValue::SAVED_VALUE_OLD);
    // Old compensated samples remain zero until the chip completes a new measurement window.
    for (uint8_t phase = 0; phase < 3; ++phase) {
      access.delegate.set_register((power ? ATM90E32_REGISTER_PMEAN : ATM90E32_REGISTER_URMS) + phase, power ? 0 : 4);
      access.delegate.set_register((power ? ATM90E32_REGISTER_PMEANLSB : ATM90E32_REGISTER_URMSLSB) + phase,
                                   power ? 1024 : 0);
      access.delegate.set_register((power ? ATM90E32_REGISTER_QMEAN : ATM90E32_REGISTER_IRMS) + phase,
                                   power ? 0xFFFF : 2);
      access.delegate.set_register((power ? ATM90E32_REGISTER_QMEANLSB : ATM90E32_REGISTER_IRMSLSB) + phase,
                                   power ? 0xFE00 : 0);
    }
    const auto output = capture_calibration_logs([&]() { access.settle_calibration(); });
    EXPECT_NE(output.find(power ? "Power Offset Calibration" : "Offset Calibration"), std::string::npos);
    EXPECT_EQ(static_cast<int16_t>(access.read16(first_offset)), power ? -1024 : -2048);
    EXPECT_EQ(static_cast<int16_t>(access.read16(second_offset)), power ? 512 : -1024);
  }
}

TEST_P(ATM90E32OffsetPersistence, CorruptCalibrationSampleRollsBackWithoutSaving) {
  const auto &kind = GetParam();
  Access access;
  auto &preference = access.make_preference(kind);
  save_values(preference, kind, SavedValue::SAVED_VALUE_OLD);
  access.restore(kind);
  access.component.set_enable_offset_calibration(true);
  access.run_offsets(kind);
  access.delegate.corrupt_read(kind.offset_type == OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_POWER
                                   ? ATM90E32_REGISTER_PMEANLSB
                                   : ATM90E32_REGISTER_URMSLSB);

  access.settle_calibration();

  expect_values(preference, kind, SavedValue::SAVED_VALUE_OLD);
  access.expect_previous_runtime_values(kind, true);
}

TEST_P(ATM90E32OffsetPersistence, BlocksMeasurementsAndActionsUntilCompensatedSamplesAreReady) {
  const auto &kind = GetParam();
  Access access;
  auto &component = access.component;
  for (const auto &table : CALIBRATIONS) {
    save_values(access.make_preference(table), table, SavedValue::SAVED_VALUE_OLD);
    access.restore(table);
  }
  component.set_enable_offset_calibration(true);
  component.set_enable_gain_calibration(true);
  sensor::Sensor voltage;
  component.set_voltage_sensor(0, &voltage);
  voltage.publish_state(42.0f);
  access.delegate.set_register(ATM90E32_REGISTER_METEREN, 1);
  access.delegate.set_register(ATM90E32_REGISTER_URMS, 400);
  access.run_offsets(kind);

  component.run_gain_calibrations();
  component.run_offset_calibrations();
  component.run_power_offset_calibrations();
  for (const auto &table : CALIBRATIONS) {
    (component.*table.clear)();
    expect_values(access.preference(table), table, SavedValue::SAVED_VALUE_OLD);
  }
  access.expect_previous_runtime_values(Access::GAIN, true);
  component.update();
  component.loop();
  EXPECT_FLOAT_EQ(voltage.get_state(), 42.0f);

  const auto sample_output = capture_calibration_logs([&]() { access.advance_calibration(); });
  EXPECT_EQ(sample_output.find("completed and verified"), std::string::npos);
  component.update();
  component.loop();
  EXPECT_FLOAT_EQ(voltage.get_state(), 42.0f);

  const auto completion_output = capture_calibration_logs([&]() { access.settle_calibration(); });
  EXPECT_NE(completion_output.find("completed and verified"), std::string::npos);
  access.delegate.set_register(ATM90E32_REGISTER_URMS, 100);
  component.update();
  component.loop();
  EXPECT_FLOAT_EQ(voltage.get_state(), 1.0f);
}

TEST_P(ATM90E32OffsetPersistence, FailedZeroingRestoresPreviousCalibration) {
  const auto &kind = GetParam();
  Access access;
  auto &preference = access.make_preference(kind);
  save_values(preference, kind, SavedValue::SAVED_VALUE_OLD);
  access.restore(kind);
  access.component.set_enable_offset_calibration(true);
  access.delegate.drop_next_calibration_write();

  access.run_offsets(kind);
  access.settle_calibration();

  expect_values(preference, kind, SavedValue::SAVED_VALUE_OLD);
  access.expect_previous_runtime_values(kind, true);
  EXPECT_TRUE(access.component.*kind.restored);
}

TEST_P(ATM90E32CalibrationPersistence, RestoreVerifiesStoredValuesAndConfigFallback) {
  const auto &kind = GetParam();
  for (const bool stored : {false, true}) {
    for (const bool corrupt : {false, true}) {
      for (const bool drop : {false, true}) {
        SCOPED_TRACE(::testing::Message() << "stored=" << stored << ", corrupt=" << corrupt << ", drop=" << drop);
        Access access;
        auto &component = access.component;
        auto &preference = access.make_preference(kind);
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
          access.delegate.corrupt_calibration_writes();
        if (drop)
          access.delegate.drop_next_calibration_write();

        const auto output = capture_calibration_logs([&]() { access.restore(kind); });

        const bool restored = stored && !corrupt && !drop;
        EXPECT_EQ(component.*kind.has_stored, stored);
        EXPECT_EQ(component.*kind.restored, restored);
        if (!kind.gain) {
          component.set_enable_offset_calibration(true);
          const auto dump = capture_calibration_logs([&]() { component.dump_config(); });
          const char *marker = kind.offset_type == OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_POWER
                                   ? "Power offset calibration restore verified."
                                   : "Offset calibration restore verified.";
          EXPECT_EQ(dump.find(marker) != std::string::npos, restored);
        }
        expect_values(preference, kind, stored ? SavedValue::SAVED_VALUE_OLD : SavedValue::SAVED_VALUE_ZERO);
        if (!restored)
          access.expect_previous_runtime_values(kind);
        if (corrupt) {
          EXPECT_NE(output.find("config fallback both failed verification"), std::string::npos);
        } else if (drop) {
          EXPECT_NE(output.find("config values verified"), std::string::npos);
        }
      }
    }
  }
}

TEST_P(ATM90E32OffsetPersistence, ReconnectReportsVerifiedRestoreWithAndWithoutConfigMismatch) {
  const auto &kind = GetParam();
  const bool power = kind.offset_type == OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_POWER;
  const char *marker = power ? "Power offset calibration restore verified." : "Offset calibration restore verified.";
  for (const bool mismatch : {false, true}) {
    Access access;
    auto &component = access.component;
    component.set_enable_offset_calibration(true);
    if (mismatch) {
      if (power)
        component.set_active_power_offset(0, 999);
      else
        component.set_voltage_offset(0, 999);
    }
    save_values(access.make_preference(kind), kind, SavedValue::SAVED_VALUE_OLD);
    access.restore(kind);
    ASSERT_TRUE(component.*kind.restored);
    EXPECT_EQ((component.*kind.mismatches)[0], mismatch);
    for (unsigned reconnect = 0; reconnect < 2; ++reconnect) {
      const auto output = capture_calibration_logs([&]() { component.dump_config(); });
      const auto position = output.find(marker);
      ASSERT_NE(position, std::string::npos);
      EXPECT_EQ(output.find(marker, position + 1), std::string::npos);
    }
    component.set_enable_offset_calibration(false);
    const auto disabled = capture_calibration_logs([&]() { component.dump_config(); });
    EXPECT_EQ(disabled.find(marker), std::string::npos);
  }
}

TEST_P(ATM90E32CalibrationPersistence, FailedPersistencePreservesStoredPayloadAndPreviousRuntimeState) {
  const auto &kind = GetParam();
  for (const bool restored : {false, true}) {
    SCOPED_TRACE(restored);
    Access access;
    access.fail_sync();
    auto &preference = access.prepare_finish(kind);
    save_values(preference, kind, SavedValue::SAVED_VALUE_OLD);
    access.set_state(kind, true, restored);

    const auto output = capture_calibration_logs([&]() { access.finish(kind, restored); });

    expect_values(preference, kind, SavedValue::SAVED_VALUE_OLD);
    access.expect_previous_runtime_values(kind, restored);
    EXPECT_TRUE(access.component.*kind.has_stored);
    EXPECT_EQ(access.component.*kind.restored, restored);
    EXPECT_EQ(output.find("completed and verified"), std::string::npos);
    // The CircuitSetup calibration helper matches this line, so its text must not change.
    if (kind.gain)
      EXPECT_NE(output.find("Failed to save gain calibration to memory!"), std::string::npos);
  }
}

TEST_P(ATM90E32CalibrationPersistence, StoredPayloadLoadFailureRefusesOverwrite) {
  const auto &kind = GetParam();
  Access access;
  const uint32_t key = Access::next_preference_key();
  access.prepare_finish(kind, key);
  auto raw_preference = global_preferences->make_preference<uint32_t>(key, true);
  constexpr uint32_t raw_value = 0xA5A55A5A;
  ASSERT_TRUE(raw_preference.save(&raw_value));
  ASSERT_TRUE(global_preferences->sync());
  access.set_state(kind, true, false);

  const auto output = capture_calibration_logs([&]() { access.finish(kind, false); });

  uint32_t actual = 0;
  ASSERT_TRUE(raw_preference.load(&actual));
  EXPECT_EQ(actual, raw_value);
  EXPECT_TRUE(access.component.*kind.has_stored);
  EXPECT_FALSE(access.component.*kind.restored);
  EXPECT_NE(output.find("refusing to overwrite them"), std::string::npos);
  EXPECT_EQ(output.find("completed and verified"), std::string::npos);
}

TEST_P(ATM90E32CalibrationPersistence, FreshPersistenceUsesZeroSentinelAndCanRetry) {
  const auto &kind = GetParam();
  Access access;
  access.fail_sync();
  auto &preference = access.prepare_finish(kind);
  access.set_state(kind, false, false);

  const auto output = capture_calibration_logs([&]() { access.finish(kind, false); });

  expect_values(preference, kind, SavedValue::SAVED_VALUE_ZERO);
  EXPECT_FALSE(access.component.*kind.has_stored);
  EXPECT_FALSE(access.component.*kind.restored);
  EXPECT_EQ(output.find("completed and verified"), std::string::npos);

  access.allow_sync();
  access.set_new_calibration(kind);
  const auto retry_output = capture_calibration_logs([&]() { access.finish(kind, false); });

  expect_values(preference, kind, SavedValue::SAVED_VALUE_NEW);
  EXPECT_TRUE(access.component.*kind.has_stored);
  EXPECT_TRUE(access.component.*kind.restored);
  EXPECT_NE(retry_output.find("completed and verified"), std::string::npos);
}

static void check_clear_retry(const CalibrationTraits &kind, bool other_restored) {
  Access access;
  auto &component = access.component;
  access.fail_sync();
  auto &preference = access.prepare_finish(kind);
  save_values(preference, kind, SavedValue::SAVED_VALUE_OLD);
  access.set_state(kind, true, true);
  component.*kind.other_restored = other_restored;
  (component.*kind.mismatches)[1] = true;

  const auto first_output = capture_calibration_logs([&]() { (component.*kind.clear)(); });

  EXPECT_TRUE(component.*kind.has_stored);
  EXPECT_TRUE(component.*kind.restored);
  EXPECT_EQ(component.*kind.other_restored, other_restored);
  EXPECT_TRUE((component.*kind.mismatches)[1]);
  expect_values(preference, kind, SavedValue::SAVED_VALUE_OLD);
  EXPECT_NE(first_output.find("Failed to clear"), std::string::npos);
  EXPECT_EQ(first_output.find("Offsets cleared."), std::string::npos);
  EXPECT_EQ(first_output.find("Power offsets cleared."), std::string::npos);

  access.allow_sync();
  const auto second_output = capture_calibration_logs([&]() { (component.*kind.clear)(); });

  EXPECT_FALSE(component.*kind.has_stored);
  EXPECT_FALSE(component.*kind.restored);
  EXPECT_EQ(component.*kind.other_restored, other_restored);
  expect_values(preference, kind, SavedValue::SAVED_VALUE_ZERO);
  EXPECT_EQ(second_output.find("Failed to clear"), std::string::npos);
}

TEST_P(ATM90E32CalibrationPersistence, ClearFailureRetainsRetryEligibility) {
  check_clear_retry(GetParam(), false);
  check_clear_retry(GetParam(), true);
}

TEST_P(ATM90E32OffsetPersistence, RollbackReadbackFailureInvalidatesState) {
  const auto &kind = GetParam();
  Access access;
  auto &component = access.component;
  access.fail_sync();
  auto &preference = access.prepare_finish(kind);
  save_values(preference, kind, SavedValue::SAVED_VALUE_OLD);
  access.set_state(kind, true, true);
  component.*kind.other_restored = true;
  for (bool &mismatch : component.*kind.mismatches)
    mismatch = true;
  access.delegate.corrupt_calibration_writes();

  const auto output = capture_calibration_logs([&]() { access.finish(kind, true); });

  EXPECT_TRUE(component.*kind.has_stored);
  EXPECT_FALSE(component.*kind.restored);
  EXPECT_TRUE(component.*kind.other_restored);
  access.expect_mismatches_cleared(kind);
  EXPECT_NE(output.find("rollback readback verification failed"), std::string::npos);
  EXPECT_EQ(output.find("completed and verified"), std::string::npos);
}

TEST(ATM90E32CalibrationPersistence, OffsetRestoreFallbackPreservesOtherSavedCategories) {
  Access access;
  auto &component = access.component;
  save_values(access.make_preference(Access::OFFSET), Access::OFFSET, SavedValue::SAVED_VALUE_OLD);
  component.*Access::POWER_OFFSET.restored = true;
  component.*Access::GAIN.restored = true;
  for (uint8_t phase = 0; phase < 3; ++phase) {
    component.set_voltage_offset(phase, static_cast<int16_t>(101 + phase));
    component.set_current_offset(phase, static_cast<int16_t>(-201 - phase));
  }
  access.delegate.drop_next_calibration_write();

  const auto output = capture_calibration_logs([&]() { access.restore(Access::OFFSET); });

  EXPECT_FALSE(component.*Access::OFFSET.restored);
  EXPECT_TRUE(component.*Access::POWER_OFFSET.restored);
  EXPECT_TRUE(component.*Access::GAIN.restored);
  access.expect_mismatches_cleared(Access::OFFSET);
  EXPECT_NE(output.find("config values verified"), std::string::npos);
}

TEST(ATM90E32CalibrationPersistence, GainTableEqualToConfigIsBackedUpAndCleared) {
  const auto &kind = Access::GAIN;
  Access access;
  auto &component = access.component;
  access.fail_sync();
  auto &preference = access.make_preference(kind);
  save_values(preference, kind, SavedValue::SAVED_VALUE_OLD);
  for (uint8_t phase = 0; phase < 3; ++phase) {
    const auto value = gain_values(SavedValue::SAVED_VALUE_OLD, phase);
    component.set_volt_gain(phase, value.voltage_gain);
    component.set_ct_gain(phase, value.current_gain);
  }

  access.restore(kind);

  EXPECT_TRUE(component.*kind.has_stored);
  EXPECT_FALSE(component.*kind.restored);

  access.set_new_calibration(kind);
  access.finish(kind, false);

  EXPECT_TRUE(component.*kind.has_stored);
  expect_values(preference, kind, SavedValue::SAVED_VALUE_OLD);

  access.allow_sync();
  const auto output = capture_calibration_logs([&]() { component.clear_gain_calibrations(); });

  EXPECT_FALSE(component.*kind.has_stored);
  expect_values(preference, kind, SavedValue::SAVED_VALUE_ZERO);
  EXPECT_EQ(output.find("No stored gain calibrations to clear"), std::string::npos);
  EXPECT_EQ(output.find("Failed to clear"), std::string::npos);
}

}  // namespace testing

#endif  // USE_HOST

}  // namespace esphome::atm90e32
