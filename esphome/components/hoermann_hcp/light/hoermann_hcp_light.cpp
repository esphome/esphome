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
  // LightState::setup() always performs a call, so the very first write here is the restored state coming back
  // rather than a request. Spending that allowance on whichever branch runs keeps a later real request out of it.
  const bool restore_replay = !this->boot_replay_done_;
  this->boot_replay_done_ = true;
  if (!this->parent_->is_light_known()) {
    // Commanding a lamp that has never been read could switch off one that is already on, so show what is
    // known instead. Nothing is known yet, so that is the lamp's default.
    if (binary != this->parent_->is_light_on()) {
      if (!restore_replay)
        ESP_LOGW(TAG, "Door has not reported the lamp yet, ignoring the requested state");
      this->publish_lamp_state_(this->parent_->is_light_on());
    }
    return;
  }
  // Each toggle on its way inverts the lamp, and the lamp reads as its old self until the door reports it, so
  // the request has to be judged against where the toggles are taking it rather than where it is.
  const bool effective_on = this->parent_->is_light_on() != (this->parent_->pending_light_toggles() % 2 != 0);
  if (restore_replay) {
    if (binary != effective_on)
      this->publish_lamp_state_(effective_on);
    return;
  }
  if (binary == effective_on)
    return;
  if (this->parent_->cancel_light_toggle())
    return;
  if (this->parent_->toggle_light())
    return;
  ESP_LOGW(TAG, "Light command was not accepted by the door");
  this->publish_lamp_state_(effective_on);
}

void HoermannHcpLight::update_from_state_() {
  if (this->light_state_ == nullptr)
    return;
  if (!this->parent_->is_valid()) {
    this->status_set_warning("bus controller not responding");
    return;
  }
  if (!this->parent_->is_light_known()) {
    // Commands are refused until the door says, so say so rather than looking healthy and doing nothing.
    this->status_set_warning("door has not reported the lamp");
    return;
  }
  this->status_clear_warning();
  // A toggle on its way will invert the lamp, so what it reads now is not what to settle on.
  if (this->parent_->pending_light_toggles() != 0)
    return;
  if (this->light_state_->remote_values.is_on() != this->parent_->is_light_on())
    this->publish_lamp_state_(this->parent_->is_light_on());
}

// Re-enters write_state(), where the request then matches the lamp and queues nothing.
void HoermannHcpLight::publish_lamp_state_(bool on) {
  auto call = this->light_state_->make_call();
  call.set_state(on);
  // The bus reports the lamp on every broadcast, so nothing here is worth restoring from flash.
  call.set_save(false);
  call.perform();
}

}  // namespace esphome::hoermann_hcp
