#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

#include <cinttypes>

namespace esphome::hc8 {

class HC8Component : public PollingComponent, public uart::UARTDevice {
 public:
  float get_setup_priority() const override;

  void setup() override;
  void update() override;
  void dump_config() override;

  void calibrate(uint16_t baseline);

  void set_co2_sensor(sensor::Sensor *co2_sensor) { co2_sensor_ = co2_sensor; }
  void set_warmup_seconds(uint32_t seconds) { warmup_seconds_ = seconds; }

 protected:
  sensor::Sensor *co2_sensor_{nullptr};
  uint32_t warmup_seconds_{0};
};

template<typename... Ts> class HC8CalibrateAction : public Action<Ts...>, public Parented<HC8Component> {
 public:
  TEMPLATABLE_VALUE(uint16_t, baseline)

  void play(const Ts &...x) override { this->parent_->calibrate(this->baseline_.value(x...)); }
};

}  // namespace esphome::hc8
