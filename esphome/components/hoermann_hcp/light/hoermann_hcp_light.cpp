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
  // Nothing is known about the lamp until the bus controller is heard from, so flag the entity until then.
  this->status_set_warning("waiting for the bus controller");
  this->parent_->add_on_state_callback([this]() { this->update_from_state_(); });
}

void HoermannHcpLight::setup_state(light::LightState *state) { this->light_state_ = state; }

void HoermannHcpLight::write_state(light::LightState *state) {
  bool binary;
  state->current_values_as_binary(&binary);
  // A queued toggle has not reached the lamp yet but will invert it, so the request has to be judged against
  // where the lamp is heading. Skipping a matching request is also what stops the state callback from
  // toggling the lamp back and forth forever.
  const bool effective_on = this->parent_->is_light_on() != this->parent_->is_light_toggle_pending();
  if (binary == effective_on || this->parent_->cancel_light_toggle() || this->parent_->toggle_light()) {
    this->reported_on_ = binary;
    return;
  }
  ESP_LOGW(TAG, "Light command was not accepted by the door");
  // A toggle already on the wire settles this entity from the next broadcast; republishing now would only
  // re-enter write_state() with the same mismatch.
  if (!this->parent_->is_light_toggle_pending())
    this->publish_light_state_();
}

void HoermannHcpLight::update_from_state_() {
  if (this->light_state_ == nullptr)
    return;
  if (!this->parent_->is_valid()) {
    this->status_set_warning("bus controller not responding");
    return;
  }
  this->status_clear_warning();
  // A queued toggle will invert the lamp, so the reported state is not the one to reconcile against yet. The
  // hub flags a change when it drops a command, so a reconcile still follows if the toggle never lands.
  if (this->parent_->is_light_toggle_pending())
    return;
  if (this->reported_on_ != this->parent_->is_light_on())
    this->publish_light_state_();
}

// Re-enters write_state(), where the request then matches the lamp and queues nothing.
void HoermannHcpLight::publish_light_state_() {
  this->reported_on_ = this->parent_->is_light_on();
  auto call = this->light_state_->make_call();
  call.set_state(this->reported_on_);
  // The bus reports the lamp on every broadcast, so nothing here is worth restoring from flash.
  call.set_save(false);
  call.perform();
}

}  // namespace esphome::hoermann_hcp
