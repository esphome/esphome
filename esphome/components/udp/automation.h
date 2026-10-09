#pragma once

#include "udp_component.h"
#ifdef USE_NETWORK
#include "esphome/core/automation.h"

namespace esphome::udp {

/// Constant payloads up to this size are sent from the stack on ESP8266.
static constexpr size_t STACK_PAYLOAD_SIZE = 128;

template<typename... Ts> class UDPWriteAction final : public Action<Ts...>, public Parented<UDPComponent> {
  TEMPLATABLE_BYTES(data)

  void play(const Ts &...x) override {
    // One write per packet: WiFiUDP appends all or nothing, so a failed allocation never sends a partial packet
    this->data_.template visit<STACK_PAYLOAD_SIZE>(
        [this](const uint8_t *data, size_t len) { this->parent_->send_packet(data, len); }, x...);
  }
};

}  // namespace esphome::udp

#endif
