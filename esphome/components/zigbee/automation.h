#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/defines.h"

#ifdef USE_ZIGBEE
#ifdef USE_ESP32
#include "zigbee_esp32.h"
#endif

namespace esphome::zigbee {
template<typename... Ts> class FactoryResetAction : public Action<Ts...>, public Parented<ZigbeeComponent> {
 public:
  void play(const Ts &...x) override { this->parent_->reset(); }
};
}  // namespace esphome::zigbee

#endif
