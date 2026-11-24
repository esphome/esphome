#pragma once

#include "esphome/core/component.h"
#include "esphome/core/frame_reader.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome::a02yyuw {

class A02yyuwComponent final : public sensor::Sensor, public Component, public uart::UARTDevice {
 public:
  // Nothing really public.

  // ========== INTERNAL METHODS ==========
  void loop() override;
  void dump_config() override;

 protected:
  void check_frame_(const uint8_t *frame);

  FrameReader<4, 0xFF> reader_;
};

}  // namespace esphome::a02yyuw
