#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome::systa_bus {

static constexpr uint16_t MESSAGE_TYPE_AQUA_SENSOR_DATA = 0xfc16;

static inline uint16_t get_message_type(std::vector<uint8_t> &message) { return (message[0] << 8) + message[1]; }

class SystaBusListener {
 public:
  virtual void handle_message(std::vector<uint8_t> &message) = 0;
};

class SystaBus : public uart::UARTDevice, public Component {
 public:
  void dump_config() override;
  void loop() override;

  void register_listener(SystaBusListener *listener) { this->listeners_.push_back(listener); }

 protected:
  int state_{0};
  std::vector<uint8_t> buffer_;
  uint16_t length_{0};
  uint16_t message_type_{0};
  std::vector<SystaBusListener *> listeners_{};
};

}  // namespace esphome::systa_bus
