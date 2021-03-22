#pragma once

#include "esphome/components/canbus/canbus.h"
#include "esphome/core/component.h"

namespace esphome {
namespace esp32_can {

class ESP32Can : public canbus::Canbus {
 public:
  void set_rx(GPIOPin *rx) { rx_ = rx; }
  void set_tx(GPIOPin *tx) { tx_ = tx; }
  ESP32Can(){};

 protected:
  bool setup_internal() override;
  canbus::Error send_message(struct canbus::CanFrame *frame) override;
  canbus::Error read_message(struct canbus::CanFrame *frame) override;

  GPIOPin *rx_{nullptr};
  GPIOPin *tx_{nullptr};
};

}  // namespace esp32_can
}  // namespace esphome
