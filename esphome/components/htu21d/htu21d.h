#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace htu21d {

class HTU21DComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_temperature(sensor::Sensor *temperature) { temperature_ = temperature; }
  void set_humidity(sensor::Sensor *humidity) { humidity_ = humidity; }
  void set_heater(sensor::Sensor *heater) { heater_ = heater; }

  /// Setup (reset) the sensor and check connection.
  void setup() override;
  void dump_config() override;
  /// Update the sensor values (temperature+humidity).
  void update() override;

  bool is_heater_enabled();
  void set_heater(bool status);
  void set_heater_level(uint8_t level);
  int8_t get_heater_level();

  float get_setup_priority() const override;

 protected:
  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *humidity_{nullptr};
  sensor::Sensor *heater_{nullptr};
};

template<typename... Ts> class SetHeaterLevelAction : public Action<Ts...>, public Parented<HTU21DComponent> {
 public:
  TEMPLATABLE_VALUE(uint8_t, level)

  void play(Ts... x) override {
    auto level = this->level_.value(x...);

    this->parent_->set_heater_level(level);
  }
};

template<typename... Ts> class SetHeaterAction : public Action<Ts...>, public Parented<HTU21DComponent> {
 public:
  TEMPLATABLE_VALUE(bool, status)

  void play(Ts... x) override {
    auto status = this->status_.value(x...);

    this->parent_->set_heater(status);
  }
};

}  // namespace htu21d
}  // namespace esphome
