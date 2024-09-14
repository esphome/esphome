#pragma once

#ifdef USE_ESP32

#include "esphome/components/canbus/canbus.h"
#include "esphome/core/component.h"

namespace esphome {
namespace esp32_can {

class ESP32Can : public canbus::Canbus {
 public:
  void set_rx(int rx) { rx_ = rx; }
  void set_tx(int tx) { tx_ = tx; }
  void set_tx_queue_len(uint32_t tx_queue_len) { this->tx_queue_len_ = tx_queue_len; }
  void set_rx_queue_len(uint32_t rx_queue_len) { this->rx_queue_len_ = rx_queue_len; }
  ESP32Can(){};

 protected:
  bool setup_internal() override;
  canbus::Error send_message(struct canbus::CanFrame *frame) override;
  canbus::Error read_message(struct canbus::CanFrame *frame) override;

  int rx_{-1};
  int tx_{-1};
  optional<uint32_t> tx_queue_len_{};
  optional<uint32_t> rx_queue_len_{};
};

}  // namespace esp32_can
}  // namespace esphome

#endif
