#include "common.h"
#include "esphome/components/sensor/sensor.h"

#include <cmath>

namespace esphome::bq25820::testing {

static constexpr uint8_t REG_CHARGER_STATUS_1 = 0x21;
static constexpr uint8_t WD_STAT = 0x08;

// Every option away from its default, with the 40 s watchdog, as code generation packs packed_config.yaml
// (checked in tests/component_tests/bq25820/test_init.py)
static constexpr BQ25820Config SET_CONFIG{
    .limits = {240, 0, 256, 8000, 660},
    .controls = {0x17, 0x9C, 0xCC, 0x04, 0x03, 0xFF, 0x00, 0x80},
    .ibat_scale = 0.002f,
    .iac_scale = 0.002f,
};

class BQ25820ConfigTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->hub_.set_i2c_bus(&this->bus_);
    this->hub_.set_i2c_address(ADDRESS);
  }

  uint8_t &reg_(uint8_t address) { return this->bus_.registers[address]; }

  RegisterBus bus_;
  BQ25820Component hub_{&SET_CONFIG, true};
};

TEST_F(BQ25820ConfigTest, FirstPollWritesEachRegisterInTableOrder) {
  // Bits the configuration does not own, which must survive
  this->reg_(0x15) = 0x01;  // EN_TMR2X
  this->reg_(0x19) = 0xFB;
  this->reg_(0x1A) = 0x78;
  this->reg_(0x1C) = 0xFE;
  this->reg_(0x2B) = 0x3F;
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
  EXPECT_EQ(this->reg_(0x15), 0x9D);
  EXPECT_EQ(this->reg_(0x19), 0xFF);
  EXPECT_EQ(this->reg_(0x1A), 0xFB);  // with FORCE_SWEEP
  EXPECT_EQ(this->reg_(0x1B), 0xFF);
  EXPECT_EQ(this->reg_(0x1C), 0xFE);
  EXPECT_EQ(this->reg_(0x2B), 0xBF);
  // The charger control write, then WD_RST to feed the watchdog
  EXPECT_EQ(this->reg_(0x17), 0xCC | 0x20);
}

TEST_F(BQ25820ConfigTest, EveryPollFeedsTheWatchdog) {
  this->hub_.update();
  for (int i = 0; i < 3; i++) {
    this->reg_(0x17) = 0;
    this->hub_.update();
    EXPECT_EQ(this->reg_(0x17), 0xCC | 0x20);
  }
}

TEST_F(BQ25820ConfigTest, WatchdogExpiryRewritesTheConfigurationWithoutAnotherSweep) {
  this->hub_.update();
  this->reg_(0x15) = 0;
  this->reg_(0x1A) = 0;
  this->hub_.update();
  EXPECT_EQ(this->reg_(0x15), 0);  // not written again while WD_STAT is clear
  this->reg_(REG_CHARGER_STATUS_1) = WD_STAT;
  this->reg_(0x17) = 0;
  this->hub_.update();
  EXPECT_EQ(this->reg_(0x15), 0x9C);
  EXPECT_EQ(this->reg_(0x1A), 0x03);  // the panel sweep only runs once
  // The recovery poll feeds the watchdog too, which is what clears WD_STAT on the chip
  EXPECT_EQ(this->reg_(0x17), 0xCC | 0x20);
}

TEST(BQ25820WatchdogOff, PollsNeitherFeedNorRewrite) {
  RegisterBus bus;
  BQ25820Component hub{&DEFAULT_CONFIG, false};
  hub.set_i2c_bus(&bus);
  hub.set_i2c_address(ADDRESS);
  hub.update();
  EXPECT_EQ(bus.registers[0x17], 0xC9);
  EXPECT_EQ(bus.registers[0x15], 0x0A);
  bus.registers[0x15] = 0;
  bus.registers[REG_CHARGER_STATUS_1] = WD_STAT;
  hub.update();
  EXPECT_EQ(bus.registers[0x15], 0);
  EXPECT_EQ(bus.registers[0x17], 0xC9);
}

TEST(BQ25820ChipId, OnlyThePartNumberIsChecked) {
  // PART_INFORMATION (0x3D): PART_NUM[5:3], DEV_REV[2:0]
  for (uint8_t part_information : {0x1A, 0x18, 0x1F, 0x22, 0x00}) {
    SCOPED_TRACE(part_information);
    RegisterBus bus;
    BQ25820Component hub{&DEFAULT_CONFIG, false};
    hub.set_i2c_bus(&bus);
    hub.set_i2c_address(ADDRESS);
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
  sensor::Sensor temperature;
  hub.set_i2c_bus(&bus);
  hub.set_i2c_address(ADDRESS);
  hub.set_temp_in_c_sensor(&temperature);
  hub.set_ntc_calibration(1.0f / T0 + logf(PULL_UP / R0) / B, 1.0f / B);
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
}

}  // namespace esphome::bq25820::testing
