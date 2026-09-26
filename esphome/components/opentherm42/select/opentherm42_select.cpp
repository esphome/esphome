#include "opentherm42_select.h"

namespace esphome::opentherm42 {

static const char *const TAG = "opentherm42.select";

void OpenTherm42Select::setup() {
  this->pref_ = this->make_entity_preference<size_t>();
  size_t index;
  if (!this->pref_.load(&index) || !this->has_index(index)) {
    index = this->index_of(this->initial_option_).value_or(0);
  }
  this->hub_->set_solar_storage_solar_mode_write_value(static_cast<uint8_t>(index));
  this->publish_state(index);
}

void OpenTherm42Select::control(size_t index) {
  this->hub_->set_solar_storage_solar_mode_write_value(static_cast<uint8_t>(index));
  this->publish_state(index);
  this->pref_.save(&index);
}

void OpenTherm42Select::dump_config() { LOG_SELECT("", "OpenTherm42 Select", this); }

}  // namespace esphome::opentherm42
