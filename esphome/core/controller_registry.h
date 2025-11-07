#pragma once

#include "esphome/core/defines.h"

#ifdef USE_CONTROLLER_REGISTRY

#include "esphome/core/helpers.h"

// Forward declarations
namespace esphome {

class Controller;

#ifdef USE_BINARY_SENSOR
namespace binary_sensor {
class BinarySensor;
}
#endif

#ifdef USE_FAN
namespace fan {
class Fan;
}
#endif

#ifdef USE_LIGHT
namespace light {
class LightState;
}
#endif

#ifdef USE_SENSOR
namespace sensor {
class Sensor;
}
#endif

#ifdef USE_SWITCH
namespace switch_ {
class Switch;
}
#endif

#ifdef USE_COVER
namespace cover {
class Cover;
}
#endif

#ifdef USE_TEXT_SENSOR
namespace text_sensor {
class TextSensor;
}
#endif

#ifdef USE_CLIMATE
namespace climate {
class Climate;
}
#endif

#ifdef USE_NUMBER
namespace number {
class Number;
}
#endif

#ifdef USE_DATETIME_DATE
namespace datetime {
class DateEntity;
}
#endif

#ifdef USE_DATETIME_TIME
namespace datetime {
class TimeEntity;
}
#endif

#ifdef USE_DATETIME_DATETIME
namespace datetime {
class DateTimeEntity;
}
#endif

#ifdef USE_TEXT
namespace text {
class Text;
}
#endif

#ifdef USE_SELECT
namespace select {
class Select;
}
#endif

#ifdef USE_LOCK
namespace lock {
class Lock;
}
#endif

#ifdef USE_VALVE
namespace valve {
class Valve;
}
#endif

#ifdef USE_MEDIA_PLAYER
namespace media_player {
class MediaPlayer;
}
#endif

#ifdef USE_ALARM_CONTROL_PANEL
namespace alarm_control_panel {
class AlarmControlPanel;
}
#endif

#ifdef USE_EVENT
namespace event {
class Event;
}
#endif

#ifdef USE_UPDATE
namespace update {
class UpdateEntity;
}
#endif

/** Global registry for Controllers to receive entity state updates.
 *
 * This singleton registry allows Controllers (APIServer, WebServer) to receive
 * entity state change notifications without storing per-entity callbacks.
 *
 * Instead of each entity maintaining controller callbacks (32 bytes overhead per entity),
 * entities call ControllerRegistry::notify_*_update() which iterates the small list
 * of registered controllers (typically 2: API and WebServer).
 *
 * Controllers read state directly from entities using existing accessors (obj->state, etc.)
 * rather than receiving it as callback parameters that were being ignored anyway.
 *
 * Memory savings: 32 bytes per entity (2 controllers × 16 bytes std::function overhead)
 * Typical config (25 entities): ~780 bytes saved
 * Large config (80 entities): ~2,540 bytes saved
 */
class ControllerRegistry {
 public:
  /** Register a controller to receive entity state updates.
   *
   * Controllers should call this in their setup() method.
   * Typically only APIServer and WebServer register.
   */
  static void register_controller(Controller *controller);

#ifdef USE_BINARY_SENSOR
  static void notify_binary_sensor_update(binary_sensor::BinarySensor *obj);
#endif

#ifdef USE_FAN
  static void notify_fan_update(fan::Fan *obj);
#endif

#ifdef USE_LIGHT
  static void notify_light_update(light::LightState *obj);
#endif

#ifdef USE_SENSOR
  static void notify_sensor_update(sensor::Sensor *obj);
#endif

#ifdef USE_SWITCH
  static void notify_switch_update(switch_::Switch *obj);
#endif

#ifdef USE_COVER
  static void notify_cover_update(cover::Cover *obj);
#endif

#ifdef USE_TEXT_SENSOR
  static void notify_text_sensor_update(text_sensor::TextSensor *obj);
#endif

#ifdef USE_CLIMATE
  static void notify_climate_update(climate::Climate *obj);
#endif

#ifdef USE_NUMBER
  static void notify_number_update(number::Number *obj);
#endif

#ifdef USE_DATETIME_DATE
  static void notify_date_update(datetime::DateEntity *obj);
#endif

#ifdef USE_DATETIME_TIME
  static void notify_time_update(datetime::TimeEntity *obj);
#endif

#ifdef USE_DATETIME_DATETIME
  static void notify_datetime_update(datetime::DateTimeEntity *obj);
#endif

#ifdef USE_TEXT
  static void notify_text_update(text::Text *obj);
#endif

#ifdef USE_SELECT
  static void notify_select_update(select::Select *obj);
#endif

#ifdef USE_LOCK
  static void notify_lock_update(lock::Lock *obj);
#endif

#ifdef USE_VALVE
  static void notify_valve_update(valve::Valve *obj);
#endif

#ifdef USE_MEDIA_PLAYER
  static void notify_media_player_update(media_player::MediaPlayer *obj);
#endif

#ifdef USE_ALARM_CONTROL_PANEL
  static void notify_alarm_control_panel_update(alarm_control_panel::AlarmControlPanel *obj);
#endif

#ifdef USE_EVENT
  static void notify_event(event::Event *obj);
#endif

#ifdef USE_UPDATE
  static void notify_update(update::UpdateEntity *obj);
#endif

 protected:
  static StaticVector<Controller *, CONTROLLER_REGISTRY_MAX> controllers;
};

}  // namespace esphome

#endif  // USE_CONTROLLER_REGISTRY
