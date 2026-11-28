#include <gtest/gtest.h>

#include <array>
#include <ctime>
#include <string>

#include "esphome/components/logger/logger.h"
#include "esphome/components/pcf8563/pcf8563.h"
#include "esphome/core/time.h"

namespace esphome::pcf8563::testing {

static constexpr uint8_t ADDRESS = 0x51;
static constexpr uint8_t VL_SECONDS_REG = 0x02;
static constexpr uint8_t VOLTAGE_LOW_BIT = 0x80;
static constexpr uint8_t WEEKDAYS_REG = 0x06;
static constexpr uint8_t CHIP_DAYS_PER_WEEK = 7;  // The chip counts weekdays 0-6, Sunday = 0
static constexpr const char *VOLTAGE_LOW_WARNING = "Voltage-low flag set";

static uint8_t to_bcd(int value) { return ((value / 10) << 4) | (value % 10); }

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

class PCF8563Test : public ::testing::Test {
 protected:
  void SetUp() override {
    this->rtc_.set_i2c_bus(&this->bus_);
    this->rtc_.set_i2c_address(ADDRESS);
    this->rtc_.add_on_time_sync_callback([this]() { ++this->syncs_; });
  }

  // Uses the host's current time, so a sync never changes the host clock.
  void load_current_time_(bool voltage_low) {
    const ESPTime now = ESPTime::from_epoch_utc(::time(nullptr));
    auto &reg = this->bus_.registers;
    reg[VL_SECONDS_REG] = to_bcd(now.second) | (voltage_low ? VOLTAGE_LOW_BIT : 0);
    reg[0x03] = to_bcd(now.minute);
    reg[0x04] = to_bcd(now.hour);
    reg[0x05] = to_bcd(now.day_of_month);
    reg[WEEKDAYS_REG] = now.day_of_week - 1;
    reg[0x07] = to_bcd(now.month);
    reg[0x08] = to_bcd(now.year - 2000);
  }

  // Returns what read_time() logged. The test logger only prints while it has a baud rate.
  std::string read_time_() {
    const uint32_t baud_rate = logger::global_logger->get_baud_rate();
    logger::global_logger->set_baud_rate(115200);
    ::testing::internal::CaptureStdout();
    this->rtc_.read_time();
    std::string log = ::testing::internal::GetCapturedStdout();
    logger::global_logger->set_baud_rate(baud_rate);
    return log;
  }

  RegisterBus bus_;
  PCF8563Component rtc_;
  unsigned syncs_{0};
};

TEST_F(PCF8563Test, VoltageLowFlagBlocksSyncAndWarns) {
  this->load_current_time_(true);
  this->rtc_.setup();
  const std::string log = this->read_time_();
  EXPECT_EQ(this->syncs_, 0u);
  EXPECT_NE(log.find(VOLTAGE_LOW_WARNING), std::string::npos) << log;
}

TEST_F(PCF8563Test, ClearVoltageLowFlagSyncs) {
  this->load_current_time_(false);
  this->rtc_.setup();
  const std::string log = this->read_time_();
  EXPECT_EQ(this->syncs_, 1u);
  EXPECT_EQ(log.find(VOLTAGE_LOW_WARNING), std::string::npos) << log;
}

TEST_F(PCF8563Test, WriteTimeClearsVoltageLowFlag) {
  this->load_current_time_(true);
  this->rtc_.setup();
  this->rtc_.write_time();
  EXPECT_EQ(this->bus_.registers[VL_SECONDS_REG] & VOLTAGE_LOW_BIT, 0);
  this->read_time_();
  EXPECT_EQ(this->syncs_, 1u);
}

// Seven steps from the written value visit every weekday once, and each has to read back as a valid time.
// Only the weekday moves, so the time read back is still the host's time.
TEST_F(PCF8563Test, WeekdayRoundTripsThroughSundayAndSaturday) {
  this->rtc_.setup();
  this->rtc_.write_time();
  uint8_t &weekday = this->bus_.registers[WEEKDAYS_REG];
  for (uint8_t step = 0; step < CHIP_DAYS_PER_WEEK; step++) {
    weekday = (weekday + 1) % CHIP_DAYS_PER_WEEK;
    const unsigned syncs_before = this->syncs_;
    this->read_time_();
    EXPECT_EQ(this->syncs_, syncs_before + 1) << "chip weekday " << unsigned(weekday);
  }
}

}  // namespace esphome::pcf8563::testing
