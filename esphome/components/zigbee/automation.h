#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/defines.h"

#ifdef USE_ESP32
#ifdef USE_ZIGBEE
#include "zigbee.h"

namespace esphome::zigbee {

template<typename... Ts> class ResetZigbeeAction : public Action<Ts...>, public Parented<ZigBeeComponent> {
 public:
  void play(const Ts &...x) override { this->parent_->reset(); }
};

}  // namespace esphome::zigbee

#endif
#endif
