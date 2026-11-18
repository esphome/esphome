#include <gtest/gtest.h>

#include <array>
#include <ctime>

#include "esphome/components/bm8563/bm8563.h"
#include "esphome/core/time.h"

namespace esphome::bm8563::testing {

static constexpr uint8_t ADDRESS = 0x51;
static constexpr uint8_t WEEKDAYS_REG = 0x06;
static constexpr uint8_t CHIP_DAYS_PER_WEEK = 7;  // The chip counts weekdays 0-6, Sunday = 0

// The chip's 16 registers behind an address pointer that moves on after every byte.
class RegisterBus final : public i2c::I2CBus {
 public:
  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                             size_t out_size) override {
    EXPECT_EQ(address, ADDRESS);
    if (size != 0) {
      this->pointer_ = data[0];
      for (size_t i = 1; i < size; i++)
        this->registers[this->next_()] = data[i];
    }
    for (size_t i = 0; i < out_size; i++)
      out[i] = this->registers[this->next_()];
    return i2c::ERROR_OK;
  }

  std::array<uint8_t, 16> registers{};

 protected:
  size_t next_() { return this->pointer_++ % this->registers.size(); }

  uint8_t pointer_{0};
};

// write_time() takes the host's current time, so reading it back never changes the host clock.
class BM8563Test : public ::testing::Test {
 protected:
  void SetUp() override {
    this->rtc_.set_i2c_bus(&this->bus_);
    this->rtc_.set_i2c_address(ADDRESS);
    this->rtc_.add_on_time_sync_callback([this]() { ++this->syncs_; });
    this->rtc_.setup();
  }

  RegisterBus bus_;
  BM8563 rtc_;
  unsigned syncs_{0};
};

TEST_F(BM8563Test, WriteTimeStoresTheChipsWeekday) {
  // Read the clock on both sides of the write, in case UTC midnight falls in between.
  const int before = ESPTime::from_epoch_utc(::time(nullptr)).day_of_week - 1;
  this->rtc_.write_time();
  const int after = ESPTime::from_epoch_utc(::time(nullptr)).day_of_week - 1;
  const int stored = this->bus_.registers[WEEKDAYS_REG];
  EXPECT_TRUE(stored == before || stored == after) << "stored " << stored << ", expected " << before;
}

// After a write the chip keeps counting on its own and goes from Saturday (6) back to Sunday (0) at midnight.
// Seven steps from the written value visit every weekday once, and each has to read back as a valid time.
// Only the weekday moves, so the time read back is still the host's time.
TEST_F(BM8563Test, WeekdayRoundTripsThroughSundayAndSaturday) {
  this->rtc_.write_time();
  uint8_t &weekday = this->bus_.registers[WEEKDAYS_REG];
  for (uint8_t day = 0; day < CHIP_DAYS_PER_WEEK; day++) {
    weekday = (weekday + 1) % CHIP_DAYS_PER_WEEK;
    const unsigned syncs_before = this->syncs_;
    this->rtc_.read_time();
    EXPECT_EQ(this->syncs_, syncs_before + 1) << "chip weekday " << unsigned(weekday);
  }
}

}  // namespace esphome::bm8563::testing
