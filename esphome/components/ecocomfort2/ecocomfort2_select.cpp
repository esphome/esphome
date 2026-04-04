#ifdef USE_ESP32

#include "ecocomfort2_select.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ecocomfort2 {

static const char *const TAG = "ecocomfort2.select";

// --- Season Select ---

void Ecocomfort2SeasonSelect::dump_config() {
  ESP_LOGCONFIG(TAG, "Ecocomfort2 Season Select:");
  LOG_SELECT("  ", "Season", this);
}

void Ecocomfort2SeasonSelect::control(size_t index) {
  if (!this->parent_->is_ready()) {
    ESP_LOGW(TAG, "Not ready, cannot change season");
    return;
  }
  if (!this->parent_->has_config_data()) {
    ESP_LOGW(TAG, "Configuration not loaded yet, cannot change season");
    return;
  }

  bool is_summer = index == 1;
  const char *value = is_summer ? "Summer" : "Winter";
  this->parent_->write_season(is_summer);
  this->publish_state(value);
}

void Ecocomfort2SeasonSelect::on_config() {
  if (!this->parent_->has_config_data()) {
    return;
  }

  bool is_summer = this->parent_->get_season_summer();
  const char *value = is_summer ? "Summer" : "Winter";
  if (!this->has_state() || this->current_option() != value) {
    this->publish_state(value);
  }
}

// --- Free Cooling Select ---

void Ecocomfort2FreeCoolingSelect::dump_config() {
  ESP_LOGCONFIG(TAG, "Ecocomfort2 Free Cooling Select:");
  LOG_SELECT("  ", "Free Cooling", this);
}

void Ecocomfort2FreeCoolingSelect::control(size_t index) {
  if (!this->parent_->is_ready()) {
    ESP_LOGW(TAG, "Not ready, cannot change free cooling");
    return;
  }
  if (!this->parent_->has_config_data()) {
    ESP_LOGW(TAG, "Configuration not loaded yet, cannot change free cooling");
    return;
  }

  uint8_t level = static_cast<uint8_t>(index);
  const char *value = "Off";
  switch (level) {
    case 1:
      value = "Low";
      break;
    case 2:
      value = "Medium";
      break;
    case 3:
      value = "High";
      break;
    default:
      break;
  }

  this->parent_->write_free_cooling(level);
  this->publish_state(value);
}

void Ecocomfort2FreeCoolingSelect::on_config() {
  if (!this->parent_->has_config_data()) {
    return;
  }

  uint8_t level = this->parent_->get_free_cooling_level();
  const char *value = "Off";
  switch (level) {
    case 1:
      value = "Low";
      break;
    case 2:
      value = "Medium";
      break;
    case 3:
      value = "High";
      break;
    default:
      break;
  }
  if (!this->has_state() || this->current_option() != value) {
    this->publish_state(value);
  }
}

}  // namespace ecocomfort2
}  // namespace esphome

#endif
