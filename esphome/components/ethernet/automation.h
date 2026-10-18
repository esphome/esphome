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

template<typename... Ts> class EthernetEnableAction final : public Action<Ts...> {
 public:
  void play(const Ts &...x) override { global_eth_component->enable(); }
};

template<typename... Ts> class EthernetDisableAction final : public Action<Ts...> {
 public:
  void play(const Ts &...x) override { global_eth_component->disable(); }
};

}  // namespace esphome::ethernet
#endif
