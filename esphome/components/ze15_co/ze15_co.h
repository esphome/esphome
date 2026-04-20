#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::ze15_co {

enum class Mode {
  QA,
  STREAM,
};

class ZE15COComponent : public sensor::Sensor, public PollingComponent, public uart::UARTDevice {
 public:
  void set_mode(Mode mode) { this->mode_ = mode; }
  void set_warmup_seconds(uint32_t seconds) { this->warmup_seconds_ = seconds; }

  void dump_config() override;
  void setup() override;
  void update() override;
  void loop() override;

 protected:
  Mode mode_{Mode::QA};
  uint32_t warmup_seconds_{30};
  bool warmup_complete_{false};

  uint8_t buffer_[9];
  uint8_t buffer_pos_{0};

  void process_stream_byte_(uint8_t byte);
  bool ze15_co_write_command_(const uint8_t *command, uint8_t *response);
};

}  // namespace esphome::ze15_co
