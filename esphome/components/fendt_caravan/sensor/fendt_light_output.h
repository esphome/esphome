#pragma once

#ifdef USE_ESP32
#include "esphome/components/light/light_output.h"
#include "esphome/components/fendt_caravan/caravan_sensor_base.h"

namespace esphome::fendt_caravan {

#define FENDT_LIGHT_OUTPUT(name) \
 protected: \
  FendtLightOutput *name##_light_output_{nullptr}; \
\
 public: \
  void set_##name##_light_output(FendtLightOutput *light_output) { this->name##_light_output_ = light_output; }

struct LampStateT {
  bool status;
  float state;
};

class FendtLightOutput : public CaravanSensorBase<LampStateT>, public light::LightOutput {
 public:
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
    return traits;
  }
  void set_state_change_callback(std::function<void(FendtLightOutput *, LampStateT state)> &&callback) {
    this->on_state_change_.add(std::move(callback));
  }

  void write_state(light::LightState *state) override;
  void setup_state(light::LightState *state) override { this->light_state_ = state; }

 protected:
  void on_decoded_(const LampStateT state) override;

 private:
  CallbackManager<void(FendtLightOutput *, LampStateT state)> on_state_change_{};
  light::LightState *light_state_{nullptr};
};
}  // namespace esphome::fendt_caravan
#endif
