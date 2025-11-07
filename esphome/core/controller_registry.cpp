#include "esphome/core/controller_registry.h"
#include "esphome/core/controller.h"

namespace esphome {

std::vector<Controller *> ControllerRegistry::controllers_;

void ControllerRegistry::register_controller(Controller *controller) { controllers_.push_back(controller); }

void ControllerRegistry::unregister_controller(Controller *controller) {
  auto it = std::find(controllers_.begin(), controllers_.end(), controller);
  if (it != controllers_.end()) {
    controllers_.erase(it);
  }
}

#ifdef USE_BINARY_SENSOR
void ControllerRegistry::notify_binary_sensor_update(binary_sensor::BinarySensor *obj) {
  for (auto *controller : controllers_) {
    controller->on_binary_sensor_update(obj);
  }
}
#endif

#ifdef USE_FAN
void ControllerRegistry::notify_fan_update(fan::Fan *obj) {
  for (auto *controller : controllers_) {
    controller->on_fan_update(obj);
  }
}
#endif

#ifdef USE_LIGHT
void ControllerRegistry::notify_light_update(light::LightState *obj) {
  for (auto *controller : controllers_) {
    controller->on_light_update(obj);
  }
}
#endif

#ifdef USE_SENSOR
void ControllerRegistry::notify_sensor_update(sensor::Sensor *obj) {
  for (auto *controller : controllers_) {
    controller->on_sensor_update(obj);
  }
}
#endif

#ifdef USE_SWITCH
void ControllerRegistry::notify_switch_update(switch_::Switch *obj) {
  for (auto *controller : controllers_) {
    controller->on_switch_update(obj);
  }
}
#endif

#ifdef USE_COVER
void ControllerRegistry::notify_cover_update(cover::Cover *obj) {
  for (auto *controller : controllers_) {
    controller->on_cover_update(obj);
  }
}
#endif

#ifdef USE_TEXT_SENSOR
void ControllerRegistry::notify_text_sensor_update(text_sensor::TextSensor *obj) {
  for (auto *controller : controllers_) {
    controller->on_text_sensor_update(obj);
  }
}
#endif

#ifdef USE_CLIMATE
void ControllerRegistry::notify_climate_update(climate::Climate *obj) {
  for (auto *controller : controllers_) {
    controller->on_climate_update(obj);
  }
}
#endif

#ifdef USE_NUMBER
void ControllerRegistry::notify_number_update(number::Number *obj) {
  for (auto *controller : controllers_) {
    controller->on_number_update(obj);
  }
}
#endif

#ifdef USE_DATETIME_DATE
void ControllerRegistry::notify_date_update(datetime::DateEntity *obj) {
  for (auto *controller : controllers_) {
    controller->on_date_update(obj);
  }
}
#endif

#ifdef USE_DATETIME_TIME
void ControllerRegistry::notify_time_update(datetime::TimeEntity *obj) {
  for (auto *controller : controllers_) {
    controller->on_time_update(obj);
  }
}
#endif

#ifdef USE_DATETIME_DATETIME
void ControllerRegistry::notify_datetime_update(datetime::DateTimeEntity *obj) {
  for (auto *controller : controllers_) {
    controller->on_datetime_update(obj);
  }
}
#endif

#ifdef USE_TEXT
void ControllerRegistry::notify_text_update(text::Text *obj) {
  for (auto *controller : controllers_) {
    controller->on_text_update(obj);
  }
}
#endif

#ifdef USE_SELECT
void ControllerRegistry::notify_select_update(select::Select *obj) {
  for (auto *controller : controllers_) {
    controller->on_select_update(obj);
  }
}
#endif

#ifdef USE_LOCK
void ControllerRegistry::notify_lock_update(lock::Lock *obj) {
  for (auto *controller : controllers_) {
    controller->on_lock_update(obj);
  }
}
#endif

#ifdef USE_VALVE
void ControllerRegistry::notify_valve_update(valve::Valve *obj) {
  for (auto *controller : controllers_) {
    controller->on_valve_update(obj);
  }
}
#endif

#ifdef USE_MEDIA_PLAYER
void ControllerRegistry::notify_media_player_update(media_player::MediaPlayer *obj) {
  for (auto *controller : controllers_) {
    controller->on_media_player_update(obj);
  }
}
#endif

#ifdef USE_ALARM_CONTROL_PANEL
void ControllerRegistry::notify_alarm_control_panel_update(alarm_control_panel::AlarmControlPanel *obj) {
  for (auto *controller : controllers_) {
    controller->on_alarm_control_panel_update(obj);
  }
}
#endif

#ifdef USE_EVENT
void ControllerRegistry::notify_event(event::Event *obj, const std::string &event_type) {
  for (auto *controller : controllers_) {
    controller->on_event(obj, event_type);
  }
}
#endif

#ifdef USE_UPDATE
void ControllerRegistry::notify_update(update::UpdateEntity *obj) {
  for (auto *controller : controllers_) {
    controller->on_update(obj);
  }
}
#endif

}  // namespace esphome
