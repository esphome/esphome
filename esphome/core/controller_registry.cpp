#include "esphome/core/controller_registry.h"

#ifdef USE_CONTROLLER_REGISTRY

#include "esphome/core/controller.h"

namespace esphome {

StaticVector<Controller *, CONTROLLER_REGISTRY_MAX> ControllerRegistry::controllers;

void ControllerRegistry::register_controller(Controller *controller) { controllers.push_back(controller); }

// Macro for registry notification dispatch - iterates registered controllers and calls their handler
#define CONTROLLER_REGISTRY_NOTIFY(entity_type, entity_name) \
  void ControllerRegistry::notify_##entity_name##_update(entity_type *obj) { /* NOLINT(bugprone-macro-parentheses) */ \
    for (auto *controller : controllers) { \
      controller->on_##entity_name##_update(obj); \
    } \
  }

#ifdef USE_BINARY_SENSOR
CONTROLLER_REGISTRY_NOTIFY(binary_sensor::BinarySensor, binary_sensor)
#endif

#ifdef USE_FAN
CONTROLLER_REGISTRY_NOTIFY(fan::Fan, fan)
#endif

#ifdef USE_LIGHT
CONTROLLER_REGISTRY_NOTIFY(light::LightState, light)
#endif

#ifdef USE_SENSOR
CONTROLLER_REGISTRY_NOTIFY(sensor::Sensor, sensor)
#endif

#ifdef USE_SWITCH
CONTROLLER_REGISTRY_NOTIFY(switch_::Switch, switch)
#endif

#ifdef USE_COVER
CONTROLLER_REGISTRY_NOTIFY(cover::Cover, cover)
#endif

#ifdef USE_TEXT_SENSOR
CONTROLLER_REGISTRY_NOTIFY(text_sensor::TextSensor, text_sensor)
#endif

#ifdef USE_CLIMATE
CONTROLLER_REGISTRY_NOTIFY(climate::Climate, climate)
#endif

#ifdef USE_NUMBER
CONTROLLER_REGISTRY_NOTIFY(number::Number, number)
#endif

#ifdef USE_DATETIME_DATE
CONTROLLER_REGISTRY_NOTIFY(datetime::DateEntity, date)
#endif

#ifdef USE_DATETIME_TIME
CONTROLLER_REGISTRY_NOTIFY(datetime::TimeEntity, time)
#endif

#ifdef USE_DATETIME_DATETIME
CONTROLLER_REGISTRY_NOTIFY(datetime::DateTimeEntity, datetime)
#endif

#ifdef USE_TEXT
CONTROLLER_REGISTRY_NOTIFY(text::Text, text)
#endif

#ifdef USE_SELECT
CONTROLLER_REGISTRY_NOTIFY(select::Select, select)
#endif

#ifdef USE_LOCK
CONTROLLER_REGISTRY_NOTIFY(lock::Lock, lock)
#endif

#ifdef USE_VALVE
CONTROLLER_REGISTRY_NOTIFY(valve::Valve, valve)
#endif

#ifdef USE_MEDIA_PLAYER
CONTROLLER_REGISTRY_NOTIFY(media_player::MediaPlayer, media_player)
#endif

#ifdef USE_ALARM_CONTROL_PANEL
CONTROLLER_REGISTRY_NOTIFY(alarm_control_panel::AlarmControlPanel, alarm_control_panel)
#endif

#ifdef USE_EVENT
// Event is a special case - notify_event() calls on_event() (no "_update" suffix)
void ControllerRegistry::notify_event(event::Event *obj) {
  for (auto *controller : controllers) {
    controller->on_event(obj);
  }
}
#endif

#ifdef USE_UPDATE
// Update is a special case - notify_update() calls on_update() (no "_update" suffix)
void ControllerRegistry::notify_update(update::UpdateEntity *obj) {
  for (auto *controller : controllers) {
    controller->on_update(obj);
  }
}
#endif

#undef CONTROLLER_REGISTRY_NOTIFY

}  // namespace esphome

#endif  // USE_CONTROLLER_REGISTRY
