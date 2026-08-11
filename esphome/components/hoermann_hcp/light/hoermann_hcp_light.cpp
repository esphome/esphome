#include "hoermann_hcp_light.h"

#include "esphome/core/log.h"

namespace esphome::hoermann_hcp {

static const char *const TAG = "hoermann_hcp.light";

light::LightTraits HoermannHcpLight::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::ON_OFF});
  return traits;
}

void HoermannHcpLight::setup() {
  this->parent_->add_on_state_callback([this]() { this->update_from_state_(); });
}

void HoermannHcpLight::setup_state(light::LightState *state) { this->light_state_ = state; }

void HoermannHcpLight::write_state(light::LightState *state) {
  bool binary;
  state->current_values_as_binary(&binary);
  if (this->parent_->turn_light(binary)) {
    this->reported_on_ = binary;
    return;
  }
  ESP_LOGW(TAG, "Light command was not accepted by the door");
  this->publish_light_state_();
}

void HoermannHcpLight::update_from_state_() {
  if (this->light_state_ == nullptr || !this->parent_->is_valid())
    return;
  if (this->reported_on_ != this->parent_->is_light_on())
    this->publish_light_state_();
}

// Re-enters write_state(), where the request then matches the door and queues nothing.
void HoermannHcpLight::publish_light_state_() {
  this->reported_on_ = this->parent_->is_light_on();
  auto call = this->light_state_->make_call();
  call.set_state(this->reported_on_);
  call.perform();
}

}  // namespace esphome::hoermann_hcp
