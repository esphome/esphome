#pragma once

#include "esphome/components/light/light_state.h"
#include "esphome/core/component.h"

namespace esphome::api_connection_destroy_light {

// Installs a hook that publishes a light from inside ~APIConnection(), reproducing the
// teardown reentrancy in issue #16798. See the component's __init__.py for the full story.
class ApiConnectionDestroyLight : public Component {
 public:
  void set_light(light::LightState *light) { this->light_ = light; }
  void setup() override;

 protected:
  light::LightState *light_{nullptr};
};

}  // namespace esphome::api_connection_destroy_light
