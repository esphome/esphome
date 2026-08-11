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
    // reported_on_ deliberately keeps tracking the lamp, not the request: publishing only ever repeats what
    // the door said, so a broadcast about anything else cannot drag the entity back to a stale value.
    return;
  }
  ESP_LOGW(TAG, "Light command was not accepted by the door");
  // The toggle already on the wire will still land, so show where the lamp is heading rather than the refused
  // request. Re-entering write_state() with that value then matches and queues nothing.
  this->publish_lamp_state_(effective_on);
}

void HoermannHcpLight::update_from_state_() {
  if (this->light_state_ == nullptr)
    return;
  if (!this->parent_->is_valid()) {
    this->status_set_warning("bus controller not responding");
    return;
  }
  this->status_clear_warning();
  // While a toggle is in flight the lamp still reads as its old self, and a refused request may have left
  // reported_on_ holding where the lamp is heading. Reconciling now would publish a value that write_state()
  // would immediately overturn, so wait. The hub flags a change when it drops a command, so a reconcile
  // still follows if the toggle never lands.
  if (this->parent_->is_light_toggle_pending())
    return;
  if (this->reported_on_ != this->parent_->is_light_on())
    this->publish_lamp_state_(this->parent_->is_light_on());
}

// Re-enters write_state(), where the request then matches the lamp and queues nothing.
void HoermannHcpLight::publish_lamp_state_(bool on) {
  this->reported_on_ = on;
  auto call = this->light_state_->make_call();
  call.set_state(this->reported_on_);
  // The bus reports the lamp on every broadcast, so nothing here is worth restoring from flash.
  call.set_save(false);
  call.perform();
}

}  // namespace esphome::hoermann_hcp
