#include <gtest/gtest.h>

#include <array>
#include <ctime>

#include "esphome/components/ds1307/ds1307.h"
#include "esphome/core/time.h"

namespace esphome::ds1307::testing {

static constexpr uint8_t ADDRESS = 0x68;
static constexpr uint8_t SECONDS_REG = 0x00;
static constexpr uint8_t CONTROL_REG = 0x07;
static constexpr uint8_t CLOCK_HALT_BIT = 0x80;
static constexpr uint8_t CONTROL_OUT_SQWE_RS = 0x93;  // OUT, SQWE and both rate select bits set

static uint8_t to_bcd(int value) { return ((value / 10) << 4) | (value % 10); }
static int from_bcd(uint8_t value) { return (value >> 4) * 10 + (value & 0x0F); }

// The chip's 64 registers (clock, control and RAM) behind an address pointer that moves on after every byte.
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

  std::array<uint8_t, 64> registers{};

 protected:
  size_t next_() { return this->pointer_++ % this->registers.size(); }

  uint8_t pointer_{0};
};

// Uses the host's current time, so a sync never changes the host clock.
class DS1307Test : public ::testing::Test {
 protected:
  void SetUp() override {
    this->rtc_.set_i2c_bus(&this->bus_);
    this->rtc_.set_i2c_address(ADDRESS);
    this->rtc_.add_on_time_sync_callback([this]() { ++this->syncs_; });
  }

  void load_current_time_(bool halted) {
    const ESPTime now = ESPTime::from_epoch_utc(::time(nullptr));
    auto &reg = this->bus_.registers;
    reg[SECONDS_REG] = to_bcd(now.second) | (halted ? CLOCK_HALT_BIT : 0);
    reg[0x01] = to_bcd(now.minute);
    reg[0x02] = to_bcd(now.hour);
    reg[0x03] = now.day_of_week;  // 1-7, Sunday = 1
    reg[0x04] = to_bcd(now.day_of_month);
    reg[0x05] = to_bcd(now.month);
    reg[0x06] = to_bcd(now.year - 2000);
    reg[CONTROL_REG] = CONTROL_OUT_SQWE_RS;
  }

  RegisterBus bus_;
  DS1307Component rtc_;
  unsigned syncs_{0};
};

TEST_F(DS1307Test, RunningClockSyncs) {
  this->load_current_time_(false);
  this->rtc_.setup();
  this->rtc_.read_time();
  EXPECT_EQ(this->syncs_, 1u);
}

TEST_F(DS1307Test, HaltedClockDoesNotSync) {
  this->load_current_time_(true);
  this->rtc_.setup();
  this->rtc_.read_time();
  EXPECT_EQ(this->syncs_, 0u);
}

TEST_F(DS1307Test, WriteTimeStartsTheClockAndReadsBack) {
  this->load_current_time_(true);
  this->rtc_.setup();
  this->rtc_.write_time();
  EXPECT_EQ(this->bus_.registers[SECONDS_REG] & CLOCK_HALT_BIT, 0);
  this->rtc_.read_time();
  EXPECT_EQ(this->syncs_, 1u);
}

// Decodes the written registers independently of the driver, so swapped fields or digits show up.
TEST_F(DS1307Test, WriteTimeStoresTheHostTime) {
  this->load_current_time_(false);
  this->rtc_.setup();
  const time_t before = ::time(nullptr);
  this->rtc_.write_time();
  const time_t after = ::time(nullptr);
  const auto &reg = this->bus_.registers;
  ESPTime written{
      .second = static_cast<uint8_t>(from_bcd(reg[0x00] & 0x7F)),
      .minute = static_cast<uint8_t>(from_bcd(reg[0x01])),
      .hour = static_cast<uint8_t>(from_bcd(reg[0x02])),
      .day_of_month = static_cast<uint8_t>(from_bcd(reg[0x04])),
      .month = static_cast<uint8_t>(from_bcd(reg[0x05])),
      .year = static_cast<uint16_t>(from_bcd(reg[0x06]) + 2000),
  };
  written.recalc_timestamp_utc(false);
  const time_t epoch = written.timestamp;
  EXPECT_GE(epoch, before);
  EXPECT_LE(epoch, after);
  EXPECT_EQ(reg[0x03], ESPTime::from_epoch_utc(epoch).day_of_week);  // 1-7, Sunday = 1
}

TEST_F(DS1307Test, InvalidTimeDoesNotSync) {
  this->load_current_time_(false);
  this->bus_.registers[0x05] = 0x13;  // Month 13
  this->rtc_.setup();
  this->rtc_.read_time();
  EXPECT_EQ(this->syncs_, 0u);
}

TEST_F(DS1307Test, WriteTimeKeepsTheControlRegister) {
  this->load_current_time_(false);
  this->rtc_.setup();
  this->rtc_.write_time();
  EXPECT_EQ(this->bus_.registers[CONTROL_REG], CONTROL_OUT_SQWE_RS);
}

}  // namespace esphome::ds1307::testing
