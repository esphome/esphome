#pragma once

#include "esphome/core/component.h"

#include "ESPAsyncE131.h"

#include <memory>
#include <set>

namespace esphome {
namespace e131 {

class E131AddressableLightEffect;

class E131Component : public esphome::Component {
 public:
  void loop() override;

 public:
  void add_effect(E131AddressableLightEffect *light_effect);
  void remove_effect(E131AddressableLightEffect *light_effect);

 public:
  void set_method(e131_listen_t listen_method) { this->listen_method_ = listen_method; }

 protected:
  void rebind_();
  bool process_(int universe, const e131_packet_t &packet);

 protected:
  e131_listen_t listen_method_{E131_MULTICAST};
  std::unique_ptr<ESPAsyncE131> e131_client_;
  std::set<E131AddressableLightEffect *> light_effects_;
  bool should_rebind_{false};
};

}  // namespace e131
}  // namespace esphome
