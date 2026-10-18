#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#ifdef USE_VBUS_TX
#include "vbus_tx.h"
#endif
#ifdef USE_VBUS_DATAGRAM
#include "esphome/core/helpers.h"
#endif

namespace esphome::vbus {

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

class VBus final : public uart::UARTDevice, public Component {
 public:
  void dump_config() override;
  void loop() override;

  void register_listener(VBusListener *listener) { this->listeners_.push_back(listener); }
#ifdef USE_VBUS_TX
  void set_transmitter(VBusTransmitter *transmitter) { this->transmitter_ = transmitter; }
  bool request_bus() { return this->transmitter_ != nullptr && this->transmitter_->request_bus(); }
  bool has_bus() const { return this->transmitter_ != nullptr && this->transmitter_->has_bus(); }
  bool send_datagram(uint32_t grant_id, uint16_t command, uint16_t value_id, uint32_t value) {
    return this->transmitter_ != nullptr && this->transmitter_->send_datagram(grant_id, command, value_id, value);
  }
  void release_bus(uint32_t grant_id) {
    if (this->transmitter_ != nullptr)
      this->transmitter_->release_bus(grant_id);
  }
#endif
#ifdef USE_VBUS_DATAGRAM
  template<typename F> void add_on_datagram_callback(F &&callback) {
    this->datagram_callback_.add(std::forward<F>(callback));
  }
#endif

 protected:
#ifdef USE_VBUS_TX
  friend class VBusTransmitter;
  VBusTransmitter *transmitter_{nullptr};
#endif
  int state_{0};
  uint32_t last_rx_ms_{0};
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
#ifdef USE_VBUS_DATAGRAM
  CallbackManager<void(uint16_t, uint16_t, uint16_t, uint16_t, uint32_t)> datagram_callback_;
#endif
};

}  // namespace esphome::vbus
