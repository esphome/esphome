#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace vbus {

using message_parser_t = std::function<float(std::vector<uint8_t> &)>;

class VBus;

class VBusListener {
 public:
  void set_command(uint16_t command) { this->command_ = command; }
  void set_source(uint16_t source) { this->source_ = source; }
  void set_dest(uint16_t dest) { this->dest_ = dest; }

  void on_message(uint16_t command, uint16_t source, uint16_t dest, std::vector<uint8_t> &message);

 protected:
  uint16_t command_{0xffff};
  uint16_t source_{0xffff};
  uint16_t dest_{0xffff};

  virtual void handle_message(std::vector<uint8_t> &message) = 0;
};

class VBus : public uart::UARTDevice, public Component {
 public:
  void dump_config() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void register_listener(VBusListener *listener) { this->listeners_.push_back(listener); }

 protected:
  int state_{0};
  std::vector<uint8_t> buffer_;
  uint8_t protocol_;
  uint16_t source_;
  uint16_t dest_;
  uint16_t command_;
  uint8_t frames_;
  uint8_t cframe_;
  uint8_t fbytes_[6];
  int fbcount_;
  std::vector<VBusListener *> listeners_{};
};

}  // namespace vbus
}  // namespace esphome
