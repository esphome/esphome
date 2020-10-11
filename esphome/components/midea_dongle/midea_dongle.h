#pragma once
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "midea_frame.h"

namespace esphome {
namespace midea_dongle {

class MideaDongle : public Component, public uart::UARTDevice {
public:
  float get_setup_priority() const override { return setup_priority::LATE; }
  void loop() override;

  void register_listener(MideaAppliance app_type, const std::function<void(Frame &)> &func);
  void write_frame(const Frame &frame) { this->write_array(frame.data(), frame.size()); }
  
protected:
  // Buffer
  uint8_t buf_[36];
  // Index
  uint8_t idx_{0};
  // Reverse receive counter
  uint8_t cnt_{2};
  // Reset receiver state
  void reset_() {
    this->idx_ = 0;
    this->cnt_ = 2;
  }

  std::vector<MideaListener> listeners_;
};

}  // namespace midea_dongle
}  // namespace esphome
