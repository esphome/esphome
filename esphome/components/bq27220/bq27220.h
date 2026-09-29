#pragma once

// Datasheet: https://www.ti.com/lit/ds/symlink/bq27220.pdf
// Technical Reference Manual: https://www.ti.com/lit/ug/sluubd4a/sluubd4a.pdf

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/i2c/i2c.h"
#include <cmath>

namespace esphome::bq27220 {

// BQ27220 Standard Command registers (16-bit, little-endian)
// Source: BQ27220 Technical Reference Manual (SLUUBD4A)
static const uint8_t BQ27220_REG_CONTROL = 0x00;
static const uint8_t BQ27220_REG_TEMPERATURE = 0x06;           // 0.1 K units
static const uint8_t BQ27220_REG_VOLTAGE = 0x08;               // mV
static const uint8_t BQ27220_REG_CURRENT = 0x0C;               // mA (signed, instantaneous)
static const uint8_t BQ27220_REG_REMAINING_CAPACITY = 0x10;    // mAh
static const uint8_t BQ27220_REG_FULL_CHARGE_CAPACITY = 0x12;  // mAh
static const uint8_t BQ27220_REG_TIME_TO_EMPTY = 0x16;         // min (0xFFFF = N/A)
static const uint8_t BQ27220_REG_STATE_OF_CHARGE = 0x2C;       // %
static const uint8_t BQ27220_REG_STATE_OF_HEALTH = 0x2E;       // %

// DEVICE_NUMBER subcommand (0x0001, SLUUBD4A §2.2.2); its result is read from the
// MAC data buffer.
static const uint8_t BQ27220_REG_MAC_DATA = 0x40;
static const uint16_t BQ27220_DEVICE_NUMBER = 0x0220;

// One decoded reading per poll. A field is NAN when its register could not be read
// or the gauge reported "not applicable" (e.g. TimeToEmpty = 0xFFFF).
struct BQ27220Data {
  float voltage{NAN};
  float current{NAN};
  float battery_level{NAN};
  float temperature{NAN};
  float remaining_capacity{NAN};
  float full_charge_capacity{NAN};
  float time_to_empty{NAN};
  float state_of_health{NAN};
};

class BQ27220Component final : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  // Each configured sensor registers a listener that reads its value from the
  // decoded BQ27220Data and publishes it. Templated so lambdas and lightweight
  // forwarders are accepted without forcing std::function allocation.
  template<typename F> void add_on_data_callback(F &&callback) { this->data_callback_.add(std::forward<F>(callback)); }

 protected:
  bool read_word_(uint8_t reg, uint16_t &value);

  CallbackManager<void(const BQ27220Data &)> data_callback_{};
};

}  // namespace esphome::bq27220
