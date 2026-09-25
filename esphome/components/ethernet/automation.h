#pragma once

#include "esphome/core/defines.h"
#ifdef USE_ETHERNET
#include "ethernet_component.h"

namespace esphome::ethernet {

template<typename... Ts> class EthernetConnectedCondition final : public Condition<Ts...> {
 public:
  bool check(const Ts &...x) override { return global_eth_component->is_connected(); }
};

template<typename... Ts> class EthernetEnabledCondition final : public Condition<Ts...> {
 public:
  bool check(const Ts &...x) override { return global_eth_component->is_enabled(); }
};

}  // namespace esphome::ethernet
#endif
