#pragma once

#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace jsn_sr04t {

enum Model {
  JSN_SR04T,
  AJ_SR04M,
};

class Jsnsr04tComponent : public sensor::Sensor, public PollingComponent, public uart::UARTDevice {
 public:
  void set_model(Model model) { this->model_ = model; }

  // ========== INTERNAL METHODS ==========
  void update() override;
  void loop() override;
  void dump_config() override;

 protected:
  void check_buffer_();
  Model model_;

  std::vector<uint8_t> buffer_;
};

}  // namespace jsn_sr04t
}  // namespace esphome
