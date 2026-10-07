#pragma once

#include "udp_component.h"
#ifdef USE_NETWORK
#include "esphome/core/automation.h"

namespace esphome::udp {

template<typename... Ts> class UDPWriteAction final : public Action<Ts...>, public Parented<UDPComponent> {
  TEMPLATABLE_BYTES(data)

  void play(const Ts &...x) override {
    this->data_.visit([this](const uint8_t *data, size_t len) { this->parent_->send_packet(data, len); }, x...);
  }
};

}  // namespace esphome::udp

#endif
