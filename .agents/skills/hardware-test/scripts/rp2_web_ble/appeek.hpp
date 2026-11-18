#pragma once
// Reads the protected WiFiComponent::ap_clients_ for the test: explicit instantiation skips access checks.
#include "esphome/components/wifi/wifi_component.h"
namespace appeek {
struct Tag {
  using type = uint8_t esphome::wifi::WiFiComponent::*;
  friend type get(Tag);
};
template<typename T, typename T::type M> struct Rob {
  friend typename T::type get(T) { return M; }
};
template struct Rob<Tag, &esphome::wifi::WiFiComponent::ap_clients_>;
inline int ap_clients() { return esphome::wifi::global_wifi_component->*get(Tag{}); }
}  // namespace appeek
