#include "controller.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {

void Controller::setup_controller() {
#ifdef USE_BINARY_SENSOR
  for (auto *obj : App.get_binary_sensors()) {
    if (!obj->is_internal())
      obj->add_on_state_callback([this, obj](bool state) { this->on_binary_sensor_update(obj, state); });
  }
#endif
#ifdef USE_FAN
  for (auto *obj : App.get_fans()) {
    if (!obj->is_internal())
      obj->add_on_state_callback([this, obj]() { this->on_fan_update(obj); });
  }
#endif
#ifdef USE_LIGHT
  for (auto *obj : App.get_lights()) {
    if (!obj->is_internal())
      obj->add_new_remote_values_callback([this, obj]() { this->on_light_update(obj); });
  }
#endif
#ifdef USE_SENSOR
  for (auto *obj : App.get_sensors()) {
    if (!obj->is_internal())
      obj->add_on_state_callback([this, obj](float state) { this->on_sensor_update(obj, state); });
  }
#endif
#ifdef USE_SWITCH
  for (auto *obj : App.get_switches()) {
    if (!obj->is_internal())
      obj->add_on_state_callback([this, obj](bool state) { this->on_switch_update(obj, state); });
  }
#endif
#ifdef USE_COVER
  for (auto *obj : App.get_covers()) {
    if (!obj->is_internal())
      obj->add_on_state_callback([this, obj]() { this->on_cover_update(obj); });
  }
#endif
#ifdef USE_TEXT_SENSOR
  for (auto *obj : App.get_text_sensors()) {
    if (!obj->is_internal())
      obj->add_on_state_callback([this, obj](std::string state) { this->on_text_sensor_update(obj, state); });
  }
#endif
#ifdef USE_CLIMATE
  for (auto *obj : App.get_climates()) {
    if (!obj->is_internal())
      obj->add_on_state_callback([this, obj]() { this->on_climate_update(obj); });
  }
#endif
}

}  // namespace esphome
