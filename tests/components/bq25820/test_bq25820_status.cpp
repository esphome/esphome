#include "common.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include <string>

namespace esphome::bq25820::testing {

enum BinaryIndex : uint8_t {
  CHARGING,
  CHARGED,
  LOW_BATTERY,
  PG,
  REVERSE,
  VAC_UNDERVOLTAGE,
  VAC_OVERVOLTAGE,
  IBAT_OVERCURRENT,
  VBAT_OVERVOLTAGE,
  THERMAL_SHUTDOWN,
  CHARGE_SAFETY_TIMER,
  DRV_SUPPLY_INVALID,
  BINARY_COUNT,
};
enum TextIndex : uint8_t { CHARGE_STATUS, TEMP_STATUS, TEXT_COUNT };

// The status register (offset from CHARGER_STATUS_1) and bit each single-bit sensor reads, from the datasheet
struct SingleBit {
  BinaryIndex sensor;
  uint8_t reg;
  uint8_t bit;
};
static constexpr std::array<SingleBit, 9> SINGLE_BITS{{
    {PG, 1, 0x80},
    {REVERSE, 2, 0x04},
    {VAC_UNDERVOLTAGE, 3, 0x80},
    {VAC_OVERVOLTAGE, 3, 0x40},
    {IBAT_OVERCURRENT, 3, 0x20},
    {VBAT_OVERVOLTAGE, 3, 0x10},
    {THERMAL_SHUTDOWN, 3, 0x08},
    {CHARGE_SAFETY_TIMER, 3, 0x04},
    {DRV_SUPPLY_INVALID, 3, 0x02},
}};

// Copy of the generated function for a hub with every status entity;
// tests/component_tests/bq25820/test_hub_function.py checks it against the generated code
struct StatusEntities {
  std::array<binary_sensor::BinarySensor, BINARY_COUNT> binary;
  std::array<text_sensor::TextSensor, TEXT_COUNT> text;
  uint8_t charge_status_last{0xFF};
  uint8_t temp_status_last{0xFF};
};
static StatusEntities *entities = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static void publish_status(const BQ25820StatusRegisters &status) {
  entities->binary[CHARGING].publish_state(((0b1111110 >> ((status[0] >> 0) & 0x07)) & 1));
  entities->binary[CHARGED].publish_state(((0b10000000 >> ((status[0] >> 0) & 0x07)) & 1));
  entities->binary[LOW_BATTERY].publish_state(((0b0110 >> ((status[0] >> 0) & 0x07)) & 1));
  entities->binary[PG].publish_state(((status[1] >> 7) & 0x01));
  entities->binary[REVERSE].publish_state(((status[2] >> 2) & 0x01));
  entities->binary[VAC_UNDERVOLTAGE].publish_state(((status[3] >> 7) & 0x01));
  entities->binary[VAC_OVERVOLTAGE].publish_state(((status[3] >> 6) & 0x01));
  entities->binary[IBAT_OVERCURRENT].publish_state(((status[3] >> 5) & 0x01));
  entities->binary[VBAT_OVERVOLTAGE].publish_state(((status[3] >> 4) & 0x01));
  entities->binary[THERMAL_SHUTDOWN].publish_state(((status[3] >> 3) & 0x01));
  entities->binary[CHARGE_SAFETY_TIMER].publish_state(((status[3] >> 2) & 0x01));
  entities->binary[DRV_SUPPLY_INVALID].publish_state(((status[3] >> 1) & 0x01));
  publish_status_text(&entities->text[CHARGE_STATUS], entities->charge_status_last, (status[0] >> 0), 0);
  publish_status_text(&entities->text[TEMP_STATUS], entities->temp_status_last, (status[1] >> 4), 8);
}

class BQ25820StatusTest : public ::testing::Test {
 protected:
  void SetUp() override {
    attach(this->hub_, this->bus_);
    this->hub_.set_status_publisher(publish_status);
    entities = &this->entities_;
    // The first poll writes the configuration and publishes nothing
    this->hub_.update();
  }

  binary_sensor::BinarySensor &binary_(size_t index) { return this->entities_.binary[index]; }
  text_sensor::TextSensor &text_(size_t index) { return this->entities_.text[index]; }

  void poll_(uint8_t status1, uint8_t status2 = 0, uint8_t status3 = 0, uint8_t fault = 0) {
    this->bus_.registers[REG_CHARGER_STATUS_1] = status1;
    this->bus_.registers[REG_CHARGER_STATUS_1 + 1] = status2;
    this->bus_.registers[REG_CHARGER_STATUS_1 + 2] = status3;
    this->bus_.registers[REG_CHARGER_STATUS_1 + 3] = fault;
    this->hub_.update();
  }

  RegisterBus bus_;
  BQ25820Component hub_{&DEFAULT_CONFIG, false};
  CancelDelayedPoll cancel_poll_{this->hub_};
  StatusEntities entities_;
};

TEST_F(BQ25820StatusTest, EachSingleBitSensorReadsOnlyItsBit) {
  for (const SingleBit &field : SINGLE_BITS) {
    std::array<uint8_t, 4> regs{};
    regs[field.reg] = field.bit;
    this->poll_(regs[0], regs[1], regs[2], regs[3]);
    for (size_t i = PG; i < BINARY_COUNT; i++) {
      SCOPED_TRACE(i);
      EXPECT_TRUE(this->binary_(i).has_state());
      EXPECT_EQ(this->binary_(i).state, i == field.sensor);
    }
  }
}

TEST_F(BQ25820StatusTest, SingleBitSensorsIgnoreTheOtherBits) {
  for (const SingleBit &field : SINGLE_BITS) {
    std::array<uint8_t, 4> regs{0xFF, 0xFF, 0xFF, 0xFF};
    regs[field.reg] = static_cast<uint8_t>(~field.bit);
    this->poll_(regs[0], regs[1], regs[2], regs[3]);
    SCOPED_TRACE(field.sensor);
    EXPECT_FALSE(this->binary_(field.sensor).state);
  }
}

TEST_F(BQ25820StatusTest, ChargeStatSensorsDecodeTheChargeStatus) {
  // CHARGE_STAT: not charging, trickle, precharge, fast, taper, reserved, top-off, done
  static constexpr std::array<bool, 8> CHARGING_ON{false, true, true, true, true, true, true, false};
  static constexpr std::array<bool, 8> CHARGED_ON{false, false, false, false, false, false, false, true};
  static constexpr std::array<bool, 8> LOW_BATTERY_ON{false, true, true, false, false, false, false, false};
  for (uint8_t value = 0; value < 8; value++) {
    SCOPED_TRACE(value);
    // WD_STAT (bit 3) is set by the second poll; the watchdog is off, so it is only another bit
    for (uint8_t other : {0x00, 0xF8}) {
      this->poll_(static_cast<uint8_t>(value | other));
      EXPECT_EQ(this->binary_(CHARGING).state, CHARGING_ON[value]);
      EXPECT_EQ(this->binary_(CHARGED).state, CHARGED_ON[value]);
      EXPECT_EQ(this->binary_(LOW_BATTERY).state, LOW_BATTERY_ON[value]);
    }
  }
}

TEST_F(BQ25820StatusTest, ChargeStatusText) {
  static constexpr std::array<const char *, 8> NAMES{"Not Charging", "Trickle", "Precharge", "Fast Charge",
                                                     "Taper",        "Unknown", "Top-Off",   "Done"};
  for (uint8_t value = 0; value < 8; value++) {
    this->poll_(static_cast<uint8_t>(value | 0xF8), 0xFF, 0xFF, 0xFF);
    EXPECT_EQ(this->text_(CHARGE_STATUS).state, NAMES[value]);
  }
}

TEST_F(BQ25820StatusTest, TempStatusText) {
  static constexpr std::array<const char *, 8> NAMES{"Normal", "Warm",    "Cool",    "Cold",
                                                     "Hot",    "Unknown", "Unknown", "Unknown"};
  for (uint8_t value = 0; value < 8; value++) {
    this->poll_(0xFF, static_cast<uint8_t>((value << 4) | 0x8F), 0xFF, 0xFF);
    EXPECT_EQ(this->text_(TEMP_STATUS).state, NAMES[value]);
  }
}

TEST_F(BQ25820StatusTest, FirstPollPublishesTextForValueZero) {
  int published = 0;
  this->text_(CHARGE_STATUS).add_on_state_callback([&published](const std::string &) { published++; });
  this->poll_(0);
  EXPECT_EQ(published, 1);
  EXPECT_EQ(this->text_(CHARGE_STATUS).state, "Not Charging");
}

TEST_F(BQ25820StatusTest, TextPublishesOnlyWhenTheFieldChanges) {
  int charge = 0;
  int temp = 0;
  this->text_(CHARGE_STATUS).add_on_state_callback([&charge](const std::string &) { charge++; });
  this->text_(TEMP_STATUS).add_on_state_callback([&temp](const std::string &) { temp++; });
  this->poll_(0x03, 0x10);
  EXPECT_EQ(charge, 1);
  EXPECT_EQ(temp, 1);
  // Other bits change, the fields do not
  this->poll_(0xF3, 0x8F | 0x10, 0xFF, 0xFF);
  EXPECT_EQ(charge, 1);
  EXPECT_EQ(temp, 1);
  this->poll_(0x04, 0x10);
  EXPECT_EQ(charge, 2);
  EXPECT_EQ(temp, 1);
  this->poll_(0x04, 0x20);
  EXPECT_EQ(charge, 2);
  EXPECT_EQ(temp, 2);
}

TEST_F(BQ25820StatusTest, BinarySensorsPublishEveryPoll) {
  std::array<int, BINARY_COUNT> published{};
  for (size_t i = 0; i < BINARY_COUNT; i++)
    this->binary_(i).add_on_raw_state_callback([&published, i](bool) { published[i]++; });
  this->poll_(0);
  this->poll_(0);
  this->poll_(0xFF, 0xFF, 0xFF, 0xFF);
  for (size_t i = 0; i < BINARY_COUNT; i++) {
    SCOPED_TRACE(i);
    EXPECT_EQ(published[i], 3);
  }
}

TEST_F(BQ25820StatusTest, FailedReadPublishesNothing) {
  this->bus_.fail_reads = true;
  this->poll_(0xFF, 0xFF, 0xFF, 0xFF);
  for (size_t i = 0; i < BINARY_COUNT; i++)
    EXPECT_FALSE(this->binary_(i).has_state());
  for (size_t i = 0; i < TEXT_COUNT; i++)
    EXPECT_FALSE(this->text_(i).has_state());
}

}  // namespace esphome::bq25820::testing
