#pragma once

#include "esphome/core/defines.h"
#ifdef USE_ETHERNET
#include "esphome/core/automation.h"
#include "ethernet_component.h"

namespace esphome::ethernet {

template<typename... Ts> class EthernetConnectedCondition : public Condition<Ts...> {
 public:
  bool check(const Ts &...x) override { return global_eth_component->is_connected(); }
};

template<typename... Ts> class EthernetHasLinkCondition : public Condition<Ts...> {
 public:
  bool check(const Ts &...x) override { return global_eth_component->has_link(); }
};

template<typename... Ts> class EthernetEnableAction : public Action<Ts...> {
 public:
  void play(const Ts &...x) override { global_eth_component->enable(); }
};

template<typename... Ts> class EthernetDisableAction : public Action<Ts...> {
 public:
  void play(const Ts &...x) override { global_eth_component->disable(); }
};

}  // namespace esphome::ethernet
#endif
