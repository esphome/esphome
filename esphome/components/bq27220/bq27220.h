#pragma once

// Datasheet: https://www.ti.com/lit/ds/symlink/bq27220.pdf
// Technical Reference Manual: https://www.ti.com/lit/ug/sluubd4a/sluubd4a.pdf

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace bq27220 {

// BQ27220 Standard Command registers (16-bit, little-endian)
// Source: BQ27220 Technical Reference Manual (SLUUBD4A)
static const uint8_t BQ27220_REG_CONTROL = 0x00;
static const uint8_t BQ27220_REG_TEMPERATURE = 0x06;  // 0.1 K units
static const uint8_t BQ27220_REG_VOLTAGE = 0x08;      // mV
static const uint8_t BQ27220_REG_BATTERY_STATUS = 0x0A;
static const uint8_t BQ27220_REG_CURRENT = 0x0C;               // mA (signed, instantaneous)
static const uint8_t BQ27220_REG_REMAINING_CAPACITY = 0x10;    // mAh
static const uint8_t BQ27220_REG_FULL_CHARGE_CAPACITY = 0x12;  // mAh
static const uint8_t BQ27220_REG_TIME_TO_EMPTY = 0x16;         // min (0xFFFF = N/A)
static const uint8_t BQ27220_REG_STATE_OF_CHARGE = 0x2C;       // %
static const uint8_t BQ27220_REG_STATE_OF_HEALTH = 0x2E;       // %

// Control subcommands
static const uint16_t BQ27220_CTRL_DEVICE_TYPE = 0x0001;

// BatteryStatus register bits
static const uint16_t BQ27220_FLAG_DSG = (1 << 0);   // Discharging
static const uint16_t BQ27220_FLAG_FC = (1 << 9);    // Full charge
static const uint16_t BQ27220_FLAG_OTC = (1 << 15);  // Over-temp during charge

class BQ27220Component final : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_voltage_sensor(sensor::Sensor *sensor) { this->voltage_sensor_ = sensor; }
  void set_current_sensor(sensor::Sensor *sensor) { this->current_sensor_ = sensor; }
  void set_battery_level_sensor(sensor::Sensor *sensor) { this->battery_level_sensor_ = sensor; }
  void set_temperature_sensor(sensor::Sensor *sensor) { this->temperature_sensor_ = sensor; }
  void set_remaining_capacity_sensor(sensor::Sensor *sensor) { this->remaining_capacity_sensor_ = sensor; }
  void set_full_charge_capacity_sensor(sensor::Sensor *sensor) { this->full_charge_capacity_sensor_ = sensor; }
  void set_time_to_empty_sensor(sensor::Sensor *sensor) { this->time_to_empty_sensor_ = sensor; }
  void set_state_of_health_sensor(sensor::Sensor *sensor) { this->state_of_health_sensor_ = sensor; }

 protected:
  bool read_word_(uint8_t reg, uint16_t &value);

  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *current_sensor_{nullptr};
  sensor::Sensor *battery_level_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *remaining_capacity_sensor_{nullptr};
  sensor::Sensor *full_charge_capacity_sensor_{nullptr};
  sensor::Sensor *time_to_empty_sensor_{nullptr};
  sensor::Sensor *state_of_health_sensor_{nullptr};
};

}  // namespace bq27220
}  // namespace esphome
