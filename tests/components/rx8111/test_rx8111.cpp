#include <gtest/gtest.h>

#include <array>
#include <ctime>
#include <vector>

#include "esphome/components/rx8111/rx8111.h"

namespace esphome::rx8111::testing {

static constexpr uint8_t ADDRESS = 0x32;
static constexpr uint8_t SEC = 0x10;
static constexpr uint8_t WEEK = 0x13;
static constexpr uint8_t DAY = 0x14;
static constexpr uint8_t MONTH = 0x15;
static constexpr uint8_t FLAG = 0x1E;
static constexpr uint8_t CONTROL = 0x1F;
static constexpr uint8_t EXTENSION = 0x1D;
static constexpr uint8_t SEC_ALARM = 0x2C;
static constexpr uint8_t MIN_ALARM = 0x17;

class RegisterBus final : public i2c::I2CBus {
 public:
  std::array<uint8_t, 256> regs{};
  std::vector<uint8_t> writes;
  int fail_read{-1};
  int fail_write{-1};
  bool latch_event_before_flag_write{false};

  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *out, size_t out_size, uint8_t *in,
                             size_t in_size) override {
    EXPECT_EQ(address, ADDRESS);
    EXPECT_GT(out_size, 0);
    if (out_size == 0)
      return i2c::ERROR_TIMEOUT;
    const uint8_t reg = out[0];
    if (in_size > 0) {
      if (reg == this->fail_read)
        return i2c::ERROR_TIMEOUT;
      for (size_t i = 0; i < in_size; i++)
        in[i] = this->regs[reg + i];
    } else {
      if (reg == this->fail_write)
        return i2c::ERROR_TIMEOUT;
      this->writes.push_back(reg);
      if (reg == FLAG) {
        if (this->latch_event_before_flag_write)
          this->regs[FLAG] |= 0x04;  // EVF latches immediately before FLAG write.
        // RX8111 flags are write-zero-to-clear; writing one leaves the bit unchanged.
        this->regs[FLAG] &= out[1];
      } else {
        for (size_t i = 1; i < out_size; i++)
          this->regs[reg + i - 1] = out[i];
      }
    }
    return i2c::ERROR_OK;
  }
};

class RX8111Test : public ::testing::Test {
 protected:
  void SetUp() override {
    this->rtc_.set_i2c_bus(&this->bus_);
    this->rtc_.set_i2c_address(ADDRESS);
    this->rtc_.add_on_time_sync_callback([this]() { this->syncs_++; });
  }

  RegisterBus bus_;
  RX8111Component rtc_;
  unsigned syncs_{0};
};

TEST_F(RX8111Test, WritesCalendarAndPreservesEventFlagsWithoutReadModifyWrite) {
  this->bus_.regs[FLAG] = 0xB8;  // POR + existing UF/TF/AF.
  this->bus_.regs[CONTROL] = 0x5C;
  this->bus_.latch_event_before_flag_write = true;
  const auto before = ESPTime::from_epoch_utc(::time(nullptr));
  this->rtc_.write_time();
  const auto after = ESPTime::from_epoch_utc(::time(nullptr));
  EXPECT_EQ(this->bus_.regs[CONTROL], 0x5C);
  EXPECT_EQ(this->bus_.regs[FLAG], 0x3C);  // The newly latched EVF survives too.
  auto bcd = [](int value) { return (value / 10) * 16 + value % 10; };
  EXPECT_TRUE((this->bus_.regs[SEC] == bcd(before.second) && this->bus_.regs[DAY] == bcd(before.day_of_month)) ||
              (this->bus_.regs[SEC] == bcd(after.second) && this->bus_.regs[DAY] == bcd(after.day_of_month)));
  EXPECT_TRUE(this->bus_.regs[WEEK] == (1 << (before.day_of_week - 1)) ||
              this->bus_.regs[WEEK] == (1 << (after.day_of_week - 1)));
  EXPECT_EQ(this->bus_.writes, (std::vector<uint8_t>{CONTROL, SEC, CONTROL, FLAG}));
}

TEST_F(RX8111Test, RejectsInvalidFlagsBCDWeekdayAndCalendar) {
  this->bus_.regs[FLAG] = 0x83;
  this->rtc_.write_time();
  EXPECT_EQ(this->bus_.regs[FLAG], 0);
  this->bus_.regs[FLAG] = 0x83;
  this->bus_.writes.clear();
  this->rtc_.read_time();
  EXPECT_EQ(this->syncs_, 0);
  EXPECT_TRUE(this->rtc_.status_has_warning());
  EXPECT_TRUE(this->bus_.writes.empty());
  this->bus_.regs[FLAG] = 0;
  this->rtc_.read_time();
  EXPECT_EQ(this->syncs_, 1);
  EXPECT_FALSE(this->rtc_.status_has_warning());
  const uint8_t valid_weekday = this->bus_.regs[WEEK];
  const uint8_t valid_day = this->bus_.regs[DAY];
  const uint8_t valid_month = this->bus_.regs[MONTH];
  const uint8_t wrong_weekday = valid_weekday == 1 ? 2 : 1;
  for (uint8_t weekday : {uint8_t{0x00}, uint8_t{0x03}, uint8_t{0x80}, wrong_weekday}) {
    this->bus_.regs[WEEK] = weekday;
    this->rtc_.read_time();
    EXPECT_EQ(this->syncs_, 1) << "weekday " << unsigned(weekday);
    EXPECT_TRUE(this->rtc_.status_has_warning());
  }
  this->bus_.regs[WEEK] = valid_weekday;
  this->bus_.regs[DAY] = 0x32;
  this->rtc_.read_time();
  EXPECT_EQ(this->syncs_, 1);
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->bus_.regs[DAY] = 0x31;
  this->bus_.regs[MONTH] = 0x04;  // April 31 is always invalid.
  this->rtc_.read_time();
  EXPECT_EQ(this->syncs_, 1);
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->bus_.regs[DAY] = valid_day;
  this->bus_.regs[MONTH] = valid_month;
  this->bus_.regs[SEC] = 0x6A;
  this->rtc_.read_time();
  EXPECT_EQ(this->syncs_, 1);
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->bus_.regs[SEC] = 0x60;  // ESPTime permits leap seconds, but RX8111 only counts 00..59.
  this->rtc_.read_time();
  EXPECT_EQ(this->syncs_, 1);
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->bus_.regs[SEC] = 0x00;
  this->rtc_.read_time();
  EXPECT_EQ(this->syncs_, 2);
  EXPECT_FALSE(this->rtc_.status_has_warning());
}

TEST_F(RX8111Test, I2CFailuresDoNotSyncOrLeaveTheCounterStopped) {
  this->bus_.fail_read = FLAG;
  this->rtc_.read_time();
  EXPECT_EQ(this->syncs_, 0);
  EXPECT_TRUE(this->rtc_.status_has_warning());
  this->bus_.fail_read = SEC;
  this->rtc_.read_time();
  EXPECT_EQ(this->syncs_, 0);
  this->bus_.fail_read = -1;
  this->bus_.fail_write = CONTROL;
  this->rtc_.write_time();
  EXPECT_TRUE(this->bus_.writes.empty());
  this->bus_.fail_write = SEC;
  this->rtc_.write_time();
  EXPECT_EQ(this->bus_.writes, (std::vector<uint8_t>{CONTROL, CONTROL}));
  EXPECT_EQ(this->bus_.regs[CONTROL] & 1, 0);
  this->bus_.fail_write = FLAG;
  this->bus_.regs[FLAG] = 0x83;
  this->rtc_.write_time();
  EXPECT_EQ(this->bus_.regs[FLAG], 0x83);
  EXPECT_EQ(this->bus_.regs[CONTROL] & 1, 0);
}

TEST_F(RX8111Test, AlarmProgramsMaskedFieldsAndPreservesOtherControls) {
  this->bus_.regs[CONTROL] = 0x54;
  this->bus_.regs[EXTENSION] = 0xC8;
  this->rtc_.set_alarm_enabled(true);
  ASSERT_TRUE(this->rtc_.set_alarm(0, 30, 7, 2, 0xFF));
  EXPECT_EQ(this->bus_.regs[SEC_ALARM], 0x00);
  EXPECT_EQ(this->bus_.regs[MIN_ALARM], 0x30);
  EXPECT_EQ(this->bus_.regs[MIN_ALARM + 1], 0x07);
  EXPECT_EQ(this->bus_.regs[MIN_ALARM + 2], 0x02);
  EXPECT_EQ(this->bus_.regs[EXTENSION], 0xC0);
  EXPECT_EQ(this->bus_.regs[CONTROL], 0x5C);
  ASSERT_TRUE(this->rtc_.set_alarm(0xFF, 5, 0xFF, 0xFF, 31));
  EXPECT_EQ(this->bus_.regs[SEC_ALARM], 0x80);
  EXPECT_EQ(this->bus_.regs[MIN_ALARM + 2], 0x31);
  EXPECT_EQ(this->bus_.regs[EXTENSION], 0xC8);
  EXPECT_EQ(this->bus_.regs[FLAG], 0);
  EXPECT_FALSE(this->rtc_.set_alarm(60, 0, 0, 0xFF, 0xFF));
  EXPECT_FALSE(this->rtc_.set_alarm(0, 0, 0, 1, 1));
  EXPECT_EQ(this->bus_.regs[SEC_ALARM], 0x80);
}

TEST_F(RX8111Test, FlagIsLatchedUntilExplicitClearAndOnlyEdgesTrigger) {
  unsigned alarms = 0;
  this->rtc_.set_alarm_enabled(true);
  this->rtc_.add_on_alarm_callback([&alarms]() { alarms++; });
  this->bus_.regs[FLAG] = 0x08;  // Event already latched before alarm configuration.
  ASSERT_TRUE(this->rtc_.set_alarm(0, 0, 0, 0xFF, 0xFF));
  this->rtc_.update();  // A preexisting battery-backed event is not a new edge.
  EXPECT_EQ(alarms, 0);
  this->rtc_.update();
  EXPECT_EQ(alarms, 0);
  this->bus_.regs[FLAG] = 0x3C;
  this->bus_.latch_event_before_flag_write = true;
  ASSERT_TRUE(this->rtc_.clear_alarm_flag());
  EXPECT_EQ(this->bus_.regs[FLAG], 0x34);
  EXPECT_EQ(this->bus_.writes.back(), FLAG);
  this->bus_.regs[FLAG] |= 0x08;
  this->rtc_.update();
  EXPECT_EQ(alarms, 1);
  this->rtc_.update();
  EXPECT_EQ(alarms, 1);
  ASSERT_TRUE(this->rtc_.unset_alarm());
  EXPECT_EQ(this->bus_.regs[CONTROL] & 0x08, 0);
  EXPECT_EQ(this->bus_.regs[FLAG] & 0x08, 0x08);
  EXPECT_EQ(this->bus_.regs[SEC_ALARM], 0x80);
}

TEST_F(RX8111Test, AlarmI2CFailureLeavesInterruptDisabled) {
  this->rtc_.set_alarm_enabled(true);
  this->bus_.regs[CONTROL] = 0x08;
  this->bus_.fail_write = SEC_ALARM;
  EXPECT_FALSE(this->rtc_.set_alarm(0, 0, 0, 0xFF, 0xFF));
  EXPECT_EQ(this->bus_.regs[CONTROL] & 0x08, 0);
  EXPECT_TRUE(this->rtc_.status_has_warning());
}

TEST_F(RX8111Test, ClearFromAlarmAutomationAllowsNextEvent) {
  this->rtc_.set_alarm_enabled(true);
  unsigned calls = 0;
  this->rtc_.add_on_alarm_callback([this, &calls]() {
    calls++;
    EXPECT_TRUE(this->rtc_.clear_alarm_flag());
  });
  ASSERT_TRUE(this->rtc_.set_alarm(0, 0, 0, 0xFF, 0xFF));
  this->rtc_.update();  // Initialize the edge detector with AF low.
  this->bus_.regs[FLAG] = 0x08;
  this->rtc_.update();
  EXPECT_EQ(calls, 1);
  EXPECT_EQ(this->bus_.regs[FLAG] & 0x08, 0);
  this->bus_.regs[FLAG] = 0x08;
  this->rtc_.update();
  EXPECT_EQ(calls, 2);
}

TEST_F(RX8111Test, RelativeAlarmUsesUTCAndRejectsInvalidDurations) {
  EXPECT_FALSE(this->rtc_.schedule_alarm_in(1));  // Opt-in required.
  this->rtc_.set_alarm_enabled(true);
  EXPECT_FALSE(this->rtc_.schedule_alarm_in(0));
  EXPECT_FALSE(this->rtc_.schedule_alarm_in(28U * 86400U));
  const auto before = ESPTime::from_epoch_utc(::time(nullptr) + 60);
  ASSERT_TRUE(this->rtc_.schedule_alarm_in(60));
  const auto after = ESPTime::from_epoch_utc(::time(nullptr) + 60);
  const auto matches = [this](const ESPTime &time) {
    const auto bcd = [](int value) { return (value / 10) * 16 + value % 10; };
    return this->bus_.regs[SEC_ALARM] == bcd(time.second) && this->bus_.regs[MIN_ALARM] == bcd(time.minute) &&
           this->bus_.regs[MIN_ALARM + 1] == bcd(time.hour) && this->bus_.regs[MIN_ALARM + 2] == bcd(time.day_of_month);
  };
  EXPECT_TRUE(matches(before) || matches(after));
}

TEST_F(RX8111Test, TimerPrograms24BitCountAndPreservesAlarmAndOtherExtensionBits) {
  this->bus_.regs[EXTENSION] = 0xC8;
  this->bus_.regs[CONTROL] = 0x09;
  this->bus_.regs[FLAG] = 0x1C;
  this->rtc_.set_timer_enabled(true);
  this->rtc_.set_timer_count(0x123456);
  this->rtc_.set_timer_clock(1);
  this->rtc_.setup();
  EXPECT_EQ(this->bus_.regs[0x1A], 0x56);
  EXPECT_EQ(this->bus_.regs[0x1B], 0x34);
  EXPECT_EQ(this->bus_.regs[0x1C], 0x12);
  EXPECT_EQ(this->bus_.regs[EXTENSION], 0xD9);
  EXPECT_EQ(this->bus_.regs[CONTROL], 0x19);
  EXPECT_EQ(this->bus_.regs[FLAG] & 0x1C, 0x1C);
  unsigned calls = 0;
  this->rtc_.add_on_timer_callback([&calls]() { calls++; });
  this->rtc_.update();  // Retained TF is not a new event.
  EXPECT_EQ(calls, 0);
  ASSERT_TRUE(this->rtc_.clear_timer_flag());
  EXPECT_EQ(this->bus_.regs[FLAG] & 0x1C, 0x0C);
  this->bus_.regs[FLAG] |= 0x10;
  this->rtc_.update();
  this->rtc_.update();
  EXPECT_EQ(calls, 1);
  EXPECT_TRUE(this->rtc_.clear_timer_flag());
  this->bus_.regs[FLAG] |= 0x10;
  this->rtc_.update();
  EXPECT_EQ(calls, 2);
}

TEST_F(RX8111Test, TimerCallbackCanClearTFAndFailedProgrammingDoesNotStartTimer) {
  this->rtc_.set_timer_enabled(true);
  this->rtc_.set_timer_count(1);
  this->bus_.fail_write = 0x1A;
  this->rtc_.setup();
  EXPECT_EQ(this->bus_.regs[EXTENSION] & 0x10, 0);
  EXPECT_EQ(this->bus_.regs[CONTROL] & 0x10, 0);
  EXPECT_TRUE(this->rtc_.status_has_warning());

  this->bus_.fail_write = -1;
  this->bus_.regs[FLAG] = 0;
  unsigned calls = 0;
  this->rtc_.add_on_timer_callback([this, &calls]() {
    calls++;
    EXPECT_TRUE(this->rtc_.clear_timer_flag());
  });
  this->rtc_.update();
  this->bus_.regs[FLAG] = 0x10;
  this->rtc_.update();
  EXPECT_EQ(calls, 1);
  EXPECT_EQ(this->bus_.regs[FLAG], 0);
  this->bus_.regs[FLAG] = 0x10;
  this->rtc_.update();
  EXPECT_EQ(calls, 2);
}

}  // namespace esphome::rx8111::testing
