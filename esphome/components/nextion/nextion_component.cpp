#include "nextion_component.h"

namespace esphome {
namespace nextion {

void NextionComponent::set_background_color(Color bco) {
  if (this->variable_name_ == this->variable_name_to_send_) {
    return;  // This is a variable. no need to set color
  }
  this->bco_ = bco;
  this->component_flags_.bco_needs_update = true;
  this->component_flags_.bco_is_set = true;
  this->update_component_settings();
}

void NextionComponent::set_background_pressed_color(Color bco2) {
  if (this->variable_name_ == this->variable_name_to_send_) {
    return;  // This is a variable. no need to set color
  }

  this->bco2_ = bco2;
  this->component_flags_.bco2_needs_update = true;
  this->component_flags_.bco2_is_set = true;
  this->update_component_settings();
}

void NextionComponent::set_foreground_color(Color pco) {
  if (this->variable_name_ == this->variable_name_to_send_) {
    return;  // This is a variable. no need to set color
  }
  this->pco_ = pco;
  this->component_flags_.pco_needs_update = true;
  this->component_flags_.pco_is_set = true;
  this->update_component_settings();
}

void NextionComponent::set_foreground_pressed_color(Color pco2) {
  if (this->variable_name_ == this->variable_name_to_send_) {
    return;  // This is a variable. no need to set color
  }
  this->pco2_ = pco2;
  this->component_flags_.pco2_needs_update = true;
  this->component_flags_.pco2_is_set = true;
  this->update_component_settings();
}

void NextionComponent::set_font_id(uint8_t font_id) {
  if (this->variable_name_ == this->variable_name_to_send_) {
    return;  // This is a variable. no need to set color
  }
  this->font_id_ = font_id;
  this->component_flags_.font_id_needs_update = true;
  this->component_flags_.font_id_is_set = true;
  this->update_component_settings();
}

void NextionComponent::set_visible(bool visible) {
  if (this->variable_name_ == this->variable_name_to_send_) {
    return;  // This is a variable. no need to set color
  }
  this->component_flags_.visible = visible;
  this->component_flags_.visible_needs_update = true;
  this->component_flags_.visible_is_set = true;
  this->update_component_settings();
}

void NextionComponent::update_component_settings(bool force_update) {
  if (this->nextion_->is_sleeping() || !this->nextion_->is_setup() || !this->component_flags_.visible_is_set ||
      (!this->component_flags_.visible_needs_update && !this->component_flags_.visible)) {
    this->needs_to_send_update_ = true;
    return;
  }

  if (this->component_flags_.visible_needs_update || (force_update && this->component_flags_.visible_is_set)) {
    std::string name_to_send = this->variable_name_;

    size_t pos = name_to_send.find_last_of('.');
    if (pos != std::string::npos) {
      name_to_send = name_to_send.substr(pos + 1);
    }

    this->component_flags_.visible_needs_update = false;

    this->nextion_->set_component_visibility(name_to_send.c_str(), this->component_flags_.visible);
    if (!this->component_flags_.visible) {
      return;
    }
    this->send_state_to_nextion();
  }

  if (this->component_flags_.bco_needs_update || (force_update && this->component_flags_.bco2_is_set)) {
    this->nextion_->set_component_background_color(this->variable_name_.c_str(), this->bco_);
    this->component_flags_.bco_needs_update = false;
  }
  if (this->component_flags_.bco2_needs_update || (force_update && this->component_flags_.bco2_is_set)) {
    this->nextion_->set_component_pressed_background_color(this->variable_name_.c_str(), this->bco2_);
    this->component_flags_.bco2_needs_update = false;
  }
  if (this->component_flags_.pco_needs_update || (force_update && this->component_flags_.pco_is_set)) {
    this->nextion_->set_component_foreground_color(this->variable_name_.c_str(), this->pco_);
    this->component_flags_.pco_needs_update = false;
  }
  if (this->component_flags_.pco2_needs_update || (force_update && this->component_flags_.pco2_is_set)) {
    this->nextion_->set_component_pressed_foreground_color(this->variable_name_.c_str(), this->pco2_);
    this->component_flags_.pco2_needs_update = false;
  }

  if (this->component_flags_.font_id_needs_update || (force_update && this->component_flags_.font_id_is_set)) {
    this->nextion_->set_component_font(this->variable_name_.c_str(), this->font_id_);
    this->component_flags_.font_id_needs_update = false;
  }
}
}  // namespace nextion
}  // namespace esphome
