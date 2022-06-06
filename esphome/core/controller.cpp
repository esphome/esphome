#include "controller.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {

void Controller::setup_controller(bool include_internal) {
#ifdef USE_BINARY_SENSOR
  for (auto *obj : App.get_binary_sensors()) {
    if (include_internal || !obj->is_internal())
      obj->add_on_state_callback([this, obj](bool state) { this->on_binary_sensor_update(obj, state); });
  }
#endif
#ifdef USE_FAN
  for (auto *obj : App.get_fans()) {
    if (include_internal || !obj->is_internal())
      obj->add_on_state_callback([this, obj]() { this->on_fan_update(obj); });
  }
#endif
#ifdef USE_LIGHT
  for (auto *obj : App.get_lights()) {
    if (include_internal || !obj->is_internal())
      obj->add_new_remote_values_callback([this, obj]() { this->on_light_update(obj); });
  }
#endif
#ifdef USE_SENSOR
  for (auto *obj : App.get_sensors()) {
    if (include_internal || !obj->is_internal())
      obj->add_on_state_callback([this, obj](float state) { this->on_sensor_update(obj, state); });
  }
#endif
#ifdef USE_SWITCH
  for (auto *obj : App.get_switches()) {
    if (include_internal || !obj->is_internal())
      obj->add_on_state_callback([this, obj](bool state) { this->on_switch_update(obj, state); });
  }
#endif
#ifdef USE_COVER
  for (auto *obj : App.get_covers()) {
    if (include_internal || !obj->is_internal())
      obj->add_on_state_callback([this, obj]() { this->on_cover_update(obj); });
  }
#endif
#ifdef USE_TEXT_SENSOR
  for (auto *obj : App.get_text_sensors()) {
    if (include_internal || !obj->is_internal())
      obj->add_on_state_callback([this, obj](const std::string &state) { this->on_text_sensor_update(obj, state); });
  }
#endif
#ifdef USE_CLIMATE
  for (auto *obj : App.get_climates()) {
    if (include_internal || !obj->is_internal())
      obj->add_on_state_callback([this, obj]() { this->on_climate_update(obj); });
  }
#endif
#ifdef USE_NUMBER
  for (auto *obj : App.get_numbers()) {
    if (include_internal || !obj->is_internal())
      obj->add_on_state_callback([this, obj](float state) { this->on_number_update(obj, state); });
  }
#endif
#ifdef USE_SELECT
  for (auto *obj : App.get_selects()) {
    if (include_internal || !obj->is_internal()) {
      obj->add_on_state_callback(
          [this, obj](const std::string &state, size_t index) { this->on_select_update(obj, state, index); });
    }
  }
#endif
#ifdef USE_LOCK
  for (auto *obj : App.get_locks()) {
    if (include_internal || !obj->is_internal())
      obj->add_on_state_callback([this, obj]() { this->on_lock_update(obj); });
  }
#endif
#ifdef USE_MEDIA_PLAYER
  for (auto *obj : App.get_media_players()) {
    if (include_internal || !obj->is_internal())
      obj->add_on_state_callback([this, obj]() { this->on_media_player_update(obj); });
  }
#endif
}

}  // namespace esphome
