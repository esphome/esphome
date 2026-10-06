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
  this->status_set_warning(LOG_STR("waiting for the bus controller"));
  this->parent_->add_on_state_callback([this]() { this->update_from_state_(); });
}

void HoermannHcpLight::setup_state(light::LightState *state) { this->light_state_ = state; }

void HoermannHcpLight::write_state(light::LightState *state) {
  bool binary;
  state->current_values_as_binary(&binary);
  // A publish of ours only reaches write_state() a loop pass later, by which time the lamp may have moved on,
  // so it is recognised by the value it carried rather than by the current one.
  const optional<bool> published = this->published_state_;
  this->published_state_.reset();
  // LightState::setup() always performs a call, so the very first write here is the restored state coming back
  // rather than a request.
  const bool restored = !this->boot_replay_done_;
  this->boot_replay_done_ = true;
  const bool heading_on = this->parent_->is_light_heading_on();
  if (binary == heading_on)
    return;
  if (restored) {
    ESP_LOGD(TAG, "Ignoring the restored state, the door decides what the lamp is doing");
  } else if (published != binary) {
    if (!this->parent_->is_light_known()) {
      // Commanding a lamp that has not been read could switch off one that is already on.
      ESP_LOGW(TAG, "Door has not reported the lamp yet, ignoring the requested state");
    } else if (this->parent_->cancel_light_toggle() || this->parent_->toggle_light()) {
      // A toggle the controller has not fetched is withdrawn outright rather than fought with a second one.
      return;
    } else {
      ESP_LOGW(TAG, "Light command was not accepted by the door");
    }
  }
  // Nothing was sent, so the entity has to go back to showing the lamp rather than the request.
  this->publish_lamp_state_(heading_on);
}

void HoermannHcpLight::update_from_state_() {
  if (this->light_state_ == nullptr)
    return;
  if (!this->parent_->is_valid()) {
    this->status_set_warning(LOG_STR("bus controller not responding"));
    return;
  }
  if (!this->parent_->is_light_known()) {
    // Commands are refused until the door says, so say so rather than looking healthy and doing nothing.
    this->status_set_warning(LOG_STR("door has not reported the lamp"));
    return;
  }
  this->status_clear_warning();
  const bool heading_on = this->parent_->is_light_heading_on();
  if (this->light_state_->remote_values.is_on() != heading_on)
    this->publish_lamp_state_(heading_on);
}

// Re-enters write_state() a loop pass later, where published_state_ marks the write as ours.
void HoermannHcpLight::publish_lamp_state_(bool on) {
  this->published_state_ = on;
  auto call = this->light_state_->make_call();
  call.set_state(on);
  // The bus reports the lamp on every broadcast, so nothing here is worth restoring from flash.
  call.set_save(false);
  call.perform();
}

}  // namespace esphome::hoermann_hcp
