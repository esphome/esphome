#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/sx127x/sx127x.h"

namespace esphome::sx127x {

template<typename... Ts> class SendPacketAction final : public Action<Ts...>, public Parented<SX127x> {
  TEMPLATABLE_BYTES(data)

 public:
  void play(const Ts &...x) override {
    this->data_.template visit<SX127X_MAX_PACKET_SIZE>(
        [this](const uint8_t *data, size_t len) { this->parent_->transmit_packet(data, len); }, x...);
  }
};

}  // namespace esphome::sx127x
