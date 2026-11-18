#pragma once

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

#include "esphome/components/i2c/i2c_bus.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/xgzp68xx/xgzp6899dc.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::xgzp68xx::testing {

static constexpr uint8_t ADDRESS = 0x58;

// One I2C transaction: a single register write (value set) or a single register read.
struct Transaction {
  uint8_t reg;
  bool is_write;
  uint8_t value;
  bool operator==(const Transaction &other) const = default;
};

inline Transaction read_of(uint8_t reg) { return {reg, false, 0}; }
inline Transaction write_of(uint8_t reg, uint8_t value) { return {reg, true, value}; }

// The sensor's 8-bit register file.
class RegisterBus final : public i2c::I2CBus {
 public:
  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                             size_t out_size) override {
    EXPECT_EQ(address, ADDRESS);
    if (this->failing[data[0]] || (size == 2 && this->fail_writes))
      return i2c::ERROR_NOT_ACKNOWLEDGED;
    if (size == 2) {
      this->registers[data[0]] = data[1];
      this->transactions.push_back(write_of(data[0], data[1]));
    }
    if (out_size > 0) {
      // The driver only reads one register at a time
      EXPECT_EQ(out_size, 1u);
      out[0] = this->registers[data[0]];
      this->transactions.push_back(read_of(data[0]));
    }
    return i2c::ERROR_OK;
  }

  // Pressure and temperature raw values as the chip returns them
  void set_data(int32_t pressure_raw, int16_t temperature_raw) {
    this->registers[0x04] = pressure_raw >> 16;
    this->registers[0x05] = pressure_raw >> 8;
    this->registers[0x06] = pressure_raw;
    this->registers[0x07] = static_cast<uint16_t>(temperature_raw) >> 8;
    this->registers[0x08] = temperature_raw;
  }

  std::array<uint8_t, 256> registers{};
  std::vector<Transaction> transactions;
  std::array<bool, 256> failing{};
  bool fail_writes{false};
};

struct Fixture {
  RegisterBus bus;
  sensor::Sensor pressure;
  sensor::Sensor temperature;
  // A -500 Pa to +500 Pa part
  XGZP6899DCComponent chip{1000.0f};

  Fixture() {
    static char name[] = "test_xgzp6899dc";
    static char friendly_name[] = "";
    App.pre_setup(name, sizeof(name) - 1, friendly_name, 0);
    this->chip.set_i2c_bus(&this->bus);
    this->chip.set_i2c_address(ADDRESS);
    this->chip.set_pressure_sensor(&this->pressure);
    this->chip.set_temperature_sensor(&this->temperature);
    this->bus.registers[0x00] = ADDRESS;
    // Cal_coff read from a real part: offset 4096, shift 6
    this->bus.registers[0x20] = 0x0C;
    this->bus.registers[0x21] = 0x3C;
  }

  void measure() {
    this->chip.update();
    delay(25);
    App.scheduler.call(millis());
  }
};

}  // namespace esphome::xgzp68xx::testing
