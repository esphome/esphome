#include <gtest/gtest.h>

#include <array>

#include "esphome/components/atm90e32/atm90e32.h"
#include "esphome/components/sensor/filter.h"
#include "esphome/components/host/preferences.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::atm90e32::testing {

class ThresholdTestComponent : public ATM90E32Component {
 public:
  void spi_setup() override {}
  using ATM90E32Component::delegate_;
  using ATM90E32Component::gain_calibration_pref_;
  using ATM90E32Component::GainCalibration;
  using ATM90E32Component::gain_phase_;
  using ATM90E32Component::restore_gain_calibrations_;
  using ATM90E32Component::write_gains_to_registers_;
};

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
  ThresholdTestComponent component_;
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
  this->component_.setup();

  EXPECT_NEAR(this->delegate_.registers[ATM90E32_REGISTER_SAGTH], 30450, 1);
  EXPECT_NEAR(this->delegate_.registers[ATM90E32_REGISTER_OVTH], 45675, 1);
  EXPECT_NEAR(this->delegate_.registers[ATM90E32_REGISTER_OITH], 8287, 1);
}

INSTANTIATE_TEST_SUITE_P(PhaseBAndC, ATM90E32SinglePhaseThresholds, ::testing::Values(1, 2));

TEST_F(ATM90E32ThresholdRegisters, RefreshesThresholdsWhenCalibratedGainsAreWritten) {
  this->component_.set_volt_gain(1, 7305);
  this->component_.set_ct_gain(1, 27961);
  this->component_.set_threshold_current_peak_a(10.0f);
  this->component_.setup();
  this->component_.gain_phase_[1] = {14610, 13980};

  this->component_.write_gains_to_registers_();

  EXPECT_NEAR(this->delegate_.registers[ATM90E32_REGISTER_SAGTH], 15225, 1);
  EXPECT_NEAR(this->delegate_.registers[ATM90E32_REGISTER_OVTH], 22837, 1);
  EXPECT_NEAR(this->delegate_.registers[ATM90E32_REGISTER_OITH], 16574, 1);
}

TEST_F(ATM90E32ThresholdRegisters, UsesConservativeSharedLimitsForDifferentCalibratedGains) {
  this->component_.set_volt_gain(0, 7305);
  this->component_.set_volt_gain(1, 14610);
  this->component_.set_ct_gain(0, 13980);
  this->component_.set_ct_gain(1, 27961);
  this->component_.set_threshold_current_peak_a(10.0f);
  this->component_.setup();

  EXPECT_NEAR(this->delegate_.registers[ATM90E32_REGISTER_SAGTH], 30450, 1);
  EXPECT_NEAR(this->delegate_.registers[ATM90E32_REGISTER_OVTH], 22837, 1);
  EXPECT_NEAR(this->delegate_.registers[ATM90E32_REGISTER_OITH], 8287, 1);
}

TEST_F(ATM90E32ThresholdRegisters, PreservesLegacyCurrentAndReadyMadeFrequencyRegisters) {
  this->component_.set_ct_gain(1, 10000);
  this->component_.setup();
  EXPECT_EQ(this->delegate_.registers[ATM90E32_REGISTER_OITH], 0xC000);
  EXPECT_EQ(this->delegate_.registers[ATM90E32_REGISTER_FREQLOTH], 5995);
  EXPECT_EQ(this->delegate_.registers[ATM90E32_REGISTER_FREQHITH], 65535);

  this->component_.gain_phase_[1].current_gain = 5000;
  this->component_.write_gains_to_registers_();
  EXPECT_EQ(this->delegate_.registers[ATM90E32_REGISTER_OITH], 0xC000);
}

TEST_F(ATM90E32ThresholdRegisters, ClampsWhenCalibrationExceedsTheRegisterRange) {
  this->component_.set_volt_gain(1, 7305);
  this->component_.set_ct_gain(1, 27961);
  this->component_.set_threshold_current_peak_a(10.0f);
  this->component_.setup();
  this->component_.gain_phase_[1] = {1000, 1000};

  this->component_.write_gains_to_registers_();

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
  this->component_.setup();
  auto preference = global_preferences->make_preference<ThresholdTestComponent::GainCalibration[3]>(0x15244000);
  const ThresholdTestComponent::GainCalibration saved[3]{{0, 0}, {14610, 13980}, {0, 0}};
  ASSERT_TRUE(preference.save(&saved));
  this->component_.gain_calibration_pref_ = preference;

  this->component_.restore_gain_calibrations_();

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
  this->component_.setup();
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
  this->component_.setup();
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
  this->component_.setup();
  this->delegate_.registers[ATM90E32_REGISTER_EMMSTATE0] =
      ATM90E32_STATUS_S0_OIPHASEBST | ATM90E32_STATUS_S0_OVPHASEBST;
  this->delegate_.registers[ATM90E32_REGISTER_IRMS + 1] = 6000;

  this->component_.update();
  EXPECT_EQ(status.state, "Over Current; Over Voltage");
}

}  // namespace esphome::atm90e32::testing
