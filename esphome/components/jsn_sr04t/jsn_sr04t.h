#pragma once

#include "esphome/core/component.h"
#include "esphome/core/frame_reader.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome::jsn_sr04t {

enum Model {
  JSN_SR04T,
  AJ_SR04M,
};

class Jsnsr04tComponent final : public sensor::Sensor, public PollingComponent, public uart::UARTDevice {
 public:
  void set_model(Model model) { this->model_ = model; }

  // ========== INTERNAL METHODS ==========
  void update() override;
  void loop() override;
  void dump_config() override;

 protected:
  void check_frame_(const uint8_t *frame);
  Model model_;

  FrameReader<4, 0xFF> reader_;
};

}  // namespace esphome::jsn_sr04t
