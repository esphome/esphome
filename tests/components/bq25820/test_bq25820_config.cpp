#include "common.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/hal.h"

#include <cmath>

namespace esphome::bq25820::testing {

// Every option away from its default, with the 40 s watchdog, as code generation packs packed_config.yaml
// (checked in tests/component_tests/bq25820/test_init.py), except IPRECHG: 0 here covers a limit that keeps
// its reset value
static constexpr BQ25820Config SET_CONFIG{
    .limits = {240, 0, 256, 8000, 660},
    .controls = {0x17, 0x9C, 0x04, 0x03, 0xFF, 0x00, 0xCC},
    .ibat_scale = 0.002f,
    .iac_scale = 0.002f,
};

class BQ25820ConfigTest : public ::testing::Test {
 protected:
  void SetUp() override { attach(this->hub_, this->bus_); }

  uint8_t &reg_(uint8_t address) { return this->bus_.registers[address]; }

  RegisterBus bus_;
  BQ25820Component hub_{&SET_CONFIG, true};
  CancelDelayedPoll cancel_poll_{this->hub_};
};

TEST_F(BQ25820ConfigTest, FirstPollWritesEachRegisterInTableOrder) {
  // Bits the configuration does not own, which must survive
  this->reg_(REG_TIMER_CONTROL) = 0x01;  // EN_TMR2X
  this->reg_(0x19) = 0xFB;
  this->reg_(REG_MPPT_CONTROL) = 0x78;
  this->reg_(0x1C) = 0xFE;
  this->reg_(REG_ADC_CONTROL) = 0x3F;
  this->reg_(0x00) = 0xE0;
  this->reg_(0x10) = 0x5A;  // IPRECHG is left at its value with a 0 code
  this->hub_.update();
  // 16-bit limits, LSB first
  EXPECT_EQ(this->reg_(0x02), 240);
  EXPECT_EQ(this->reg_(0x03), 0);
  EXPECT_EQ(this->reg_(0x06), 0x00);
  EXPECT_EQ(this->reg_(0x07), 0x01);
  EXPECT_EQ(this->reg_(0x08), 8000 & 0xFF);
  EXPECT_EQ(this->reg_(0x09), 8000 >> 8);
  EXPECT_EQ(this->reg_(0x0C), 660 & 0xFF);
  EXPECT_EQ(this->reg_(0x0D), 660 >> 8);
  EXPECT_EQ(this->reg_(0x10), 0x5A);
  // 8-bit registers, masked
  EXPECT_EQ(this->reg_(0x00), 0xF7);
  EXPECT_EQ(this->reg_(REG_TIMER_CONTROL), 0x9D);
  EXPECT_EQ(this->reg_(0x19), 0xFF);
  EXPECT_EQ(this->reg_(REG_MPPT_CONTROL), 0xFB);  // with FORCE_SWEEP
  EXPECT_EQ(this->reg_(0x1B), 0xFF);
  EXPECT_EQ(this->reg_(0x1C), 0xFE);
  EXPECT_EQ(this->reg_(REG_ADC_CONTROL), 0xBF);
  // The charger control write, then WD_RST to feed the watchdog
  EXPECT_EQ(this->reg_(REG_CHARGER_CONTROL), (0xCC | WD_RST));
}

TEST_F(BQ25820ConfigTest, EveryPollFeedsTheWatchdog) {
  this->hub_.update();
  for (int i = 0; i < 3; i++) {
    this->reg_(REG_CHARGER_CONTROL) = 0;
    this->hub_.update();
    EXPECT_EQ(this->reg_(REG_CHARGER_CONTROL), (0xCC | WD_RST));
  }
}

TEST_F(BQ25820ConfigTest, WatchdogExpiryRewritesTheConfigurationWithoutAnotherSweep) {
  this->hub_.update();
  this->reg_(REG_TIMER_CONTROL) = 0;
  this->reg_(REG_MPPT_CONTROL) = 0;
  this->hub_.update();
  EXPECT_EQ(this->reg_(REG_TIMER_CONTROL), 0);  // not written again while WD_STAT is clear
  this->reg_(REG_CHARGER_STATUS_1) = WD_STAT;
  this->reg_(REG_CHARGER_CONTROL) = 0;
  this->hub_.update();
  EXPECT_EQ(this->reg_(REG_TIMER_CONTROL), 0x9C);
  EXPECT_EQ(this->reg_(REG_MPPT_CONTROL), 0x03);  // the panel sweep only runs once
  // The recovery poll feeds the watchdog too, which is what clears WD_STAT on the chip
  EXPECT_EQ(this->reg_(REG_CHARGER_CONTROL), (0xCC | WD_RST));
}

TEST(BQ25820WatchdogOff, PollsNeitherFeedNorRewrite) {
  RegisterBus bus;
  BQ25820Component hub{&DEFAULT_CONFIG, false};
  CancelDelayedPoll cancel_poll{hub};
  attach(hub, bus);
  hub.update();
  EXPECT_EQ(bus.registers[REG_CHARGER_CONTROL], 0xC9);
  EXPECT_EQ(bus.registers[REG_TIMER_CONTROL], 0x0A);
  bus.registers[REG_TIMER_CONTROL] = 0;
  bus.registers[REG_CHARGER_STATUS_1] = WD_STAT;
  hub.update();
  EXPECT_EQ(bus.registers[REG_TIMER_CONTROL], 0);
  EXPECT_EQ(bus.registers[REG_CHARGER_CONTROL], 0xC9);
}

TEST(BQ25820ChipId, OnlyThePartNumberIsChecked) {
  // PART_INFORMATION (0x3D): PART_NUM[5:3], DEV_REV[2:0]
  for (uint8_t part_information : {0x1A, 0x18, 0x1F, 0x22, 0x00}) {
    SCOPED_TRACE(part_information);
    RegisterBus bus;
    BQ25820Component hub{&DEFAULT_CONFIG, false};
    CancelDelayedPoll cancel_poll{hub};
    attach(hub, bus);
    bus.registers[0x3D] = part_information;
    hub.setup();
    EXPECT_EQ(hub.is_failed(), (part_information & 0x38) != 0x18);
  }
}

TEST(BQ25820Temperature, FollowsTheBParameterCalibration) {
  // A 100k B3950 NTC rated at 30 C with a 47k resistor from REGN
  static constexpr float B = 3950.0f;
  static constexpr float R0 = 100000.0f;
  static constexpr float T0 = 30.0f + 273.15f;
  static constexpr float PULL_UP = 47000.0f;
  RegisterBus bus;
  BQ25820Component hub{&DEFAULT_CONFIG, false};
  CancelDelayedPoll cancel_poll{hub};
  sensor::Sensor temperature;
  attach(hub, bus);
  hub.set_temp_in_c_sensor(&temperature);
  hub.set_ntc_calibration(1.0f / T0 + logf(PULL_UP / R0) / B, 1.0f / B);
  hub.update();  // configures the chip and restarts the ADC
  run_delayed_poll();
  for (uint16_t raw : {100, 300, 512, 700, 900}) {
    SCOPED_TRACE(raw);
    // TS_ADC (0x37), LSB first
    bus.registers[0x37] = raw & 0xFF;
    bus.registers[0x38] = raw >> 8;
    hub.update();
    const float ratio = raw / 1024.0f;
    const float r_ntc = PULL_UP * ratio / (1.0f - ratio);
    const float expected = 1.0f / (1.0f / T0 + logf(r_ntc / R0) / B) - 273.15f;
    EXPECT_NEAR(temperature.state, expected, 0.01f);
  }
  // Half of REGN reads the rated temperature when the resistor matches the NTC
  hub.set_ntc_calibration(1.0f / T0, 1.0f / B);
  bus.registers[0x37] = 0x00;
  bus.registers[0x38] = 0x02;
  hub.update();
  EXPECT_NEAR(temperature.state, 30.0f, 0.01f);
  // A shorted or an open NTC is unknown, not a temperature
  for (uint16_t raw : {0, 1020, 1023}) {
    SCOPED_TRACE(raw);
    bus.registers[0x37] = raw & 0xFF;
    bus.registers[0x38] = raw >> 8;
    hub.update();
    EXPECT_TRUE(std::isnan(temperature.state));
  }
}

static int status_publishes = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// A chip at its reset values, with EN_CHG set, and a hub with the default configuration
class BQ25820WriteFailureTest : public ::testing::Test {
 protected:
  void SetUp() override {
    status_publishes = 0;
    attach(this->hub_, this->bus_);
    this->hub_.set_status_publisher([](const BQ25820StatusRegisters &) { status_publishes++; });
    this->hub_.set_vbat_sensor(&this->vbat_);
    this->bus_.registers[0x3D] = 0x18;  // PART_INFORMATION
    this->bus_.registers[REG_CHARGER_CONTROL] = 0xC9;
    // VBAT_ADC (0x33): 4000 steps, 8 V
    this->bus_.registers[0x33] = 4000 & 0xFF;
    this->bus_.registers[0x34] = 4000 >> 8;
  }

  // Polls with writes to `reg` failing, then once they work again
  void check_failed_write_(uint8_t reg) {
    this->bus_.fail_write_register = reg;
    // The status still publishes, so the fault sensors follow the charger; the ADC readings do not
    for (int i = 1; i <= 2; i++) {
      this->hub_.update();
      EXPECT_TRUE(this->hub_.status_has_warning());
      EXPECT_EQ(this->bus_.registers[REG_CHARGER_CONTROL] & EN_CHG, 0);
      EXPECT_EQ(this->bus_.registers[REG_ADC_CONTROL] & ADC_EN, 0);
      EXPECT_EQ(status_publishes, i);
      EXPECT_FALSE(this->vbat_.has_state());
    }
    this->bus_.fail_write_register = -1;
    this->hub_.update();
    EXPECT_FALSE(this->hub_.status_has_warning());
    EXPECT_EQ(this->bus_.registers[REG_CHARGER_CONTROL], 0xC9);
    // The status was read before this poll wrote the configuration, and the ADC was restarted, so the delayed
    // poll publishes both
    EXPECT_EQ(status_publishes, 2);
    EXPECT_FALSE(this->vbat_.has_state());
    run_delayed_poll();
    EXPECT_EQ(status_publishes, 3);
    EXPECT_FLOAT_EQ(this->vbat_.state, 8.0f);
  }

  RegisterBus bus_;
  BQ25820Component hub_{&DEFAULT_CONFIG, false};
  CancelDelayedPoll cancel_poll_{this->hub_};
  sensor::Sensor vbat_;
};

TEST_F(BQ25820WriteFailureTest, FailedResetMarksSetupFailed) {
  this->bus_.fail_write_register = 0x19;  // POWER_PATH_CONTROL, REG_RST
  this->hub_.setup();
  EXPECT_TRUE(this->hub_.is_failed());
  EXPECT_EQ(this->bus_.registers[0x08], 0);  // no configuration written
  EXPECT_EQ(status_publishes, 0);
}

TEST_F(BQ25820WriteFailureTest, FailedLimitWriteKeepsChargingOff) {
  this->check_failed_write_(0x08);  // VAC_DPM
}

TEST_F(BQ25820WriteFailureTest, ConfigurationPublishesOnceTheAdcHasConverted) {
  // setup() writes the configuration with charging off at the reset values, and publishes nothing
  this->hub_.setup();
  EXPECT_NE(this->bus_.registers[REG_ADC_CONTROL] & ADC_EN, 0);
  EXPECT_EQ(this->bus_.registers[REG_CHARGER_CONTROL], 0xC9);
  EXPECT_EQ(status_publishes, 0);
  // A poll inside the conversion window, from a short interval or component.update, publishes the status only
  this->hub_.update();
  EXPECT_EQ(status_publishes, 1);
  EXPECT_FALSE(this->vbat_.has_state());
  run_delayed_poll();
  EXPECT_EQ(status_publishes, 2);
  EXPECT_FLOAT_EQ(this->vbat_.state, 8.0f);
}

TEST_F(BQ25820WriteFailureTest, SetupConfiguresEvenWhenTheStatusReadFails) {
  this->bus_.fail_read_register = REG_CHARGER_STATUS_1;
  this->hub_.setup();
  EXPECT_FALSE(this->hub_.is_failed());
  EXPECT_NE(this->bus_.registers[REG_ADC_CONTROL] & ADC_EN, 0);
  EXPECT_EQ(this->bus_.registers[0x08], 880 & 0xFF);  // VAC_DPM
  EXPECT_EQ(status_publishes, 0);
}

TEST_F(BQ25820WriteFailureTest, LandedButFailedAdcWriteIsWrittenAgain) {
  this->bus_.fail_write_register = REG_ADC_CONTROL;
  this->bus_.writes_before_failure = 1;  // clearing ADC_EN works, the last write lands but reports an error
  this->bus_.failed_write_lands = true;
  this->hub_.update();
  EXPECT_TRUE(this->hub_.status_has_warning());
  EXPECT_NE(this->bus_.registers[REG_ADC_CONTROL] & ADC_EN, 0);
  this->bus_.fail_write_register = -1;
  this->hub_.update();  // ADC_EN reads set, but the configuration is written again
  EXPECT_FALSE(this->hub_.status_has_warning());
  EXPECT_EQ(status_publishes, 1);  // from the failed write's poll
  run_delayed_poll();
  EXPECT_FLOAT_EQ(this->vbat_.state, 8.0f);
}

TEST_F(BQ25820WriteFailureTest, ChargerResetIsWrittenAgain) {
  this->hub_.update();
  this->hub_.update();
  EXPECT_EQ(status_publishes, 1);
  // A charger reset on its own clears ADC_EN and the configured limits, with the watchdog off
  this->bus_.registers[REG_ADC_CONTROL] = 0;
  this->bus_.registers[REG_CHARGER_CONTROL] = 0xC9;
  this->bus_.registers[0x08] = 0;
  this->hub_.update();
  EXPECT_NE(this->bus_.registers[REG_ADC_CONTROL] & ADC_EN, 0);
  EXPECT_NE(this->bus_.registers[0x08], 0);
  // That poll wrote the configuration, so it publishes nothing
  EXPECT_EQ(status_publishes, 1);
  this->hub_.update();
  EXPECT_EQ(status_publishes, 2);
}

TEST_F(BQ25820WriteFailureTest, FailedAdcWriteStillTurnsChargingOff) { this->check_failed_write_(REG_ADC_CONTROL); }

TEST_F(BQ25820WriteFailureTest, FailedControlWriteKeepsChargingOff) {
  this->check_failed_write_(0x1B);  // TS_THRESHOLD_CONTROL
}

TEST_F(BQ25820ConfigTest, FailedSweepIsRetriedOnce) {
  this->bus_.fail_write_register = 0x1A;  // MPPT_CONTROL
  this->bus_.writes_before_failure = 1;   // the MPPT control write, then FORCE_SWEEP fails
  this->hub_.update();
  EXPECT_TRUE(this->hub_.status_has_warning());
  EXPECT_EQ(this->reg_(REG_MPPT_CONTROL), 0x03);
  this->bus_.fail_write_register = -1;
  this->hub_.update();
  EXPECT_FALSE(this->hub_.status_has_warning());
  EXPECT_EQ(this->reg_(REG_MPPT_CONTROL), 0x83);
  // A watchdog rewrite does not sweep again
  this->reg_(REG_MPPT_CONTROL) = 0;
  this->reg_(REG_CHARGER_STATUS_1) = WD_STAT;
  this->hub_.update();
  EXPECT_EQ(this->reg_(REG_MPPT_CONTROL), 0x03);
}

TEST_F(BQ25820ConfigTest, PartialWriteLeavesAdcEnClearAndIsRetried) {
  this->bus_.fail_write_register = 0x1C;  // TS_REGION_CONTROL, the last control before charging starts
  this->hub_.update();
  EXPECT_TRUE(this->hub_.status_has_warning());
  EXPECT_EQ(this->reg_(REG_ADC_CONTROL) & ADC_EN, 0);
  EXPECT_EQ(this->reg_(REG_CHARGER_CONTROL) & EN_CHG, 0);
  this->bus_.fail_write_register = -1;
  this->hub_.update();
  EXPECT_FALSE(this->hub_.status_has_warning());
  EXPECT_EQ(this->reg_(REG_ADC_CONTROL), ADC_EN);
  EXPECT_EQ(this->reg_(REG_CHARGER_CONTROL), (0xCC | WD_RST));
}

TEST_F(BQ25820ConfigTest, FailedAdcWriteAfterWatchdogExpiryIsRetried) {
  this->hub_.update();
  this->reg_(REG_CHARGER_STATUS_1) = WD_STAT;
  this->reg_(REG_TIMER_CONTROL) = 0;
  this->bus_.fail_write_register = REG_ADC_CONTROL;
  this->bus_.writes_before_failure = 1;  // clearing ADC_EN, then the last write fails
  this->hub_.update();
  EXPECT_TRUE(this->hub_.status_has_warning());
  EXPECT_EQ(this->reg_(REG_ADC_CONTROL) & ADC_EN, 0);
  // The charger control write fed the watchdog, which clears WD_STAT on the chip
  this->reg_(REG_CHARGER_STATUS_1) = 0;
  this->reg_(REG_TIMER_CONTROL) = 0;
  this->bus_.fail_write_register = -1;
  this->hub_.update();
  EXPECT_FALSE(this->hub_.status_has_warning());
  EXPECT_EQ(this->reg_(REG_TIMER_CONTROL), 0x9C);
  EXPECT_EQ(this->reg_(REG_ADC_CONTROL), ADC_EN);
}

TEST_F(BQ25820ConfigTest, FailedWatchdogFeedDoesNotRewriteOrSweepAgain) {
  this->hub_.update();
  EXPECT_EQ(this->reg_(REG_MPPT_CONTROL), 0x83);
  this->reg_(REG_MPPT_CONTROL) = 0x03;
  this->reg_(REG_TIMER_CONTROL) = 0;
  this->reg_(REG_CHARGER_CONTROL) = 0xCC;
  this->bus_.fail_write_register = REG_CHARGER_CONTROL;
  this->hub_.update();
  EXPECT_TRUE(this->hub_.status_has_warning());
  EXPECT_EQ(this->reg_(REG_CHARGER_CONTROL), 0xCC);
  this->bus_.fail_write_register = -1;
  this->hub_.update();
  EXPECT_FALSE(this->hub_.status_has_warning());
  EXPECT_EQ(this->reg_(REG_MPPT_CONTROL), 0x03);
  EXPECT_EQ(this->reg_(REG_TIMER_CONTROL), 0);
  EXPECT_EQ(this->reg_(REG_CHARGER_CONTROL), (0xCC | WD_RST));
}

TEST_F(BQ25820ConfigTest, FailedWatchdogFeedStillPublishesTheStatus) {
  status_publishes = 0;
  this->hub_.set_status_publisher([](const BQ25820StatusRegisters &) { status_publishes++; });
  this->hub_.update();  // writes the configuration and publishes nothing
  this->bus_.fail_write_register = REG_CHARGER_CONTROL;
  this->hub_.update();
  EXPECT_TRUE(this->hub_.status_has_warning());
  EXPECT_EQ(status_publishes, 1);
}

TEST(BQ25820Adc, ScalesVoltagesAndSignedCurrentsBySenseResistor) {
  // A 2 mOhm battery shunt and a 5 mOhm input shunt: 10 uV and 4 uV ADC steps
  static constexpr BQ25820Config CONFIG{
      .limits = {0, 0, 0, 880, 1000},
      .controls = {0x10, 0x0A, 0x00, 0x00, 0x16, 0x01, 0xC9},
      .ibat_scale = 0.010f / 2.0f,
      .iac_scale = 0.004f / 5.0f,
  };
  struct Reading {
    uint16_t iac, ibat, vac, vbat, vsys;
    float iac_a, ibat_a, vac_v, vbat_v, vsys_v;
  };
  static constexpr std::array<Reading, 3> READINGS{{
      {2500, 1000, 12000, 6400, 6500, 2.0f, 5.0f, 24.0f, 12.8f, 13.0f},
      // Discharging: IBAT and IAC are two's complement; voltages above 32767 steps stay positive
      {static_cast<uint16_t>(-125), static_cast<uint16_t>(-1234), 0x9000, 0, 1, -0.1f, -6.17f, 73.728f, 0.0f, 0.002f},
      {0x7FFF, 0x8000, 0xFFFF, 1, 0, 26.2136f, -163.84f, 131.07f, 0.002f, 0.0f},
  }};
  RegisterBus bus;
  BQ25820Component hub{&CONFIG, false};
  CancelDelayedPoll cancel_poll{hub};
  sensor::Sensor iac, ibat, vac, vbat, vsys;
  attach(hub, bus);
  hub.set_iac_sensor(&iac);
  hub.set_ibat_sensor(&ibat);
  hub.set_vac_sensor(&vac);
  hub.set_vbat_sensor(&vbat);
  hub.set_vsys_sensor(&vsys);
  hub.update();
  run_delayed_poll();
  for (const Reading &reading : READINGS) {
    SCOPED_TRACE(reading.ibat);
    // IAC, IBAT, VAC, VBAT and VSYS ADCs from 0x2D, LSB first
    const std::array<uint16_t, 5> codes{reading.iac, reading.ibat, reading.vac, reading.vbat, reading.vsys};
    for (size_t i = 0; i < codes.size(); i++) {
      bus.registers[0x2D + 2 * i] = codes[i] & 0xFF;
      bus.registers[0x2E + 2 * i] = codes[i] >> 8;
    }
    hub.update();
    EXPECT_NEAR(iac.state, reading.iac_a, 1e-4f);
    EXPECT_NEAR(ibat.state, reading.ibat_a, 1e-4f);
    EXPECT_NEAR(vac.state, reading.vac_v, 1e-4f);
    EXPECT_NEAR(vbat.state, reading.vbat_v, 1e-4f);
    EXPECT_NEAR(vsys.state, reading.vsys_v, 1e-4f);
  }
}

}  // namespace esphome::bq25820::testing
