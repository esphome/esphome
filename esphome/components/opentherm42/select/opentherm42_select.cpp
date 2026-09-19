#include "opentherm42_select.h"

namespace esphome::opentherm42 {

static const char *const TAG = "opentherm42.select";

void OpenTherm42Select::setup() {
  this->pref_ = this->make_entity_preference<size_t>();
  size_t index;
  if (this->pref_.load(&index) && this->has_index(index)) {
    this->publish_state(index);
    return;
  }
  this->publish_state(this->initial_option_);
}

void OpenTherm42Select::control(size_t index) {
  this->publish_state(index);
  this->pref_.save(&index);
}

void OpenTherm42Select::dump_config() { LOG_SELECT("", "OpenTherm42 Select", this); }

}  // namespace esphome::opentherm42
