#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace mhz14a {

class MHZ14AComponent : public PollingComponent, public uart::UARTDevice {
 public:
  float get_setup_priority() const override;

  void setup() override;
  void update() override;
  void dump_config() override;

  void calibrate_zero();

  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { humidity_sensor_ = humidity_sensor; }
  void set_co2_sensor(sensor::Sensor *co2_sensor) { co2_sensor_ = co2_sensor; }

 protected:
  bool mhz14a_write_command_(const uint8_t *command, uint8_t *response);

  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *co2_sensor_{nullptr};
};

template<typename... Ts> class MHZ14ACalibrateZeroAction : public Action<Ts...> {
 public:
  MHZ14ACalibrateZeroAction(MHZ14AComponent *mhz14a) : mhz14a_(mhz14a) {}

  void play(Ts... x) override { this->mhz14a_->calibrate_zero(); }

 protected:
  MHZ14AComponent *mhz14a_;
};

}  // namespace mhz14a
}  // namespace esphome
