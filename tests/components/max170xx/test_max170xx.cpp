#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

#include "esphome/components/max170xx/max170xx.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::max170xx::testing {

static constexpr uint8_t ADDRESS = 0x36;

using Write = std::pair<uint8_t, uint16_t>;

// The gauge's 16-bit registers, big-endian on the wire.
class RegisterBus final : public i2c::I2CBus {
 public:
  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                             size_t out_size) override {
    EXPECT_EQ(address, ADDRESS);
    if (this->silent || (this->fail_writes && size > 1))
      return i2c::ERROR_NOT_ACKNOWLEDGED;
    if (size == 3) {
      const uint16_t value = (data[1] << 8) | data[2];
      this->registers[data[0]] = value;
      this->writes.emplace_back(data[0], value);
    }
    if (out_size == 2) {
      const uint16_t value = this->registers[data[0]];
      out[0] = value >> 8;
      out[1] = value;
    }
    return i2c::ERROR_OK;
  }

  bool has_write(const Write &write) const {
    return std::find(this->writes.begin(), this->writes.end(), write) != this->writes.end();
  }

  std::map<uint8_t, uint16_t> registers;
  std::vector<Write> writes;
  bool silent{false};
  bool fail_writes{false};
};

class Gauge43 final : public MAX170xxComponent<false> {
 public:
  void update() override {}
  using MAX170xxComponent::clear_wake_alert_;
  using MAX170xxComponent::configure_;
  using MAX170xxComponent::configured_;
  using MAX170xxComponent::ready_for_update_;
  bool take_pending_clear() { return this->cancel_timeout(WAKE_ALERT_CLEAR_TIMEOUT_ID); }
};

class Gauge48 final : public MAX170xxComponent<true> {
 public:
  void update() override {}
  using MAX170xxComponent::clear_wake_alert_;
  using MAX170xxComponent::configure_;
  using MAX170xxComponent::configured_;
  using MAX170xxComponent::ready_for_update_;
  bool take_pending_clear() { return this->cancel_timeout(WAKE_ALERT_CLEAR_TIMEOUT_ID); }
};

template<typename G> struct Fixture {
  RegisterBus bus;
  G gauge;

  Fixture() {
    static char name[] = "test_max170xx";
    static char friendly_name[] = "";
    App.pre_setup(name, sizeof(name) - 1, friendly_name, 0);
    this->gauge.set_i2c_bus(&this->bus);
    this->gauge.set_i2c_address(ADDRESS);
  }

  ~Fixture() {
    // Drop a pending clear while the gauge is alive; App outlives each test
    this->gauge.take_pending_clear();
    App.scheduler.call(millis());
  }
};

TEST(MAX170xxTest, SilentBusAtSetupIsRetried) {
  Fixture<Gauge48> f;
  f.bus.silent = true;
  f.gauge.setup();
  EXPECT_FALSE(f.gauge.is_failed());
  EXPECT_FALSE(f.gauge.configured_);
  EXPECT_TRUE(f.gauge.status_has_warning());
  EXPECT_FALSE(f.gauge.ready_for_update_());
  EXPECT_TRUE(f.gauge.status_has_warning());

  f.bus.silent = false;
  f.bus.registers[REG_CONFIG] = 0x971C;
  EXPECT_TRUE(f.gauge.ready_for_update_());
  EXPECT_TRUE(f.bus.has_write({0x0C, 0x971C}));
}

TEST(MAX170xxTest, WrongConfigFails) {
  Fixture<Gauge48> f;
  f.bus.registers[REG_CONFIG] = 0x1234;
  f.gauge.setup();
  EXPECT_TRUE(f.gauge.is_failed());
  EXPECT_TRUE(f.bus.writes.empty());
}

TEST(MAX170xxTest, WriteBackFailureIsRetried) {
  Fixture<Gauge48> f;
  f.bus.registers[REG_CONFIG] = 0x971C;
  f.bus.fail_writes = true;
  f.gauge.setup();
  EXPECT_FALSE(f.gauge.is_failed());
  EXPECT_FALSE(f.gauge.configured_);

  f.bus.fail_writes = false;
  EXPECT_TRUE(f.gauge.ready_for_update_());
}

TEST(MAX170xxTest, WakeFromSleepSchedulesClear) {
  Fixture<Gauge48> f;
  f.bus.registers[REG_CONFIG] = 0x979C;
  f.gauge.setup();
  EXPECT_TRUE(f.gauge.configured_);
  EXPECT_TRUE(f.bus.has_write({0x0C, 0x971C}));
  EXPECT_TRUE(f.gauge.take_pending_clear());
}

TEST(MAX170xxTest, NormalBootSchedulesNoClear) {
  Fixture<Gauge48> f;
  f.bus.registers[REG_CONFIG] = 0x971C;
  f.bus.registers[REG_STATUS] = 0x1100;
  f.gauge.setup();
  EXPECT_FALSE(f.gauge.take_pending_clear());
  // A real low-battery alert at an ordinary boot is left alone
  EXPECT_EQ(f.bus.registers[REG_STATUS], 0x1100);
  EXPECT_EQ(f.bus.writes, (std::vector<Write>{{0x0C, 0x971C}}));
}

TEST(MAX170xxTest, ClearWakeAlertKeepsSleepAndOtherStatusBits) {
  Fixture<Gauge48> f;
  f.bus.registers[REG_CONFIG] = 0x97BC;
  f.bus.registers[REG_STATUS] = 0x5F00;
  f.gauge.clear_wake_alert_();
  EXPECT_EQ(f.bus.registers[REG_CONFIG], 0x979C);
  EXPECT_EQ(f.bus.registers[REG_STATUS], 0x4F00);
}

TEST(MAX170xxTest, ClearWakeAlertOn43LeavesStatusAlone) {
  Fixture<Gauge43> f;
  f.bus.registers[REG_CONFIG] = 0x97BC;
  f.gauge.clear_wake_alert_();
  EXPECT_EQ(f.bus.registers[REG_CONFIG], 0x979C);
  for (const Write &write : f.bus.writes)
    EXPECT_NE(write.first, 0x1A);
}

TEST(MAX170xxTest, SleepModeOn48WritesEnSleepThenSleep) {
  Fixture<Gauge48> f;
  f.bus.registers[REG_CONFIG] = 0x971C;
  f.gauge.setup();
  f.bus.writes.clear();
  f.gauge.sleep_mode();
  EXPECT_EQ(f.bus.writes, (std::vector<Write>{{0x06, 0x2000}, {0x0C, 0x979C}}));
}

TEST(MAX170xxTest, SleepModeOn43WritesSleepOnly) {
  Fixture<Gauge43> f;
  f.bus.registers[REG_CONFIG] = 0x971C;
  f.gauge.setup();
  f.bus.writes.clear();
  f.gauge.sleep_mode();
  EXPECT_EQ(f.bus.writes, (std::vector<Write>{{0x0C, 0x979C}}));
}

TEST(MAX170xxTest, SleepModeRunsPendingClearFirst) {
  Fixture<Gauge48> f;
  f.bus.registers[REG_CONFIG] = 0x979C;
  f.gauge.setup();
  // The gauge raises the alert as it wakes
  f.bus.registers[REG_CONFIG] = 0x973C;
  f.bus.registers[REG_STATUS] = 0x1100;
  f.gauge.sleep_mode();
  EXPECT_FALSE(f.gauge.take_pending_clear());
  EXPECT_EQ(f.bus.registers[REG_STATUS], 0x0100);
  ASSERT_FALSE(f.bus.writes.empty());
  EXPECT_EQ(f.bus.writes.back(), (Write{0x0C, 0x979C}));
}

TEST(MAX170xxTest, SleepModeOnSilentBusWritesNothing) {
  Fixture<Gauge48> f;
  f.bus.silent = true;
  f.gauge.setup();
  f.gauge.sleep_mode();
  EXPECT_TRUE(f.bus.writes.empty());
  EXPECT_TRUE(f.gauge.status_has_warning());
  EXPECT_FALSE(f.gauge.is_failed());
}

TEST(MAX170xxTest, SetupAfterSleepModeKeepsGaugeAsleep) {
  Fixture<Gauge48> f;
  f.bus.registers[REG_CONFIG] = 0x971C;
  // An on_boot with priority above 600 runs before setup()
  f.gauge.sleep_mode();
  f.bus.writes.clear();
  f.gauge.setup();
  EXPECT_TRUE(f.bus.writes.empty());
  EXPECT_EQ(f.bus.registers[REG_CONFIG], 0x979C);
}

TEST(MAX170xxTest, SleepModeWhenFailedWritesNothing) {
  Fixture<Gauge48> f;
  f.bus.registers[REG_CONFIG] = 0x1234;
  f.gauge.setup();
  f.bus.writes.clear();
  f.gauge.sleep_mode();
  EXPECT_TRUE(f.bus.writes.empty());
}

}  // namespace esphome::max170xx::testing
