#pragma once

#include <gtest/gtest.h>

#include <array>

#include "esphome/components/bq25820/bq25820.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::bq25820::testing {

static constexpr uint8_t ADDRESS = 0x6B;
static constexpr uint8_t REG_TIMER_CONTROL = 0x15;
static constexpr uint8_t REG_CHARGER_CONTROL = 0x17;
static constexpr uint8_t REG_MPPT_CONTROL = 0x1A;
static constexpr uint8_t REG_CHARGER_STATUS_1 = 0x21;
static constexpr uint8_t REG_ADC_CONTROL = 0x2B;
static constexpr uint8_t EN_CHG = 0x01;
static constexpr uint8_t WD_RST = 0x20;
static constexpr uint8_t WD_STAT = 0x08;
static constexpr uint8_t ADC_EN = 0x80;
// The packed defaults with the watchdog off
static constexpr BQ25820Config DEFAULT_CONFIG{
    .limits = {0, 0, 0, 880, 1000},
    .controls = {0x10, 0x0A, 0x00, 0x00, 0x16, 0x01, 0xC9},
    .ibat_scale = 0.002f,
    .iac_scale = 0.002f,
};

// The chip's registers behind an address pointer that moves on after every byte; reads, and writes to one
// register, can be made to fail.
class RegisterBus final : public i2c::I2CBus {
 public:
  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                             size_t out_size) override {
    EXPECT_EQ(address, ADDRESS);
    if (out_size != 0 && (this->fail_reads || (size != 0 && data[0] == this->fail_read_register)))
      return i2c::ERROR_NOT_ACKNOWLEDGED;
    const bool fail_write = size > 1 && data[0] == this->fail_write_register && this->writes_before_failure-- <= 0;
    if (fail_write && !this->failed_write_lands)
      return i2c::ERROR_NOT_ACKNOWLEDGED;
    if (size != 0) {
      this->pointer_ = data[0];
      for (size_t i = 1; i < size; i++)
        this->registers[this->next_()] = data[i];
    }
    if (fail_write)
      return i2c::ERROR_NOT_ACKNOWLEDGED;
    for (size_t i = 0; i < out_size; i++)
      out[i] = this->registers[this->next_()];
    return i2c::ERROR_OK;
  }

  std::array<uint8_t, 0x40> registers{};
  bool fail_reads{false};
  // Reads starting at this register fail
  int fail_read_register{-1};
  // Writes to this register fail once `writes_before_failure` of them have gone through
  int fail_write_register{-1};
  int writes_before_failure{0};
  // A failing write still reaches the register, as when only the acknowledgement is lost
  bool failed_write_lands{false};

 protected:
  size_t next_() { return this->pointer_++ % this->registers.size(); }

  uint8_t pointer_{0};
};

// Declared after a hub: cancels the delayed poll a configuration write schedules, so it never runs on a destroyed hub
struct CancelDelayedPoll {
  BQ25820Component &hub;
  ~CancelDelayedPoll() { App.scheduler.cancel_timeout(&this->hub, uint32_t{0}); }
};

// Runs the poll a configuration write schedules once the ADC has converted, on the real clock
inline void run_delayed_poll() {
  App.scheduler.call(millis());
  delay(1010);  // NOLINT: waits for the real one second timer
  App.scheduler.call(millis());
}

inline void attach(BQ25820Component &hub, RegisterBus &bus) {
  hub.set_i2c_bus(&bus);
  hub.set_i2c_address(ADDRESS);
}

}  // namespace esphome::bq25820::testing
