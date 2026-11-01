#include "common.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

#include <string>
#include <vector>

namespace esphome::bq25186::testing {

enum BinaryIndex : uint8_t {
  TS,
  CHARGING,
  CHARGE_DONE,
  ILIM,
  VDPPM,
  VINDPM,
  THERMREG,
  POWER_GOOD,
  VIN_OVP,
  BATTERY_UVLO,
  SAFETY_TIMER_FLAG,
  WAKE1_FLAG,
  WAKE2_FLAG,
  TS_FLAG,
  ILIM_FLAG,
  VDPPM_FLAG,
  VINDPM_FLAG,
  THERMREG_FLAG,
  VIN_OVP_FLAG,
  BATTERY_UVLO_FLAG,
  BATTERY_OCP_FLAG,
  BINARY_COUNT,
};
enum TextIndex : uint8_t { CHARGE_STATUS, TS_STATUS, TEXT_COUNT };

// The register and bit each single-bit sensor reads, from the datasheet
struct SingleBit {
  BinaryIndex sensor;
  uint8_t reg;
  uint8_t bit;
};
static constexpr std::array<SingleBit, 19> SINGLE_BITS{{
    {TS, BQ25186_REG_STAT0, 0x80},
    {ILIM, BQ25186_REG_STAT0, 0x10},
    {VDPPM, BQ25186_REG_STAT0, 0x08},
    {VINDPM, BQ25186_REG_STAT0, 0x04},
    {THERMREG, BQ25186_REG_STAT0, 0x02},
    {POWER_GOOD, BQ25186_REG_STAT0, 0x01},
    {VIN_OVP, BQ25186_REG_STAT1, 0x80},
    {BATTERY_UVLO, BQ25186_REG_STAT1, 0x40},
    {SAFETY_TIMER_FLAG, BQ25186_REG_STAT1, 0x04},
    {WAKE1_FLAG, BQ25186_REG_STAT1, 0x02},
    {WAKE2_FLAG, BQ25186_REG_STAT1, 0x01},
    {TS_FLAG, BQ25186_REG_FLAG0, 0x80},
    {ILIM_FLAG, BQ25186_REG_FLAG0, 0x40},
    {VDPPM_FLAG, BQ25186_REG_FLAG0, 0x20},
    {VINDPM_FLAG, BQ25186_REG_FLAG0, 0x10},
    {THERMREG_FLAG, BQ25186_REG_FLAG0, 0x08},
    {VIN_OVP_FLAG, BQ25186_REG_FLAG0, 0x04},
    {BATTERY_UVLO_FLAG, BQ25186_REG_FLAG0, 0x02},
    {BATTERY_OCP_FLAG, BQ25186_REG_FLAG0, 0x01},
}};

// A copy of the function code generation emits for a hub with every status entity;
// tests/component_tests/bq25186/test_status_publisher.py checks it decodes like the generated code
struct StatusEntities {
  std::array<binary_sensor::BinarySensor, BINARY_COUNT> binary;
  std::array<text_sensor::TextSensor, TEXT_COUNT> text;
  uint8_t charge_status_last{0xFF};
  uint8_t ts_status_last{0xFF};
};
static StatusEntities *entities = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static void publish_status(const BQ25186StatusRegisters &status) {
  entities->binary[TS].publish_state((status[0] >> 7) & 0x01);
  entities->binary[CHARGING].publish_state((0b0110 >> ((status[0] >> 5) & 0x03)) & 1);
  entities->binary[CHARGE_DONE].publish_state((0b1000 >> ((status[0] >> 5) & 0x03)) & 1);
  entities->binary[ILIM].publish_state((status[0] >> 4) & 0x01);
  entities->binary[VDPPM].publish_state((status[0] >> 3) & 0x01);
  entities->binary[VINDPM].publish_state((status[0] >> 2) & 0x01);
  entities->binary[THERMREG].publish_state((status[0] >> 1) & 0x01);
  entities->binary[POWER_GOOD].publish_state((status[0] >> 0) & 0x01);
  entities->binary[VIN_OVP].publish_state((status[1] >> 7) & 0x01);
  entities->binary[BATTERY_UVLO].publish_state((status[1] >> 6) & 0x01);
  entities->binary[SAFETY_TIMER_FLAG].publish_state((status[1] >> 2) & 0x01);
  entities->binary[WAKE1_FLAG].publish_state((status[1] >> 1) & 0x01);
  entities->binary[WAKE2_FLAG].publish_state((status[1] >> 0) & 0x01);
  entities->binary[TS_FLAG].publish_state((status[2] >> 7) & 0x01);
  entities->binary[ILIM_FLAG].publish_state((status[2] >> 6) & 0x01);
  entities->binary[VDPPM_FLAG].publish_state((status[2] >> 5) & 0x01);
  entities->binary[VINDPM_FLAG].publish_state((status[2] >> 4) & 0x01);
  entities->binary[THERMREG_FLAG].publish_state((status[2] >> 3) & 0x01);
  entities->binary[VIN_OVP_FLAG].publish_state((status[2] >> 2) & 0x01);
  entities->binary[BATTERY_UVLO_FLAG].publish_state((status[2] >> 1) & 0x01);
  entities->binary[BATTERY_OCP_FLAG].publish_state((status[2] >> 0) & 0x01);
  publish_status_text(&entities->text[CHARGE_STATUS], entities->charge_status_last, status[0] >> 5, 0);
  publish_status_text(&entities->text[TS_STATUS], entities->ts_status_last, status[1] >> 3, 4);
}

class BQ25186StatusTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->hub_.set_i2c_bus(&this->bus_);
    this->hub_.set_i2c_address(ADDRESS);
    this->hub_.set_status_publisher(publish_status);
    entities = &this->entities_;
  }

  binary_sensor::BinarySensor &binary_(size_t index) { return this->entities_.binary[index]; }
  text_sensor::TextSensor &text_(size_t index) { return this->entities_.text[index]; }

  void poll_(uint8_t stat0, uint8_t stat1 = 0, uint8_t flag0 = 0) {
    this->bus_.registers[BQ25186_REG_STAT0] = stat0;
    this->bus_.registers[BQ25186_REG_STAT1] = stat1;
    this->bus_.registers[BQ25186_REG_FLAG0] = flag0;
    this->hub_.update();
  }

  RegisterBus bus_;
  BQ25186Component hub_;
  StatusEntities entities_;
};

TEST_F(BQ25186StatusTest, EachSingleBitSensorReadsOnlyItsBit) {
  for (const SingleBit &field : SINGLE_BITS) {
    std::array<uint8_t, 3> regs{};
    regs[field.reg] = field.bit;
    this->poll_(regs[0], regs[1], regs[2]);
    for (size_t i = 0; i < BINARY_COUNT; i++) {
      SCOPED_TRACE(i);
      EXPECT_TRUE(this->binary_(i).has_state());
      EXPECT_EQ(this->binary_(i).state, i == field.sensor);
    }
  }
}

TEST_F(BQ25186StatusTest, SingleBitSensorsIgnoreTheOtherBits) {
  for (const SingleBit &field : SINGLE_BITS) {
    std::array<uint8_t, 3> regs{0xFF, 0xFF, 0xFF};
    regs[field.reg] = static_cast<uint8_t>(~field.bit);
    this->poll_(regs[0], regs[1], regs[2]);
    SCOPED_TRACE(field.sensor);
    EXPECT_FALSE(this->binary_(field.sensor).state);
  }
}

TEST_F(BQ25186StatusTest, ChargingAndChargeDoneDecodeTheChargeStatus) {
  // STAT0[6:5]: not charging, constant current, constant voltage, done
  static constexpr std::array<bool, 4> CHARGING_ON{false, true, true, false};
  static constexpr std::array<bool, 4> DONE_ON{false, false, false, true};
  for (uint8_t value = 0; value < 4; value++) {
    SCOPED_TRACE(value);
    this->poll_(static_cast<uint8_t>(value << 5));
    EXPECT_EQ(this->binary_(CHARGING).state, CHARGING_ON[value]);
    EXPECT_EQ(this->binary_(CHARGE_DONE).state, DONE_ON[value]);
    // The neighbouring bits do not change the decode
    this->poll_(static_cast<uint8_t>((value << 5) | 0x9F));
    EXPECT_EQ(this->binary_(CHARGING).state, CHARGING_ON[value]);
    EXPECT_EQ(this->binary_(CHARGE_DONE).state, DONE_ON[value]);
  }
}

TEST_F(BQ25186StatusTest, ChargeStatusText) {
  static constexpr std::array<const char *, 4> NAMES{"Not Charging", "Constant Current", "Constant Voltage",
                                                     "Charge Done/Disabled"};
  for (uint8_t value = 0; value < 4; value++) {
    this->poll_(static_cast<uint8_t>((value << 5) | 0x9F), 0xFF, 0xFF);
    EXPECT_EQ(this->text_(CHARGE_STATUS).state, NAMES[value]);
  }
}

TEST_F(BQ25186StatusTest, TsStatusText) {
  static constexpr std::array<const char *, 4> NAMES{"Normal", "Hot/Cold (Charging Suspended)",
                                                     "Cool (Current Reduced)", "Warm (Voltage Reduced)"};
  for (uint8_t value = 0; value < 4; value++) {
    this->poll_(0xFF, static_cast<uint8_t>((value << 3) | 0xE7), 0xFF);
    EXPECT_EQ(this->text_(TS_STATUS).state, NAMES[value]);
  }
}

TEST_F(BQ25186StatusTest, FirstPollPublishesTextForValueZero) {
  int published = 0;
  this->text_(CHARGE_STATUS).add_on_state_callback([&published](const std::string &) { published++; });
  this->poll_(0);
  EXPECT_EQ(published, 1);
  EXPECT_EQ(this->text_(CHARGE_STATUS).state, "Not Charging");
}

TEST_F(BQ25186StatusTest, TextPublishesOnlyWhenTheFieldChanges) {
  int charge = 0;
  int ts = 0;
  this->text_(CHARGE_STATUS).add_on_state_callback([&charge](const std::string &) { charge++; });
  this->text_(TS_STATUS).add_on_state_callback([&ts](const std::string &) { ts++; });
  this->poll_(0x20, 0x08);
  EXPECT_EQ(charge, 1);
  EXPECT_EQ(ts, 1);
  // Other bits change, the fields do not
  this->poll_(0x3F, 0xE7 | 0x08, 0xFF);
  EXPECT_EQ(charge, 1);
  EXPECT_EQ(ts, 1);
  this->poll_(0x40, 0x08);
  EXPECT_EQ(charge, 2);
  EXPECT_EQ(ts, 1);
  this->poll_(0x40, 0x10);
  EXPECT_EQ(charge, 2);
  EXPECT_EQ(ts, 2);
}

TEST_F(BQ25186StatusTest, BinarySensorsPublishEveryPoll) {
  std::array<int, BINARY_COUNT> published{};
  for (size_t i = 0; i < BINARY_COUNT; i++)
    this->binary_(i).add_on_raw_state_callback([&published, i](bool) { published[i]++; });
  this->poll_(0);
  this->poll_(0);
  this->poll_(0xFF, 0xFF, 0xFF);
  for (size_t i = 0; i < BINARY_COUNT; i++) {
    SCOPED_TRACE(i);
    EXPECT_EQ(published[i], 3);
  }
}

TEST_F(BQ25186StatusTest, BinarySensorsPublishBeforeTextSensorsInFieldOrder) {
  // Binary sensor i records i, text sensor i records TEXT_BASE + i
  static constexpr size_t TEXT_BASE = 100;
  std::vector<size_t> order;
  for (size_t i = 0; i < BINARY_COUNT; i++)
    this->binary_(i).add_on_raw_state_callback([&order, i](bool) { order.push_back(i); });
  for (size_t i = 0; i < TEXT_COUNT; i++)
    this->text_(i).add_on_state_callback([&order, i](const std::string &) { order.push_back(TEXT_BASE + i); });
  this->poll_(0);
  std::vector<size_t> expected;
  expected.reserve(BINARY_COUNT + TEXT_COUNT);
  for (size_t i = 0; i < BINARY_COUNT; i++)
    expected.push_back(i);
  for (size_t i = 0; i < TEXT_COUNT; i++)
    expected.push_back(TEXT_BASE + i);
  EXPECT_EQ(order, expected);
}

TEST_F(BQ25186StatusTest, FailedReadPublishesNothing) {
  this->bus_.fail_reads = true;
  this->poll_(0xFF, 0xFF, 0xFF);
  for (size_t i = 0; i < BINARY_COUNT; i++)
    EXPECT_FALSE(this->binary_(i).has_state());
  for (size_t i = 0; i < TEXT_COUNT; i++)
    EXPECT_FALSE(this->text_(i).has_state());
}

}  // namespace esphome::bq25186::testing
