#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::hdc302x {

enum HDC302XPowerMode : uint8_t {
  HIGH_ACCURACY = 0x00,
  BALANCED = 0x0b,
  LOW_POWER = 0x16,
  ULTRA_LOW_POWER = 0xff,
};

/**
 HDC302x Temperature and humidity sensor.

 Datasheet:
 https://www.ti.com/lit/ds/symlink/hdc3020.pdf
 */
class HDC302XComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;

  void start_heater(uint16_t power, uint32_t duration_ms);
  void stop_heater();

  void set_temp_sensor(sensor::Sensor *temp_sensor) { this->temp_sensor_ = temp_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { this->humidity_sensor_ = humidity_sensor; }

  void set_power_mode(HDC302XPowerMode power_mode) { this->power_mode_ = power_mode; }

 protected:
  sensor::Sensor *temp_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};

  HDC302XPowerMode power_mode_{HDC302XPowerMode::HIGH_ACCURACY};
  bool heater_active_{false};

  bool enable_heater_();
  bool configure_heater_(uint16_t power_level);
  bool disable_heater_();
  void read_data_();
  uint32_t conversion_delay_ms_();
};

template<typename... Ts> class HeaterOnAction : public Action<Ts...>, public Parented<HDC302XComponent> {
 public:
  TEMPLATABLE_VALUE(uint16_t, power)
  TEMPLATABLE_VALUE(uint32_t, duration)

  void play(const Ts &...x) override {
    auto power_val = this->power_.value(x...);
    auto duration_val = this->duration_.value(x...);
    this->parent_->start_heater(power_val, duration_val);
  }
};

template<typename... Ts> class HeaterOffAction : public Action<Ts...>, public Parented<HDC302XComponent> {
 public:
  void play(const Ts &...x) override { this->parent_->stop_heater(); }
};

}  // namespace esphome::hdc302x
