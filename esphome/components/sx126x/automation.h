#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/sx126x/sx126x.h"

namespace esphome::sx126x {

template<typename... Ts> class SendPacketAction final : public Action<Ts...>, public Parented<SX126x> {
  TEMPLATABLE_BYTES(data)

  void play(const Ts &...x) override {
    this->data_.template visit<SX126X_MAX_PACKET_SIZE>(
        [this](const uint8_t *data, size_t len) { this->parent_->transmit_packet(data, len); }, x...);
  }
};

}  // namespace esphome::sx126x
