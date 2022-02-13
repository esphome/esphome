#pragma once

#include "esphome/core/component.h"
#include "addressable_light.h"

namespace esphome {
namespace light {

class AddressableLightWrapper : public light::AddressableLight {
 public:
  explicit AddressableLightWrapper(light::LightState *light_state) : light_state_(light_state) {
    this->wrapper_state_ = new uint8_t[5];  // NOLINT(cppcoreguidelines-owning-memory)
  }

  int32_t size() const override { return 1; }

  void clear_effect_data() override { this->wrapper_state_[4] = 0; }

  light::LightTraits get_traits() override {
    LightTraits traits;

    // Choose which color mode to use.
    // This is ordered by how closely each color mode matches the underlying RGBW data structure used in LightPartition.
    ColorMode color_mode_precedence[] = {ColorMode::RGB_WHITE,
                                         ColorMode::RGB_COLD_WARM_WHITE,
                                         ColorMode::RGB_COLOR_TEMPERATURE,
                                         ColorMode::RGB,
                                         ColorMode::WHITE,
                                         ColorMode::COLD_WARM_WHITE,
                                         ColorMode::COLOR_TEMPERATURE,
                                         ColorMode::BRIGHTNESS,
                                         ColorMode::ON_OFF,
                                         ColorMode::UNKNOWN};

    LightTraits parent_traits = this->light_state_->get_traits();
    for (auto cm : color_mode_precedence) {
      if (parent_traits.supports_color_mode(cm)) {
        this->color_mode_ = cm;
        break;
      }
    }

    // Report a color mode that's compatible with both the partition and the underlying light
    switch (this->color_mode_) {
      case ColorMode::RGB_WHITE:
      case ColorMode::RGB_COLD_WARM_WHITE:
      case ColorMode::RGB_COLOR_TEMPERATURE:
        traits.set_supported_color_modes({light::ColorMode::RGB_WHITE});
        break;

      case ColorMode::RGB:
        traits.set_supported_color_modes({light::ColorMode::RGB});
        break;

      case ColorMode::WHITE:
      case ColorMode::COLD_WARM_WHITE:
      case ColorMode::COLOR_TEMPERATURE:
      case ColorMode::BRIGHTNESS:
        traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
        break;

      case ColorMode::ON_OFF:
        traits.set_supported_color_modes({light::ColorMode::ON_OFF});
        break;

      default:
        traits.set_supported_color_modes({light::ColorMode::UNKNOWN});
    }

    return traits;
  }

  void write_state(light::LightState *state) override {
    // Don't overwrite state if the underlying light is turned on
    if (this->light_state_->remote_values.is_on()) {
      this->mark_shown_();
      return;
    }

    float gamma = this->light_state_->get_gamma_correct();
    float r = gamma_uncorrect(this->wrapper_state_[0] / 255.0f, gamma);
    float g = gamma_uncorrect(this->wrapper_state_[1] / 255.0f, gamma);
    float b = gamma_uncorrect(this->wrapper_state_[2] / 255.0f, gamma);
    float w = gamma_uncorrect(this->wrapper_state_[3] / 255.0f, gamma);

    auto call = this->light_state_->make_call();

    float color_brightness = fmaxf(r, fmaxf(g, b));
    float brightness = fmaxf(color_brightness, w);
    if (brightness == 0.0f) {
      call.set_state(false);
    } else {
      color_brightness /= brightness;
      w /= brightness;

      call.set_state(true);
      call.set_color_mode_if_supported(this->color_mode_);
      call.set_brightness_if_supported(brightness);
      call.set_color_brightness_if_supported(color_brightness);
      call.set_red_if_supported(r);
      call.set_green_if_supported(g);
      call.set_blue_if_supported(b);
      call.set_white_if_supported(w);
      call.set_warm_white_if_supported(w);
      call.set_cold_white_if_supported(w);
    }
    call.set_transition_length_if_supported(0);
    call.set_publish(false);
    call.set_save(false);
    call.perform();

    this->mark_shown_();
  }

 protected:
  light::ESPColorView get_view_internal(int32_t index) const override {
    return {&this->wrapper_state_[0], &this->wrapper_state_[1], &this->wrapper_state_[2],
            &this->wrapper_state_[3], &this->wrapper_state_[4], &this->correction_};
  }

  light::LightState *light_state_;
  uint8_t *wrapper_state_;
  ColorMode color_mode_{ColorMode::UNKNOWN};
};

}  // namespace light
}  // namespace esphome
