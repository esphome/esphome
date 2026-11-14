#include <gtest/gtest.h>

#include <array>
#include <cmath>

#include "esphome/components/vl6180x/vl6180x.h"

namespace esphome::vl6180x::testing {

class RegisterBus final : public i2c::I2CBus {
 public:
  i2c::ErrorCode write_readv(uint8_t, const uint8_t *data, size_t size, uint8_t *out, size_t out_size) override {
    if (size >= 2) {
      this->pointer_ = (static_cast<uint16_t>(data[0]) << 8) | data[1];
      for (size_t index = 2; index < size; index++) {
        this->registers_[this->pointer_++] = data[index];
      }
    }
    if (out_size > 0 && this->fail_reads_ > 0) {
      this->fail_reads_--;
      return i2c::ERROR_NOT_ACKNOWLEDGED;
    }
    for (size_t index = 0; index < out_size; index++) {
      out[index] = this->registers_[this->pointer_++];
    }
    return i2c::ERROR_OK;
  }

  void set8(uint16_t reg, uint8_t value) { this->registers_[reg] = value; }
  void fail_next_read() { this->fail_reads_++; }

  void set16(uint16_t reg, uint16_t value) {
    this->registers_[reg] = static_cast<uint8_t>(value >> 8);
    this->registers_[reg + 1] = static_cast<uint8_t>(value);
  }

 private:
  std::array<uint8_t, 0x213> registers_{};
  uint16_t pointer_{0};
  uint8_t fail_reads_{0};
};

class VL6180XComponentTest : public ::testing::Test {
 public:
  static void publish_lux(VL6180XComponent &component, uint16_t raw) { component.publish_lux_(raw); }
  static void begin_lux_wait(VL6180XComponent &component) {
    component.state_ = MeasurementState::MEASUREMENT_STATE_ALS_WAIT;
    component.update();
  }
  static bool range_pending(const VL6180XComponent &component) { return component.range_pending_; }
  static void finish_lux_wait(VL6180XComponent &component) { component.do_als_wait_(); }
  static MeasurementState state(const VL6180XComponent &component) { return component.state_; }
  static void initialize_all() { VL6180XComponent::run_init_sequence(); }
  static void retry(VL6180XComponent &component) {
    component.reinit_due_at_ = component.now_();
    component.do_recovery_wait_();
  }
  static bool initialized(const VL6180XComponent &component) { return component.initialized_; }
  static uint8_t retry_attempts(const VL6180XComponent &component) { return component.reinit_attempts_; }

 protected:
  void SetUp() override {
    this->component_.set_i2c_bus(&this->bus_);
    this->component_.set_i2c_address(0x29);
    this->component_.set_lux_sensor(&this->lux_sensor_);
    this->component_.set_range_sensor(&this->range_sensor_);
  }

  RegisterBus bus_;
  VL6180XComponent component_;
  sensor::Sensor lux_sensor_;
  sensor::Sensor range_sensor_;
};

TEST_F(VL6180XComponentTest, GainAndIntegrationTimeNormalizeLux) {
  this->component_.set_als_gain(6, 1.0f);
  VL6180XComponentTest::publish_lux(this->component_, 101);
  ASSERT_TRUE(this->lux_sensor_.has_state());
  EXPECT_NEAR(this->lux_sensor_.state, 32.0f, 0.01f);

  this->component_.set_als_gain(0, 20.0f);
  VL6180XComponentTest::publish_lux(this->component_, 101);
  EXPECT_NEAR(this->lux_sensor_.state, 1.6f, 0.01f);
}

TEST_F(VL6180XComponentTest, QueuesRangeUpdateBehindActiveLuxMeasurement) {
  VL6180XComponentTest::begin_lux_wait(this->component_);
  ASSERT_TRUE(VL6180XComponentTest::range_pending(this->component_));

  // ALS reports ready in IRQ status bits [5:3], and returns 101 raw counts.
  this->bus_.set16(0x050, 101);
  this->bus_.set8(0x04F, 0x20);
  VL6180XComponentTest::finish_lux_wait(this->component_);
  EXPECT_EQ(VL6180XComponentTest::state(this->component_), MeasurementState::MEASUREMENT_STATE_RANGE_START);
  EXPECT_FALSE(VL6180XComponentTest::range_pending(this->component_));
}

TEST(VL6180XRecoveryTest, SecondarySensorRetriesAfterBootFailure) {
  RegisterBus primary_bus;
  RegisterBus secondary_bus;
  VL6180XComponent primary;
  VL6180XComponent secondary;
  primary.set_i2c_bus(&primary_bus);
  primary.set_i2c_address(0x29);
  secondary.set_i2c_bus(&secondary_bus);
  secondary.set_i2c_address(0x29);
  primary.set_retry_interval(0);
  secondary.set_retry_interval(0);
  primary.set_max_retries(1);
  primary_bus.set8(0x000, 0xB4);
  secondary_bus.set8(0x000, 0xB4);
  secondary_bus.fail_next_read();
  VL6180XComponent::register_instance(&primary);
  VL6180XComponent::register_instance(&secondary);

  VL6180XComponentTest::initialize_all();
  ASSERT_TRUE(VL6180XComponentTest::initialized(primary));
  ASSERT_FALSE(VL6180XComponentTest::initialized(secondary));

  // The secondary schedules the primary initiator. A repeated failure enters backoff,
  // and a later successful attempt restores both sensors.
  secondary_bus.fail_next_read();
  VL6180XComponentTest::retry(secondary);
  VL6180XComponentTest::retry(primary);
  ASSERT_FALSE(VL6180XComponentTest::initialized(secondary));
  EXPECT_EQ(VL6180XComponentTest::retry_attempts(primary), 1);
  VL6180XComponentTest::retry(primary);
  ASSERT_TRUE(VL6180XComponentTest::initialized(primary));
  ASSERT_TRUE(VL6180XComponentTest::initialized(secondary));
  VL6180XComponentTest::retry(secondary);
  EXPECT_EQ(VL6180XComponentTest::state(secondary), MeasurementState::MEASUREMENT_STATE_IDLE);
}

}  // namespace esphome::vl6180x::testing
