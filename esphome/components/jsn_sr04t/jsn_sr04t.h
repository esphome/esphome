#pragma once

#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace jsn_sr04t {

class Jsnsr04tComponent : public sensor::Sensor, public PollingComponent, public uart::UARTDevice {
 public:
  // Nothing really public.

  // ========== INTERNAL METHODS ==========
  void update() override;
  void loop() override;
  void dump_config() override;

 protected:
  void check_buffer_();

  std::vector<uint8_t> buffer_;
};

}  // namespace jsn_sr04t
}  // namespace esphome
