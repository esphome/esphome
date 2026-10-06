#include "hoermann_hcp_cover.h"

#include "esphome/core/log.h"

namespace esphome::hoermann_hcp {

static const char *const TAG = "hoermann_hcp.cover";

cover::CoverTraits HoermannHcpCover::get_traits() {
  cover::CoverTraits traits;
  traits.set_supports_position(true);
  traits.set_supports_stop(true);
  traits.set_supports_toggle(true);
  return traits;
}

void HoermannHcpCover::setup() {
  // Nothing is published before the bus controller is heard from, and the untouched position reads as fully
  // open, so flag the entity until the first contact clears it again.
  this->status_set_warning("waiting for the bus controller");
  this->parent_->add_on_state_callback([this]() { this->update_from_state_(); });
}

void HoermannHcpCover::dump_config() { LOG_COVER("", "Hoermann HCP Cover", this); }

void HoermannHcpCover::control(const cover::CoverCall &call) {
  bool accepted = true;
  if (call.get_stop())
    accepted &= this->parent_->stop_door();
  if (call.get_toggle().has_value())
    accepted &= this->parent_->impulse_door();
  if (const auto position = call.get_position())
    accepted &= this->parent_->set_position(*position);
  if (!accepted) {
    // The command never reached the door, so publish the unchanged state over the one the caller assumed.
    ESP_LOGW(TAG, "Command was not accepted by the door");
    this->publish_state(false);
  }
}

void HoermannHcpCover::update_from_state_() {
  if (!this->parent_->is_valid()) {
    this->status_set_warning();
    // The door can now move unheard, so drop the baseline a direction would be inferred from and stop
    // reporting motion instead of leaving the cover travelling until the controller returns.
    this->previous_position_ = NAN;
    if (this->current_operation != cover::COVER_OPERATION_IDLE) {
      this->current_operation = cover::COVER_OPERATION_IDLE;
      this->publish_state();
    }
    return;
  }
  this->status_clear_warning();

  const auto previous_operation = this->current_operation;
  const float current_position = this->parent_->get_current_position();
  switch (this->parent_->get_door_state()) {
    case DoorState::OPENING:
      this->current_operation = cover::COVER_OPERATION_OPENING;
      break;
    case DoorState::CLOSING:
      this->current_operation = cover::COVER_OPERATION_CLOSING;
      break;
    case DoorState::MOVE_VENTING:
    case DoorState::MOVE_HALF:
      // These states carry no direction, so keep the current one until the position actually moves.
      if (!std::isnan(this->previous_position_) && current_position != this->previous_position_) {
        this->current_operation = current_position > this->previous_position_ ? cover::COVER_OPERATION_OPENING
                                                                              : cover::COVER_OPERATION_CLOSING;
      }
      break;
    default:
      this->current_operation = cover::COVER_OPERATION_IDLE;
      break;
  }
  this->previous_position_ = current_position;

  // Compare against the position last published, which starts at COVER_OPEN rather than at zero.
  const bool changed = this->position != current_position || previous_operation != this->current_operation;
  this->position = current_position;
  if (changed) {
    // The bus reports the position on every broadcast, so nothing here is worth restoring from flash.
    this->publish_state(false);
  }
}

}  // namespace esphome::hoermann_hcp
