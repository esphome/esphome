#include "hoermann_cover.h"

namespace esphome::hoermann {

cover::CoverTraits HoermannCover::get_traits() {
  auto traits = cover::CoverTraits();
  traits.set_is_assumed_state(false);
  traits.set_supports_position(true);
  traits.set_supports_tilt(false);
  traits.set_supports_stop(true);
  traits.set_supports_toggle(true);
  return traits;
}

void HoermannCover::setup() {
  this->parent_->add_on_state_callback([this]() { this->update_from_state_(); });
}

void HoermannCover::control(const cover::CoverCall &call) {
  if (call.get_stop()) {
    this->parent_->stop_door();
  }
  if (call.get_toggle().has_value()) {
    this->parent_->impulse_door();
  }
  if (call.get_position().has_value()) {
    float pos = call.get_position().value();
    if (pos >= 1.0f) {
      this->parent_->open_door();
    } else if (pos <= 0.0f) {
      this->parent_->close_door();
    } else {
      this->parent_->set_position(static_cast<int>(pos * 100.0f));
    }
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
      this->current_operation =
          this->previous_position_ > current_position ? cover::COVER_OPERATION_OPENING : cover::COVER_OPERATION_CLOSING;
      break;
    default:
      this->current_operation = cover::COVER_OPERATION_IDLE;
      break;
  }
  this->position = current_position;

  bool operation_changed = this->previous_operation_ != this->current_operation;
  bool position_changed = this->previous_position_ != this->position;
  if (operation_changed) {
    this->publish_state();
    this->previous_operation_ = this->current_operation;
  } else if (position_changed) {
    this->publish_state(false);
  }
  if (position_changed) {
    this->previous_position_ = this->position;
  }
}

}  // namespace esphome::hoermann
