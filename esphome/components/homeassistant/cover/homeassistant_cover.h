#pragma once

#include "esphome/core/component.h"
#include "esphome/components/cover/cover.h"

namespace esphome {
namespace homeassistant {

class HomeassistantCover : public cover::Cover, public EntityBase_State, public Component {
 public:
  void set_entity_id(const std::string &entity_id) { entity_id_ = entity_id; }
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  cover::CoverTraits get_traits() override;

 protected:
  void control(const cover::CoverCall &call) override;
  std::string entity_id_;
};

}  // namespace homeassistant
}  // namespace esphome
