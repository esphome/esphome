#pragma once
#include "esphome/core/defines.h"
#ifdef USE_ZIGBEE
#ifdef USE_ESP32
#include "zigbee_esp32.h"
#endif
#if defined(USE_NRF52) || defined(USE_ZEPHYR_FRAMEWORK_ZIGBEE)
#include "zigbee_zephyr.h"
#endif
namespace esphome::zigbee {

template<typename... Ts> class FactoryResetAction final : public Action<Ts...>, public Parented<ZigbeeComponent> {
 public:
  void play(const Ts &...x) override { this->parent_->factory_reset(); }
};

}  // namespace esphome::zigbee

#endif
