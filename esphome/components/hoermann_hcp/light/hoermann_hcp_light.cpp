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
  if (!this->parent_->is_light_known()) {
    // Either the restored state replayed on boot or a request the door cannot be asked to take. Commanding a
    // lamp that has never been read could switch off one that is already on, so show what is known instead.
    if (binary != this->parent_->is_light_on()) {
      ESP_LOGD(TAG, "Lamp state not known yet, ignoring the requested state");
      this->publish_lamp_state_(this->parent_->is_light_on());
    }
    return;
  }
  // A toggle on its way inverts the lamp, and the lamp reads as its old self until the door reports it, so the
  // request has to be judged against where the lamp is heading rather than where it is.
  const bool effective_on = this->parent_->is_light_on() != this->parent_->is_light_toggle_in_flight();
  if (!this->synced_) {
    // The first write after the lamp becomes known is the restored state replayed on boot, not a request, so
    // adopt the lamp rather than command from a guess. Doing it here rather than on a state callback means a
    // door that reports nothing else still lets the entity settle.
    this->synced_ = true;
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
  this->status_clear_warning();
  // Nothing to settle on until the door has reported the lamp, and a toggle on its way will invert it again.
  if (!this->parent_->is_light_known() || this->parent_->is_light_toggle_in_flight())
    return;
  this->synced_ = true;
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
