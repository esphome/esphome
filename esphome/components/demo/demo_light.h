#pragma once

#include "esphome/core/component.h"
#include "esphome/components/light/light_output.h"

namespace esphome {
namespace demo {

enum class DemoLightType {
  // binary
  TYPE_1,
  // brightness
  TYPE_2,
  // RGB
  TYPE_3,
  // RGBW
  TYPE_4,
  // RGBWW
  TYPE_5,
  // CWWW
  TYPE_6,
  // RGBW + color_interlock
  TYPE_7,
};

class DemoLight : public light::LightOutput, public Component {
 public:
  void set_type(DemoLightType type) { type_ = type; }
  light::LightTraits get_traits() override {
    light::LightTraits traits{};
    switch (type_) {
      case DemoLightType::TYPE_1:
        traits.set_supported_color_modes({light::ColorMode::ON_OFF});
        break;
      case DemoLightType::TYPE_2:
        traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
        break;
      case DemoLightType::TYPE_3:
        traits.set_supported_color_modes({light::ColorMode::RGB});
        break;
      case DemoLightType::TYPE_4:
        traits.set_supported_color_modes({light::ColorMode::RGB_WHITE});
        break;
      case DemoLightType::TYPE_5:
        traits.set_supported_color_modes({light::ColorMode::RGB_COLOR_TEMPERATURE});
        traits.set_min_mireds(153);
        traits.set_max_mireds(500);
        break;
      case DemoLightType::TYPE_6:
        traits.set_supported_color_modes({light::ColorMode::COLD_WARM_WHITE});
        traits.set_min_mireds(153);
        traits.set_max_mireds(500);
        break;
      case DemoLightType::TYPE_7:
        traits.set_supported_color_modes({light::ColorMode::RGB, light::ColorMode::WHITE});
        break;
    }
    return traits;
  }
  void write_state(light::LightState *state) override {
    // do nothing
  }

  DemoLightType type_;
};

}  // namespace demo
}  // namespace esphome
