#pragma once
#include "esphome/core/defines.h"
#ifdef USE_ZIGBEE
#ifdef USE_NRF52
#include "zigbee_zephyr.h"
#endif
namespace esphome::zigbee {

extern Zigbee *global_zigbee;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

template<typename... Ts> class FactoryResetAction : public Action<Ts...> {
 public:
  void play(Ts... x) override { global_zigbee->factory_reset(); }
};

}  // namespace esphome::zigbee

#endif
