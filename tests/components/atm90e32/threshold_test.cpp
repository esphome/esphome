#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <type_traits>

#include "esphome/components/atm90e32/atm90e32.h"
#include "esphome/components/sensor/filter.h"
#include "esphome/components/host/preferences.h"
#include "esphome/components/text_sensor/text_sensor.h"

#ifndef __has_feature
#define __has_feature(x) 0
#endif
#if defined(__SANITIZE_ADDRESS__) || __has_feature(address_sanitizer)
#define ATM90E32_TEST_ASAN
extern "C" int __sanitizer_install_malloc_and_free_hooks(void (*malloc_hook)(const volatile void *, size_t),
                                                         void (*free_hook)(const volatile void *));
#endif

namespace esphome::atm90e32 {

static_assert(std::is_final_v<ATM90E32Component>);

class ThresholdRegisterDelegate : public spi::SPIDelegate {
 public:
  std::array<uint16_t, 1024> registers{};

  void begin_transaction() override {}
  void end_transaction() override {}
  uint8_t transfer(uint8_t data) override { return 0; }
  void transfer(uint8_t *data, size_t length) override {
    ASSERT_EQ(length, 4);
    const auto address = ((data[0] & 0x03) << 8) | data[1];
    const uint16_t value = this->registers[address];
    data[2] = value >> 8;
    data[3] = value & 0xFF;
    if (address != ATM90E32_REGISTER_LASTSPIDATA)
      this->registers[ATM90E32_REGISTER_LASTSPIDATA] = value;
  }
  void write_array(const uint8_t *data, size_t length) override {
    ASSERT_EQ(length, 4);
    const auto address = (data[0] << 8) | data[1];
    const uint16_t value = (data[2] << 8) | data[3];
    this->registers[address] = value;
    this->registers[ATM90E32_REGISTER_LASTSPIDATA] = value;
  }
};

class ATM90E32ThresholdRegisters : public ::testing::Test {
 protected:
  void SetUp() override {
    this->component_.delegate_ = &this->delegate_;
    this->component_.set_cs_pin(&this->cs_);
    this->component_.set_instance_id("threshold_test");
    this->component_.set_threshold_voltage_sag_v(96.0f);
    this->component_.set_threshold_voltage_peak_v(144.0f);
    this->component_.set_threshold_current_peak_a(65.53f, 0xC000);
    this->component_.set_frequency_thresholds(5995, 65535);
  }

  ThresholdRegisterDelegate delegate_;
  spi::NullPin cs_;
  ATM90E32Component component_;
  using GainCalibration = ATM90E32Component::GainCalibration;
  GainCalibration (&gain_phase_)[3]{this->component_.gain_phase_};
  ESPPreferenceObject &gain_calibration_pref_{this->component_.gain_calibration_pref_};
  void setup_component_() { this->component_.setup_registers_(); }
  void write_gains_() { this->component_.write_gains_to_registers_(); }
  void restore_gains_() { this->component_.restore_gain_calibrations_(); }
};

TEST(ATM90E32Threshold, SaturatesVoltageBeforeNarrowing) {
  EXPECT_EQ(ATM90E32Component::calculate_voltage_threshold_from_volts(292.8f, 7305), 65535);
}

TEST(ATM90E32Threshold, SaturatesCurrentBeforeNarrowing) {
  EXPECT_EQ(ATM90E32Component::calculate_current_threshold_from_amps(50.0f, 10000), 65535);
}

TEST(ATM90E32Threshold, HandlesZeroGain) {
  EXPECT_EQ(ATM90E32Component::calculate_voltage_threshold_from_volts(120.0f, 0), 0);
  EXPECT_EQ(ATM90E32Component::calculate_current_threshold_from_amps(10.0f, 0), 0);
}

class ATM90E32SinglePhaseThresholds : public ATM90E32ThresholdRegisters,
                                      public ::testing::WithParamInterface<uint8_t> {};

TEST_P(ATM90E32SinglePhaseThresholds, UsesConfiguredPhaseWithoutPhaseA) {
  const auto phase = GetParam();
  this->component_.set_volt_gain(phase, 7305);
  this->component_.set_ct_gain(phase, 27961);
  this->component_.set_threshold_current_peak_a(10.0f);
  this->setup_component_();

  EXPECT_NEAR(this->delegate_.registers[ATM90E32_REGISTER_SAGTH], 30450, 1);
  EXPECT_NEAR(this->delegate_.registers[ATM90E32_REGISTER_OVTH], 45675, 1);
  EXPECT_NEAR(this->delegate_.registers[ATM90E32_REGISTER_OITH], 8287, 1);
}

INSTANTIATE_TEST_SUITE_P(PhaseBAndC, ATM90E32SinglePhaseThresholds, ::testing::Values(1, 2));

TEST_F(ATM90E32ThresholdRegisters, RefreshesThresholdsWhenCalibratedGainsAreWritten) {
  this->component_.set_volt_gain(1, 7305);
  this->component_.set_ct_gain(1, 27961);
  this->component_.set_threshold_current_peak_a(10.0f);
  this->setup_component_();
  this->gain_phase_[1] = {14610, 13980};

  this->write_gains_();

  EXPECT_NEAR(this->delegate_.registers[ATM90E32_REGISTER_SAGTH], 15225, 1);
  EXPECT_NEAR(this->delegate_.registers[ATM90E32_REGISTER_OVTH], 22837, 1);
  EXPECT_NEAR(this->delegate_.registers[ATM90E32_REGISTER_OITH], 16574, 1);
}

TEST_F(ATM90E32ThresholdRegisters, UsesNativeVoltageLimitsForDifferentCalibratedGains) {
  text_sensor::TextSensor status_a, status_b;
  this->component_.set_phase_status_text_sensor(0, &status_a);
  this->component_.set_phase_status_text_sensor(1, &status_b);
  this->component_.set_volt_gain(0, 7305);
  this->component_.set_volt_gain(1, 14610);
  this->component_.set_ct_gain(0, 13980);
  this->component_.set_ct_gain(1, 27961);
  this->component_.set_threshold_current_peak_a(10.0f);
  this->setup_component_();

  EXPECT_NEAR(this->delegate_.registers[ATM90E32_REGISTER_SAGTH], 30450, 1);
  EXPECT_NEAR(this->delegate_.registers[ATM90E32_REGISTER_OVTH], 22837, 1);
  EXPECT_NEAR(this->delegate_.registers[ATM90E32_REGISTER_OITH], 8287, 1);

  this->delegate_.registers[ATM90E32_REGISTER_EMMSTATE0] =
      ATM90E32_STATUS_S0_OVPHASEAST | ATM90E32_STATUS_S0_OVPHASEBST;
  this->delegate_.registers[ATM90E32_REGISTER_EMMSTATE1] =
      ATM90E32_STATUS_S1_SAGPHASEAST | ATM90E32_STATUS_S1_SAGPHASEBST;
  this->delegate_.registers[ATM90E32_REGISTER_URMS] = 12000;
  this->delegate_.registers[ATM90E32_REGISTER_URMS + 1] = 12000;
  this->component_.update();
  EXPECT_EQ(status_a.state, "Okay");
  EXPECT_EQ(status_b.state, "Okay");

  this->delegate_.registers[ATM90E32_REGISTER_URMS] = 9500;
  this->delegate_.registers[ATM90E32_REGISTER_URMS + 1] = 14500;
  this->component_.update();
  EXPECT_EQ(status_a.state, "Voltage Sag");
  EXPECT_EQ(status_b.state, "Over Voltage");

  this->delegate_.registers[ATM90E32_REGISTER_URMS] = 9600;
  this->delegate_.registers[ATM90E32_REGISTER_URMS + 1] = 14400;
  this->component_.update();
  EXPECT_EQ(status_a.state, "Okay");
  EXPECT_EQ(status_b.state, "Okay");

  this->gain_phase_[0] = {7305, 13980};
  this->gain_phase_[1] = {7305, 27961};
  this->write_gains_();
  this->delegate_.registers[ATM90E32_REGISTER_EMMSTATE1] = 0;
  this->component_.update();
  EXPECT_EQ(status_a.state, "Over Voltage");
  EXPECT_EQ(status_b.state, "Over Voltage");
}

TEST_F(ATM90E32ThresholdRegisters, PreservesLegacyCurrentAndReadyMadeFrequencyRegisters) {
  this->component_.set_ct_gain(1, 10000);
  this->setup_component_();
  EXPECT_EQ(this->delegate_.registers[ATM90E32_REGISTER_OITH], 0xC000);
  EXPECT_EQ(this->delegate_.registers[ATM90E32_REGISTER_FREQLOTH], 5995);
  EXPECT_EQ(this->delegate_.registers[ATM90E32_REGISTER_FREQHITH], 65535);

  this->gain_phase_[1].current_gain = 5000;
  this->write_gains_();
  EXPECT_EQ(this->delegate_.registers[ATM90E32_REGISTER_OITH], 0xC000);
}

TEST_F(ATM90E32ThresholdRegisters, ClampsWhenCalibrationExceedsTheRegisterRange) {
  this->component_.set_volt_gain(1, 7305);
  this->component_.set_ct_gain(1, 27961);
  this->component_.set_threshold_current_peak_a(10.0f);
  this->setup_component_();
  this->gain_phase_[1] = {1000, 1000};

  this->write_gains_();

  EXPECT_EQ(this->delegate_.registers[ATM90E32_REGISTER_SAGTH], 65535);
  EXPECT_EQ(this->delegate_.registers[ATM90E32_REGISTER_OVTH], 65535);
  EXPECT_EQ(this->delegate_.registers[ATM90E32_REGISTER_OITH], 65535);
}

#ifdef USE_HOST
TEST_F(ATM90E32ThresholdRegisters, RefreshesThresholdsAfterRestoringAndClearingStoredGains) {
  if (global_preferences == nullptr)
    host::setup_preferences();
  this->component_.set_volt_gain(1, 7305);
  this->component_.set_ct_gain(1, 27961);
  this->component_.set_threshold_current_peak_a(10.0f);
  this->setup_component_();
  auto preference = global_preferences->make_preference<GainCalibration[3]>(0x15244000);
  const GainCalibration saved[3]{{0, 0}, {14610, 13980}, {0, 0}};
  ASSERT_TRUE(preference.save(&saved));
  this->gain_calibration_pref_ = preference;

  this->restore_gains_();

  EXPECT_TRUE(this->component_.using_saved_calibrations_);
  EXPECT_NEAR(this->delegate_.registers[ATM90E32_REGISTER_SAGTH], 15225, 1);
  EXPECT_NEAR(this->delegate_.registers[ATM90E32_REGISTER_OVTH], 22837, 1);
  EXPECT_NEAR(this->delegate_.registers[ATM90E32_REGISTER_OITH], 16574, 1);

  this->component_.clear_gain_calibrations();

  EXPECT_FALSE(this->component_.using_saved_calibrations_);
  EXPECT_NEAR(this->delegate_.registers[ATM90E32_REGISTER_SAGTH], 30450, 1);
  EXPECT_NEAR(this->delegate_.registers[ATM90E32_REGISTER_OVTH], 45675, 1);
  EXPECT_NEAR(this->delegate_.registers[ATM90E32_REGISTER_OITH], 8287, 1);
}
#endif  // USE_HOST

TEST_F(ATM90E32ThresholdRegisters, CombinesNativeCurrentWithOtherPhaseAlarms) {
  sensor::Sensor current;
  sensor::MultiplyFilter multiply([]() { return 2.0f; });
  current.add_filter(&multiply);
  current.publish_state(6.0f);
  ASSERT_FLOAT_EQ(current.state, 12.0f);
  text_sensor::TextSensor status;
  this->component_.set_ct_gain(1, 27961);
  this->component_.set_current_sensor(1, &current);
  this->component_.set_phase_status_text_sensor(1, &status);
  this->component_.set_threshold_current_peak_a(10.0f);
  this->setup_component_();
  this->delegate_.registers[ATM90E32_REGISTER_EMMSTATE0] = ATM90E32_STATUS_S0_OVPHASEBST;
  this->delegate_.registers[ATM90E32_REGISTER_IRMS + 1] = 6000;

  this->component_.update();
  EXPECT_EQ(status.state, "Over Voltage");

  this->delegate_.registers[ATM90E32_REGISTER_IRMS + 1] = 12000;
  this->component_.update();
  EXPECT_EQ(status.state, "Over Current; Over Voltage");
}

TEST_F(ATM90E32ThresholdRegisters, IgnoresLegacyCurrentFlagAndPreservesRmsLimit) {
  text_sensor::TextSensor status;
  this->component_.set_ct_gain(1, 10000);
  this->component_.set_phase_status_text_sensor(1, &status);
  this->setup_component_();
  this->delegate_.registers[ATM90E32_REGISTER_EMMSTATE0] = ATM90E32_STATUS_S0_OIPHASEBST;
  this->delegate_.registers[ATM90E32_REGISTER_IRMS + 1] = 30000;

  this->component_.update();
  EXPECT_EQ(status.state, "Okay");

  this->delegate_.registers[ATM90E32_REGISTER_IRMS + 1] = 65535;
  this->component_.update();
  EXPECT_EQ(status.state, "Over Current");

  this->delegate_.registers[ATM90E32_REGISTER_EMMSTATE0] |= ATM90E32_STATUS_S0_OVPHASEBST;
  this->delegate_.registers[ATM90E32_REGISTER_IRMS + 1] = 30000;
  this->component_.update();
  EXPECT_EQ(status.state, "Over Voltage");
}

TEST_F(ATM90E32ThresholdRegisters, ReportsConfiguredCurrentFlagBelowRmsLimit) {
  text_sensor::TextSensor status;
  this->component_.set_ct_gain(1, 27961);
  this->component_.set_phase_status_text_sensor(1, &status);
  this->component_.set_threshold_current_peak_a(10.0f);
  this->setup_component_();
  this->delegate_.registers[ATM90E32_REGISTER_EMMSTATE0] =
      ATM90E32_STATUS_S0_OIPHASEBST | ATM90E32_STATUS_S0_OVPHASEBST;
  this->delegate_.registers[ATM90E32_REGISTER_IRMS + 1] = 6000;

  this->component_.update();
  EXPECT_EQ(status.state, "Over Current; Over Voltage");
}

TEST_F(ATM90E32ThresholdRegisters, RepeatedCombinedAlarmsDoNotAllocate) {
#ifdef ATM90E32_TEST_ASAN
  static std::atomic<size_t> allocations{0};
  static const int hooks_installed = __sanitizer_install_malloc_and_free_hooks(
      [](const volatile void *, size_t) { allocations.fetch_add(1, std::memory_order_relaxed); },
      [](const volatile void *) {});
  ASSERT_NE(hooks_installed, 0);
  text_sensor::TextSensor status;
  this->component_.set_ct_gain(1, 27961);
  this->component_.set_phase_status_text_sensor(1, &status);
  this->component_.set_threshold_current_peak_a(10.0f);
  this->setup_component_();
  this->delegate_.registers[ATM90E32_REGISTER_EMMSTATE0] =
      ATM90E32_STATUS_S0_OIPHASEBST | ATM90E32_STATUS_S0_OVPHASEBST;
  this->delegate_.registers[ATM90E32_REGISTER_EMMSTATE1] =
      ATM90E32_STATUS_S1_SAGPHASEBST | ATM90E32_STATUS_S1_PHASELOSSBST;
  this->component_.check_phase_status();
  ASSERT_EQ(status.state, "Over Current; Over Voltage; Voltage Sag; Phase Loss");
  const size_t before = allocations.load(std::memory_order_relaxed);
  for (unsigned i = 0; i < 10; i++)
    this->component_.check_phase_status();
  const size_t after = allocations.load(std::memory_order_relaxed);
  EXPECT_EQ(after - before, 0u);
#else
  GTEST_SKIP() << "Allocation counting requires AddressSanitizer";
#endif
}

}  // namespace esphome::atm90e32
