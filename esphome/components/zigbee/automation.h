#pragma once
#include "esphome/core/defines.h"
#ifdef USE_ZIGBEE
#ifdef USE_NRF52
#include "zigbee_zephyr.h"
#endif
namespace esphome::zigbee {

template<typename... Ts> class FactoryResetAction : public Action<Ts...>, public Parented<ZigbeeComponent> {
 public:
  void play(const Ts &...x) override { this->parent_->factory_reset(); }
};

}  // namespace esphome::zigbee

#endif
