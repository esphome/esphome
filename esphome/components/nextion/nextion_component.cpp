#include "nextion_component.h"

namespace esphome {
namespace nextion {

void NextionComponent::set_background_color(Color bco) {
  if (this->variable_name_ == this->variable_name_to_send_) {
    return;  // This is a variable. no need to set color
  }
  this->bco_ = bco;
  this->bco_needs_update_ = true;
  this->bco_is_set_ = true;
  this->update_component_settings();
}

void NextionComponent::set_background_pressed_color(Color bco2) {
  if (this->variable_name_ == this->variable_name_to_send_) {
    return;  // This is a variable. no need to set color
  }

  this->bco2_ = bco2;
  this->bco2_needs_update_ = true;
  this->bco2_is_set_ = true;
  this->update_component_settings();
}

void NextionComponent::set_foreground_color(Color pco) {
  if (this->variable_name_ == this->variable_name_to_send_) {
    return;  // This is a variable. no need to set color
  }
  this->pco_ = pco;
  this->pco_needs_update_ = true;
  this->pco_is_set_ = true;
  this->update_component_settings();
}

void NextionComponent::set_foreground_pressed_color(Color pco2) {
  if (this->variable_name_ == this->variable_name_to_send_) {
    return;  // This is a variable. no need to set color
  }
  this->pco2_ = pco2;
  this->pco2_needs_update_ = true;
  this->pco2_is_set_ = true;
  this->update_component_settings();
}

void NextionComponent::set_font_id(uint8_t font_id) {
  if (this->variable_name_ == this->variable_name_to_send_) {
    return;  // This is a variable. no need to set color
  }
  this->font_id_ = font_id;
  this->font_id_needs_update_ = true;
  this->font_id_is_set_ = true;
  this->update_component_settings();
}

void NextionComponent::set_visible(bool visible) {
  if (this->variable_name_ == this->variable_name_to_send_) {
    return;  // This is a variable. no need to set color
  }
  this->visible_ = visible;
  this->visible_needs_update_ = true;
  this->visible_is_set_ = true;
  this->update_component_settings();
}

void NextionComponent::update_component_settings(bool force_update) {
  if (this->nextion_->is_sleeping() || !this->nextion_->is_setup() || !this->visible_is_set_ ||
      (!this->visible_needs_update_ && !this->visible_)) {
    this->needs_to_send_update_ = true;
    return;
  }

  if (this->visible_needs_update_ || (force_update && this->visible_is_set_)) {
    std::string name_to_send = this->variable_name_;

    size_t pos = name_to_send.find_last_of('.');
    if (pos != std::string::npos) {
      name_to_send = name_to_send.substr(pos + 1);
    }

    this->visible_needs_update_ = false;

    if (this->visible_) {
      this->nextion_->show_component(name_to_send.c_str());
      this->send_state_to_nextion();
    } else {
      this->nextion_->hide_component(name_to_send.c_str());
      return;
    }
  }

  if (this->bco_needs_update_ || (force_update && this->bco2_is_set_)) {
    this->nextion_->set_component_background_color(this->variable_name_.c_str(), this->bco_);
    this->bco_needs_update_ = false;
  }
  if (this->bco2_needs_update_ || (force_update && this->bco2_is_set_)) {
    this->nextion_->set_component_pressed_background_color(this->variable_name_.c_str(), this->bco2_);
    this->bco2_needs_update_ = false;
  }
  if (this->pco_needs_update_ || (force_update && this->pco_is_set_)) {
    this->nextion_->set_component_foreground_color(this->variable_name_.c_str(), this->pco_);
    this->pco_needs_update_ = false;
  }
  if (this->pco2_needs_update_ || (force_update && this->pco2_is_set_)) {
    this->nextion_->set_component_pressed_foreground_color(this->variable_name_.c_str(), this->pco2_);
    this->pco2_needs_update_ = false;
  }

  if (this->font_id_needs_update_ || (force_update && this->font_id_is_set_)) {
    this->nextion_->set_component_font(this->variable_name_.c_str(), this->font_id_);
    this->font_id_needs_update_ = false;
  }
}
}  // namespace nextion
}  // namespace esphome
