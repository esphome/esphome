#pragma once

#include "esphome/components/light/light_output.h"
#include "fendt_component.h"

namespace esphome {
namespace fendt_caravan {

#define FENDT_LIGHT_OUTPUT(name) \
 protected: \
  FendtLightOutput *name##_light_output_{nullptr}; \
\
 public: \
  void set_##name##_light_output(FendtLightOutput *light_output) { this->name##_light_output_ = light_output; }

struct lamp_state_t {
  bool status;
  float state;
};

class FendtLightOutput : public FendtComponent<lamp_state_t>, public light::LightOutput {
 public:
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
    return traits;
  }
  void set_state_change_callback(std::function<void(FendtLightOutput *, lamp_state_t state)> &&callback) {
    this->on_state_change_.add(std::move(callback));
  }

  void write_state(light::LightState *state) override {
    lamp_state_t ls(state->current_values.is_on(), state->current_values.get_brightness());
    ESP_LOGD(TAG, "Current state: %d, %f", state->current_values.is_on(), state->current_values.get_brightness());
    ESP_LOGD(TAG, "Remote state: %d, %f", state->remote_values.is_on(), state->remote_values.get_brightness());
    this->on_state_change_.call(this, ls);
  }
  void setup_state(light::LightState *state) override { this->light_state_ = state; }

 protected:
  void on_decoded(const lamp_state_t state) override {
    ESP_LOGD(TAG, "on_decoded called. Sate: %d, brightness: %f", state.status, state.state);
    this->light_state_->remote_values.set_state(state.status);
    this->light_state_->remote_values.set_brightness(state.state);
    this->light_state_->publish_state();
  }

 private:
  CallbackManager<void(FendtLightOutput *, lamp_state_t state)> on_state_change_{};
  const char *TAG = "FLO";
  light::LightState *light_state_{nullptr};
};
}  // namespace fendt_caravan
}  // namespace esphome
