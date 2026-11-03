#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <utility>
#include <vector>

#include "esphome/components/vl53l0x/vl53l0x_sensor.h"
#include "esphome/core/application.h"

namespace esphome::vl53l0x::testing {

static constexpr uint8_t ADDRESS = 0x29;
static constexpr uint8_t SYSRANGE_START = 0x00;
static constexpr uint8_t SYSTEM_INTERRUPT_CLEAR = 0x0B;
static constexpr uint8_t RESULT_INTERRUPT_STATUS = 0x13;
static constexpr uint8_t RESULT_RANGE_MM = 0x14 + 10;

// The sensor's registers behind an address pointer that moves on after every byte. Reads of one
// register can be made to fail, and every write is recorded.
class RegisterBus final : public i2c::I2CBus {
 public:
  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                             size_t out_size) override {
    EXPECT_EQ(address, ADDRESS);
    if (size != 0 && out_size != 0 && data[0] == this->fail_reg && this->fail_count > 0) {
      this->fail_count--;
      return i2c::ERROR_NOT_ACKNOWLEDGED;
    }
    if (size != 0) {
      this->pointer_ = data[0];
      for (size_t i = 1; i < size; i++) {
        this->writes.emplace_back(this->pointer_, data[i]);
        this->registers[this->pointer_++] = data[i];
      }
    }
    for (size_t i = 0; i < out_size; i++)
      out[i] = this->registers[this->pointer_++];
    return i2c::ERROR_OK;
  }

  bool wrote(uint8_t reg, uint8_t value) const {
    for (const auto &write : this->writes) {
      if (write.first == reg && write.second == value)
        return true;
    }
    return false;
  }

  // Ranging done (start bit clear), interrupt raised and a range of 300 mm
  void finish_measurement() {
    this->registers[SYSRANGE_START] = 0x00;
    this->registers[RESULT_INTERRUPT_STATUS] = 0x07;
    this->registers[RESULT_RANGE_MM] = 0x01;
    this->registers[RESULT_RANGE_MM + 1] = 0x2C;
  }

  std::array<uint8_t, 256> registers{};
  std::vector<std::pair<uint8_t, uint8_t>> writes;
  int fail_reg{-1};
  int fail_count{0};

 protected:
  uint8_t pointer_{0};
};

class VL53L0XRecoveryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->sensor_.set_i2c_bus(&this->bus_);
    this->sensor_.set_i2c_address(ADDRESS);
  }
  // A momentary warning leaves a timeout on the shared scheduler that would outlive the sensor
  void TearDown() override { App.scheduler.cancel_timeout(&this->sensor_, InternalSchedulerID::STATUS_WARNING); }

  RegisterBus bus_;
  VL53L0XSensor sensor_;
};

TEST_F(VL53L0XRecoveryTest, FailedStatusReadDoesNotFinishMeasurement) {
  this->sensor_.update();
  // Still ranging, with a stale result left in the result registers, and the status read fails. A failed
  // read used to read as 0, "done", so the stale result was published.
  this->bus_.registers[RESULT_INTERRUPT_STATUS] = 0x07;
  this->bus_.registers[RESULT_RANGE_MM] = 0x00;
  this->bus_.registers[RESULT_RANGE_MM + 1] = 0x64;
  this->bus_.fail_reg = SYSRANGE_START;
  this->bus_.fail_count = 1;
  this->bus_.writes.clear();
  this->sensor_.loop();
  EXPECT_TRUE(this->bus_.writes.empty());
  EXPECT_FALSE(this->sensor_.has_state());

  this->bus_.finish_measurement();
  this->sensor_.loop();
  ASSERT_TRUE(this->sensor_.has_state());
  EXPECT_FLOAT_EQ(this->sensor_.state, 0.3f);
}

TEST_F(VL53L0XRecoveryTest, StalledMeasurementRestartsAtSecondUpdate) {
  this->sensor_.update();
  this->sensor_.loop();  // still ranging

  this->bus_.writes.clear();
  this->sensor_.update();
  EXPECT_TRUE(std::isnan(this->sensor_.state));
  EXPECT_TRUE(this->bus_.writes.empty()) << "a measurement still pending at one update gets another interval";

  this->sensor_.update();
  EXPECT_TRUE(this->bus_.wrote(SYSTEM_INTERRUPT_CLEAR, 0x01));
  EXPECT_TRUE(this->bus_.wrote(SYSRANGE_START, 0x01));

  this->bus_.finish_measurement();
  this->sensor_.loop();
  EXPECT_FLOAT_EQ(this->sensor_.state, 0.3f);
}

TEST_F(VL53L0XRecoveryTest, SlowMeasurementCompletesWithoutRestart) {
  this->sensor_.update();
  this->sensor_.update();  // pending: wait one more interval

  this->bus_.finish_measurement();
  this->sensor_.loop();
  EXPECT_FLOAT_EQ(this->sensor_.state, 0.3f);

  // The next update starts a fresh measurement without treating the last one as stalled
  this->bus_.writes.clear();
  this->sensor_.update();
  EXPECT_FALSE(this->bus_.wrote(SYSTEM_INTERRUPT_CLEAR, 0x01));
  EXPECT_TRUE(this->bus_.wrote(SYSRANGE_START, 0x01));
}

TEST_F(VL53L0XRecoveryTest, LongTimingBudgetGetsMoreIntervals) {
  // 300 ms of timing budget at 100 ms per update: the measurement gets four updates before a restart
  this->sensor_.set_update_interval(100);
  this->sensor_.set_timing_budget(300000);
  this->sensor_.update();
  this->bus_.writes.clear();
  for (int i = 0; i < 4; i++)
    this->sensor_.update();
  EXPECT_FALSE(this->bus_.wrote(SYSTEM_INTERRUPT_CLEAR, 0x01));

  this->bus_.finish_measurement();
  this->sensor_.loop();
  EXPECT_FLOAT_EQ(this->sensor_.state, 0.3f);

  // Still pending at the fifth update: stalled, so it restarts
  this->sensor_.update();
  this->bus_.registers[SYSRANGE_START] = 0x01;
  this->bus_.writes.clear();
  for (int i = 0; i < 5; i++)
    this->sensor_.update();
  EXPECT_TRUE(this->bus_.wrote(SYSTEM_INTERRUPT_CLEAR, 0x01));
}

TEST_F(VL53L0XRecoveryTest, FailedResultReadPublishesNanAndClearsInterrupt) {
  this->sensor_.update();
  this->bus_.finish_measurement();
  this->bus_.fail_reg = RESULT_RANGE_MM;
  this->bus_.fail_count = 1;
  this->bus_.writes.clear();
  this->sensor_.loop();
  ASSERT_TRUE(this->sensor_.has_state());
  EXPECT_TRUE(std::isnan(this->sensor_.state));
  EXPECT_TRUE(this->bus_.wrote(SYSTEM_INTERRUPT_CLEAR, 0x01));

  // Nothing is left pending, so the next update starts a measurement right away
  this->bus_.writes.clear();
  this->sensor_.update();
  EXPECT_TRUE(this->bus_.wrote(SYSRANGE_START, 0x01));
}

}  // namespace esphome::vl53l0x::testing
