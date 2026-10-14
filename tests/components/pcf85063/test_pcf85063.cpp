#include <gtest/gtest.h>

#include <array>
#include <ctime>

#include "esphome/components/pcf85063/pcf85063.h"
#include "esphome/core/time.h"

namespace esphome::pcf85063::testing {

static constexpr uint8_t ADDRESS = 0x51;
static constexpr uint8_t WEEKDAYS_REG = 0x08;
static constexpr uint8_t CHIP_DAYS_PER_WEEK = 7;  // The chip counts weekdays 0-6, Sunday = 0

// The chip's 18 registers behind an address pointer that moves on after every byte.
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

  std::array<uint8_t, 18> registers{};

 protected:
  size_t next_() { return this->pointer_++ % this->registers.size(); }

  uint8_t pointer_{0};
};

// write_time() takes the host's current time, so reading it back never changes the host clock.
class PCF85063Test : public ::testing::Test {
 protected:
  void SetUp() override {
    this->rtc_.set_i2c_bus(&this->bus_);
    this->rtc_.set_i2c_address(ADDRESS);
    this->rtc_.add_on_time_sync_callback([this]() { ++this->syncs_; });
    this->rtc_.setup();
  }

  RegisterBus bus_;
  PCF85063Component rtc_;
  unsigned syncs_{0};
};

TEST_F(PCF85063Test, WriteTimeStoresTheChipsWeekday) {
  const ESPTime now = ESPTime::from_epoch_utc(::time(nullptr));
  this->rtc_.write_time();
  EXPECT_EQ(this->bus_.registers[WEEKDAYS_REG], now.day_of_week - 1);
}

// After a write the chip keeps counting on its own and goes from Saturday (6) back to Sunday (0) at midnight.
// Seven steps from the written value visit every weekday once, and each has to read back as a valid time.
// Only the weekday moves, so the time read back is still the host's time.
TEST_F(PCF85063Test, WeekdayRoundTripsThroughSundayAndSaturday) {
  this->rtc_.write_time();
  uint8_t &weekday = this->bus_.registers[WEEKDAYS_REG];
  for (uint8_t day = 0; day < CHIP_DAYS_PER_WEEK; day++) {
    weekday = (weekday + 1) % CHIP_DAYS_PER_WEEK;
    const unsigned syncs_before = this->syncs_;
    this->rtc_.read_time();
    EXPECT_EQ(this->syncs_, syncs_before + 1) << "chip weekday " << unsigned(weekday);
  }
}

}  // namespace esphome::pcf85063::testing
