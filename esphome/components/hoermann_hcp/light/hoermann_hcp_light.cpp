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
  if (!this->synced_) {
    // Until the lamp has been read once there is nothing to command against: this is either the restored state
    // replayed on boot, which must not switch off a lamp that is already on, or a request the door cannot be
    // asked to take. Both end up showing the last lamp state known, which on boot is what the entity shows.
    if (binary != this->shown_on_) {
      ESP_LOGD(TAG, "Lamp state not known yet, ignoring the requested state");
      this->publish_lamp_state_(this->shown_on_);
    }
    return;
  }
  // Where the lamp is heading. A toggle already released onto the wire has stopped being pending but has not
  // been reported either, so while a report is outstanding only shown_on_ says where the lamp will end up.
  const bool effective_on = this->awaiting_lamp_
                                ? this->shown_on_
                                : (this->parent_->is_light_on() != this->parent_->is_light_toggle_pending());
  if (binary == effective_on)
    return;
  if (this->parent_->cancel_light_toggle()) {
    // Withdrawn before the controller saw it, so the lamp stays where it is and nothing is outstanding.
    this->awaiting_lamp_ = false;
  } else if (this->parent_->toggle_light()) {
    this->awaiting_lamp_ = true;
  } else {
    ESP_LOGW(TAG, "Light command was not accepted by the door");
    // Only a toggle already on the wire will still land. A refusal for any other reason - the bus is down, or a
    // door command holds the slot - leaves nothing new outstanding, and must not disturb an earlier wait.
    if (this->parent_->is_light_toggle_pending())
      this->awaiting_lamp_ = true;
    this->publish_lamp_state_(effective_on);
    return;
  }
  this->shown_on_ = binary;
}

void HoermannHcpLight::update_from_state_() {
  if (this->light_state_ == nullptr)
    return;
  if (!this->parent_->is_valid()) {
    this->status_set_warning("bus controller not responding");
    return;
  }
  this->status_clear_warning();
  const bool dropped = this->parent_->take_light_command_dropped();
  const bool lamp_on = this->parent_->is_light_on();
  // Keep showing the requested state until the lamp confirms it or the hub reports that the command was dropped.
  if (this->awaiting_lamp_ && lamp_on != this->shown_on_ && !dropped)
    return;
  this->awaiting_lamp_ = false;
  // Bus traffic alone does not report the lamp, and commanding against a lamp that was never read is what the
  // boot replay must avoid, so the entity only starts honouring writes once the door has actually said.
  if (this->parent_->is_light_known())
    this->synced_ = true;
  if (this->shown_on_ != lamp_on)
    this->publish_lamp_state_(lamp_on);
}

// Re-enters write_state(), where the request then matches the lamp and queues nothing.
void HoermannHcpLight::publish_lamp_state_(bool on) {
  this->shown_on_ = on;
  auto call = this->light_state_->make_call();
  call.set_state(on);
  // The bus reports the lamp on every broadcast, so nothing here is worth restoring from flash.
  call.set_save(false);
  call.perform();
}

}  // namespace esphome::hoermann_hcp
