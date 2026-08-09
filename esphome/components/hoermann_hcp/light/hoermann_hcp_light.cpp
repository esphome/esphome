#include "hoermann_hcp_light.h"

namespace esphome::hoermann_hcp {

light::LightTraits HoermannHcpLight::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::ON_OFF});
  return traits;
}

void HoermannHcpLight::setup() {
  this->parent_->add_on_state_callback([this]() { this->update_from_state_(); });
}

void HoermannHcpLight::write_state(light::LightState *state) {
  this->light_state_ = state;
  bool binary;
  state->current_values_as_binary(&binary);
  this->reported_on_ = binary;
  this->parent_->turn_light(binary);
}

void HoermannHcpLight::update_from_state_() {
  if (this->light_state_ == nullptr || !this->parent_->is_valid())
    return;
  bool light_on = this->parent_->is_light_on();
  if (this->reported_on_ != light_on) {
    this->reported_on_ = light_on;
    auto call = this->light_state_->make_call();
    call.set_state(light_on);
    call.perform();
  }
}

}  // namespace esphome::hoermann_hcp
