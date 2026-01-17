#include "fendt_light_output.h"

#ifdef USE_ESP32
namespace esphome {
namespace fendt_caravan {

void FendtLightOutput::write_state(light::LightState *state) {
  LampStateT ls(state->current_values.is_on(), state->current_values.get_brightness());
  ESP_LOGD(tag_, "Current state: %d, %f", state->current_values.is_on(), state->current_values.get_brightness());
  ESP_LOGD(tag_, "Remote state: %d, %f", state->remote_values.is_on(), state->remote_values.get_brightness());
  this->on_state_change_.call(this, ls);
}

void FendtLightOutput::on_decoded(const LampStateT state) {
  ESP_LOGD(tag_, "on_decoded called. Sate: %d, brightness: %f", state.status, state.state);
  this->light_state_->remote_values.set_state(state.status);
  this->light_state_->remote_values.set_brightness(state.state);
  this->light_state_->publish_state();
}

}  // namespace fendt_caravan
}  // namespace esphome
#endif
