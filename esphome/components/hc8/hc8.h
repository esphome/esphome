#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

#include <cinttypes>

namespace esphome::hc8 {

class HC8Component final : public PollingComponent, public uart::UARTDevice {
  SUB_SENSOR(co2)

 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void calibrate(uint16_t baseline);

  void set_warmup_seconds(uint32_t seconds) { warmup_seconds_ = seconds; }

 protected:
  uint32_t warmup_seconds_{0};
  bool warmup_complete_{false};
};

}  // namespace esphome::hc8
