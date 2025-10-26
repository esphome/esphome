#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/automation.h"

#ifdef USE_ESP32
#ifdef USE_ZIGBEE
#include "zigbee.h"

namespace esphome {
namespace zigbee {

template<typename... Ts> class ResetZigbeeAction : public Action<Ts...>, public Parented<ZigBeeComponent> {
 public:
  void play(Ts... x) override { this->parent_->reset(); }
};

}  // namespace zigbee
}  // namespace esphome

#endif
#endif
