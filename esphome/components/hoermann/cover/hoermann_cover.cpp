#include "hoermann_cover.h"

#include "esphome/core/log.h"

namespace esphome::hoermann {

static const char *const TAG = "hoermann.cover";

cover::CoverTraits HoermannCover::get_traits() {
  auto traits = cover::CoverTraits();
  traits.set_supports_position(true);
  traits.set_supports_stop(true);
  traits.set_supports_toggle(true);
  return traits;
}

void HoermannCover::setup() {
  this->parent_->add_on_state_callback([this]() { this->update_from_state_(); });
}

void HoermannCover::dump_config() { LOG_COVER("", "Hoermann Cover", this); }

void HoermannCover::control(const cover::CoverCall &call) {
  if (call.get_stop()) {
    this->parent_->stop_door();
  }
  if (call.get_toggle().has_value()) {
    this->parent_->impulse_door();
  }
  if (call.get_position().has_value()) {
    this->parent_->set_position(call.get_position().value());
  }
}

void HoermannCover::update_from_state_() {
  if (!this->parent_->is_valid()) {
    this->status_set_warning();
    return;
  }
  this->status_clear_warning();

  float current_position = this->parent_->get_current_position();
  switch (this->parent_->get_door_state()) {
    case DoorState::OPENING:
      this->current_operation = cover::COVER_OPERATION_OPENING;
      break;
    case DoorState::CLOSING:
      this->current_operation = cover::COVER_OPERATION_CLOSING;
      break;
    case DoorState::MOVE_VENTING:
    case DoorState::MOVE_HALF:
      // These states carry no direction, so derive it from how the position is trending.
      this->current_operation =
          current_position > this->previous_position_ ? cover::COVER_OPERATION_OPENING : cover::COVER_OPERATION_CLOSING;
      break;
    default:
      this->current_operation = cover::COVER_OPERATION_IDLE;
      break;
  }
  this->position = current_position;

  const bool position_changed = this->previous_position_ != this->position;
  if (this->previous_operation_ != this->current_operation) {
    this->previous_operation_ = this->current_operation;
    this->publish_state();
  } else if (position_changed) {
    // Position updates arrive continuously while the door travels, so keep them out of flash.
    this->publish_state(false);
  }
  this->previous_position_ = this->position;
}

}  // namespace esphome::hoermann
