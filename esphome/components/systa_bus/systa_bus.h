#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome::systa_bus {

static constexpr uint16_t BUFFER_SIZE = 258;
static constexpr uint16_t MESSAGE_TYPE_AQUA_SENSOR_DATA = 0xfc16;

static inline uint16_t get_message_type(const StaticVector<uint8_t, BUFFER_SIZE> &message) {
  return (static_cast<uint16_t>(message[0]) << 8) | static_cast<uint16_t>(message[1]);
}

enum class ParseState : uint8_t {
  IDLE,
  HEADER,
  BODY
};

class SystaBusListener {
 public:
  virtual void handle_message(const StaticVector<uint8_t, BUFFER_SIZE> &message) = 0;
};

class SystaBus : public uart::UARTDevice, public Component {
 public:
  void dump_config() override;
  void loop() override;

  void register_listener(SystaBusListener *listener) { this->listeners_.push_back(listener); }

 protected:
  ParseState state_{ParseState::IDLE};
  uint16_t length_{0};
  FixedVector<SystaBusListener *> listeners_{};
  StaticVector<uint8_t, BUFFER_SIZE> buffer_;
};

}  // namespace esphome::systa_bus
