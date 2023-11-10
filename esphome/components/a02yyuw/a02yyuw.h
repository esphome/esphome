#pragma once

#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace a02yyuw {

class A02yyuwComponent : public sensor::Sensor, public Component, public uart::UARTDevice {
 public:
  // Nothing really public.

  // ========== INTERNAL METHODS ==========
  void loop() override;
  void dump_config() override;

 protected:
  void check_buffer_();

  std::vector<uint8_t> buffer_;
};

}  // namespace a02yyuw
}  // namespace esphome
