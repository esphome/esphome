#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace cm1106 {

class CM1106Component : public PollingComponent, public uart::UARTDevice {
 public:
  float get_setup_priority() const override { return esphome::setup_priority::DATA; }

  void setup() override;
  void update() override;
  void dump_config() override;

  void calibrate_zero(uint16_t ppm);

  void set_co2_sensor(sensor::Sensor *co2_sensor) { this->co2_sensor_ = co2_sensor; }

 protected:
  sensor::Sensor *co2_sensor_{nullptr};

  bool cm1106_write_command_(const uint8_t *command, size_t command_len, uint8_t *response, size_t response_len);
};

template<typename... Ts> class CM1106CalibrateZeroAction : public Action<Ts...> {
 public:
  CM1106CalibrateZeroAction(CM1106Component *cm1106) : cm1106_(cm1106) {}

  void play(const Ts &...x) override { this->cm1106_->calibrate_zero(400); }

 protected:
  CM1106Component *cm1106_;
};

}  // namespace cm1106
}  // namespace esphome
