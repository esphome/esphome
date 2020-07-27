#pragma once

#include "esphome/core/component.h"

#include <memory>
#include <set>
#include <map>

class UDP;

namespace esphome {
namespace e131 {

class E131AddressableLightEffect;

enum E131ListenMethod { E131_MULTICAST, E131_UNICAST };

const int E131_MAX_PROPERTY_VALUES_COUNT = 513;

struct E131Packet {
  uint16_t count;
  uint8_t values[E131_MAX_PROPERTY_VALUES_COUNT];
};

class E131Component : public esphome::Component {
 public:
  E131Component();
  ~E131Component();

  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 public:
  void add_effect(E131AddressableLightEffect *light_effect);
  void remove_effect(E131AddressableLightEffect *light_effect);

 public:
  void set_method(E131ListenMethod listen_method) { this->listen_method_ = listen_method; }

 protected:
  bool packet_(const std::vector<uint8_t> &data, int &universe, E131Packet &packet);
  bool process_(int universe, const E131Packet &packet);
  bool join_igmp_groups_();
  void join_(int universe);
  void leave_(int universe);

 protected:
  E131ListenMethod listen_method_{E131_MULTICAST};
  std::unique_ptr<UDP> udp_;
  std::set<E131AddressableLightEffect *> light_effects_;
  std::map<int, int> universe_consumers_;
  std::map<int, E131Packet> universe_packets_;
};

}  // namespace e131
}  // namespace esphome
