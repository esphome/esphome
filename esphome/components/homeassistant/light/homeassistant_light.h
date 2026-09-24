#pragma once

#include "esphome/components/light/light_output.h"
#include "esphome/core/component.h"

namespace esphome {
namespace homeassistant {

class HomeassistantLight : public light::LightOutput, public Component {
 public:
  void set_entity_id(const std::string &entity_id) { this->entity_id_ = entity_id; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

  light::LightTraits get_traits() override { return this->traits_; }

  void setup_state(light::LightState *state) override;
  void write_state(light::LightState *state) override;

 protected:
  void state_changed_(const std::string &state);

  void brightness_retrieved_(const std::string &brightness);
  void color_temp_retrieved_(const std::string &color_temp);
  void color_mode_retrieved_(const std::string &color_mode);
  void supported_color_modes_retrieved_(const std::string &supported_color_modes);
  void min_mireds_retrieved_(const std::string &min_mireds);
  void max_mireds_retrieved_(const std::string &max_mireds);

  std::string entity_id_;
  light::LightTraits traits_{};
  light::LightState *state_{nullptr};
  bool inited_{false};
};

}  // namespace homeassistant
}  // namespace esphome
