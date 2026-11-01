#ifdef USE_HOST

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <utility>
#include <vector>
#include "esphome/components/ms8607/ms8607.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome::ms8607::testing {

static constexpr uint8_t PT_ADDRESS = 0x76;
static constexpr uint8_t H_ADDRESS = 0x40;
/// Marks a raw read (no command byte) in the transaction log and in fail_command
static constexpr int RAW_READ = -1;

// Datasheet example: C1..C6, then D1 and D2 give 20.00 °C and 1100.02 mbar
static constexpr std::array<uint16_t, 6> CALIBRATION{46372, 43981, 29059, 27842, 31553, 28165};
static constexpr uint32_t D1_PRESSURE = 6465444;
static constexpr uint32_t D2_TEMPERATURE = 8077636;
static constexpr uint16_t HUMIDITY_RAW = 0x7946;  // status bit 1 set

/// Datasheet CRC-4 over the PROM words, with the CRC nibble of word 0 cleared
static uint8_t prom_crc(const std::array<uint16_t, 7> &prom) {
  uint16_t remainder = 0;
  auto apply = [&remainder](uint8_t byte) {
    remainder ^= byte;
    for (int bit = 0; bit < 8; ++bit)
      remainder = (remainder & 0x8000) ? (remainder << 1) ^ 0x3000 : remainder << 1;
  };
  for (uint16_t word : prom) {
    apply(word >> 8);
    apply(word & 0xFF);
  }
  apply(0);
  apply(0);
  return remainder >> 12;
}

class MS8607Bus final : public i2c::I2CBus {
 public:
  std::array<uint16_t, 7> prom{};
  uint32_t d1{D1_PRESSURE};
  uint32_t d2{D2_TEMPERATURE};
  uint16_t humidity{HUMIDITY_RAW};
  /// (address, command) pair that answers with a NACK
  std::pair<uint8_t, int> fail{0, 0};
  std::vector<std::pair<uint8_t, int>> log;

  MS8607Bus() {
    std::copy(CALIBRATION.begin(), CALIBRATION.end(), this->prom.begin() + 1);
    this->prom[0] = 0x0123 | (prom_crc(this->prom) << 12);
  }

  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                             size_t out_size) override {
    const int command = size != 0 ? data[0] : RAW_READ;
    this->log.emplace_back(address, command);
    if (std::pair{address, command} == this->fail)
      return i2c::ERROR_NOT_ACKNOWLEDGED;
    if (address == PT_ADDRESS) {
      if (command == 0x4A || command == 0x5A) {
        this->converted_ = command == 0x4A ? this->d1 : this->d2;
      } else if (command >= 0xA0 && command <= 0xAC && out_size == 2) {
        const uint16_t word = this->prom[(command - 0xA0) / 2];
        out[0] = word >> 8;
        out[1] = word;
      } else if (command == 0x00 && out_size == 3) {
        out[0] = this->converted_ >> 16;
        out[1] = this->converted_ >> 8;
        out[2] = this->converted_;
      } else if (command != 0x1E) {
        return i2c::ERROR_NOT_ACKNOWLEDGED;
      }
      return i2c::ERROR_OK;
    }
    if (address == H_ADDRESS) {
      if (command == RAW_READ && out_size == 3) {
        out[0] = this->humidity >> 8;
        out[1] = this->humidity;
        out[2] = crc8(out, 2, 0, 0x31, true);
      } else if (command != 0xFE && command != 0xF5) {
        return i2c::ERROR_NOT_ACKNOWLEDGED;
      }
      return i2c::ERROR_OK;
    }
    return i2c::ERROR_NOT_ACKNOWLEDGED;
  }

  bool sent(uint8_t address, int command = RAW_READ, bool any_command = true) const {
    for (const auto &[logged_address, logged_command] : this->log) {
      if (logged_address == address && (any_command || logged_command == command))
        return true;
    }
    return false;
  }
  bool sent_command(uint8_t address, int command) const { return this->sent(address, command, false); }

 protected:
  uint32_t converted_{0};
};

class MS8607LifecycleTest : public ::testing::Test {
 protected:
  MS8607Bus bus_;
  MS8607Component chip_;
  MS8607HumidityDevice humidity_device_;
  sensor::Sensor temperature_;
  sensor::Sensor pressure_;
  sensor::Sensor humidity_;

  void SetUp() override {
    this->chip_.set_i2c_bus(&this->bus_);
    this->chip_.set_i2c_address(PT_ADDRESS);
    this->humidity_device_.set_i2c_bus(&this->bus_);
    this->humidity_device_.set_i2c_address(H_ADDRESS);
  }

  // Drop the chip's pending timeouts so none outlives it into a later test
  void TearDown() override {
    for (uint32_t id = 0; id < 5; ++id)
      App.scheduler.cancel_timeout(&this->chip_, id);
    App.scheduler.call(millis());
  }

  static void run_timers(uint32_t duration) {
    App.scheduler.call(millis());
    delay(duration);
    App.scheduler.call(millis());
  }

  void use_humidity() {
    this->chip_.set_humidity_sensor(&this->humidity_);
    this->chip_.set_humidity_device(&this->humidity_device_);
  }

  /// Reset, then the PROM read after its 15 ms delay
  void start() {
    this->chip_.setup();
    this->chip_.dump_config();
    run_timers(20);
    ASSERT_FALSE(this->chip_.is_failed());
  }

  /// One update; each conversion step waits 20 ms
  void cycle() {
    this->bus_.log.clear();
    this->chip_.update();
    for (int step = 0; step < 4; ++step)
      run_timers(25);
  }
};

TEST_F(MS8607LifecycleTest, TemperatureOnly) {
  this->chip_.set_temperature_sensor(&this->temperature_);
  this->start();
  this->cycle();
  ASSERT_TRUE(this->temperature_.has_state());
  EXPECT_NEAR(this->temperature_.state, 20.0f, 0.01f);
  EXPECT_FALSE(this->bus_.sent(H_ADDRESS));
  EXPECT_FALSE(this->bus_.sent_command(PT_ADDRESS, 0x4A));
  EXPECT_FALSE(this->chip_.status_has_warning());
}

TEST_F(MS8607LifecycleTest, PressureOnly) {
  this->chip_.set_pressure_sensor(&this->pressure_);
  this->start();
  this->cycle();
  EXPECT_TRUE(this->bus_.sent_command(PT_ADDRESS, 0x5A));
  ASSERT_TRUE(this->pressure_.has_state());
  EXPECT_NEAR(this->pressure_.state, 1100.02f, 0.01f);
  EXPECT_FALSE(this->temperature_.has_state());
  EXPECT_FALSE(this->bus_.sent(H_ADDRESS));
  EXPECT_FALSE(this->chip_.status_has_warning());
}

TEST_F(MS8607LifecycleTest, HumidityOnly) {
  this->use_humidity();
  this->start();
  this->cycle();
  EXPECT_TRUE(this->bus_.sent_command(PT_ADDRESS, 0x5A));
  EXPECT_FALSE(this->bus_.sent_command(PT_ADDRESS, 0x4A));
  EXPECT_TRUE(this->bus_.sent_command(H_ADDRESS, 0xF5));
  EXPECT_TRUE(this->bus_.sent_command(H_ADDRESS, RAW_READ));
  ASSERT_TRUE(this->humidity_.has_state());
  EXPECT_FLOAT_EQ(this->humidity_.state, compensated_humidity(HUMIDITY_RAW & ~0b11, 20.0f));
  EXPECT_FALSE(this->temperature_.has_state());
  EXPECT_FALSE(this->pressure_.has_state());
  EXPECT_FALSE(this->chip_.status_has_warning());
}

TEST_F(MS8607LifecycleTest, FailedPressureKeepsWarningUntilCleanCycle) {
  this->chip_.set_temperature_sensor(&this->temperature_);
  this->chip_.set_pressure_sensor(&this->pressure_);
  this->use_humidity();
  this->start();
  this->bus_.d1 = 0;  // an unfinished conversion reads as zeros
  this->cycle();
  EXPECT_FALSE(this->pressure_.has_state());
  EXPECT_TRUE(this->humidity_.has_state());
  EXPECT_TRUE(this->chip_.status_has_warning());

  this->bus_.d1 = D1_PRESSURE;
  this->cycle();
  EXPECT_TRUE(this->pressure_.has_state());
  EXPECT_FALSE(this->chip_.status_has_warning());
}

// Only the humidity address NACKs, so the reason recorded is the humidity one. The host test build has no
// logger, so the reason text itself cannot be read back here.
TEST_F(MS8607LifecycleTest, HumidityResetFailureMarksFailedAfterRetries) {
  this->use_humidity();
  this->bus_.fail = {H_ADDRESS, 0xFE};
  this->chip_.setup();
  run_timers(10);
  EXPECT_FALSE(this->chip_.is_failed());
  run_timers(30);
  EXPECT_TRUE(this->chip_.is_failed());
  EXPECT_TRUE(this->chip_.status_has_error());
  int pt_resets = 0;
  int h_resets = 0;
  for (const auto &entry : this->bus_.log) {
    pt_resets += entry == std::pair<uint8_t, int>{PT_ADDRESS, 0x1E};
    h_resets += entry == std::pair<uint8_t, int>{H_ADDRESS, 0xFE};
  }
  EXPECT_EQ(pt_resets, 3);
  EXPECT_EQ(h_resets, 3);
  EXPECT_FALSE(this->bus_.sent_command(PT_ADDRESS, 0xA0));
}

}  // namespace esphome::ms8607::testing

#endif
