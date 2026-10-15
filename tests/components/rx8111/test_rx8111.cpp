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

}  // namespace esphome::rx8111::testing
