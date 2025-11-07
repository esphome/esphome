#pragma once

#include "esphome/core/defines.h"
#include <vector>

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
 * Instead of each entity maintaining a list of controller callbacks (32 bytes overhead),
 * entities call ControllerRegistry::notify_*_update() which iterates the small list
 * of registered controllers (typically 2: API and WebServer).
 *
 * Memory savings: 32 bytes per entity (2 controllers × 16 bytes std::function overhead)
 * For 80 entities: 2,560 bytes saved
 */
class ControllerRegistry {
 public:
  /** Register a controller to receive entity state updates.
   *
   * Controllers should call this in their setup() method.
   * Typically only APIServer and WebServer register.
   */
  static void register_controller(Controller *controller);

  /** Unregister a controller (rarely used).
   *
   * Controllers are typically never unregistered in ESPHome's lifecycle,
   * but this is provided for completeness and testing.
   */
  static void unregister_controller(Controller *controller);

#ifdef USE_BINARY_SENSOR
  /** Notify all controllers of a binary sensor state update. */
  static void notify_binary_sensor_update(binary_sensor::BinarySensor *obj);
#endif

#ifdef USE_FAN
  /** Notify all controllers of a fan state update. */
  static void notify_fan_update(fan::Fan *obj);
#endif

#ifdef USE_LIGHT
  /** Notify all controllers of a light state update. */
  static void notify_light_update(light::LightState *obj);
#endif

#ifdef USE_SENSOR
  /** Notify all controllers of a sensor state update. */
  static void notify_sensor_update(sensor::Sensor *obj);
#endif

#ifdef USE_SWITCH
  /** Notify all controllers of a switch state update. */
  static void notify_switch_update(switch_::Switch *obj);
#endif

#ifdef USE_COVER
  /** Notify all controllers of a cover state update. */
  static void notify_cover_update(cover::Cover *obj);
#endif

#ifdef USE_TEXT_SENSOR
  /** Notify all controllers of a text sensor state update. */
  static void notify_text_sensor_update(text_sensor::TextSensor *obj);
#endif

#ifdef USE_CLIMATE
  /** Notify all controllers of a climate state update. */
  static void notify_climate_update(climate::Climate *obj);
#endif

#ifdef USE_NUMBER
  /** Notify all controllers of a number state update. */
  static void notify_number_update(number::Number *obj);
#endif

#ifdef USE_DATETIME_DATE
  /** Notify all controllers of a date entity state update. */
  static void notify_date_update(datetime::DateEntity *obj);
#endif

#ifdef USE_DATETIME_TIME
  /** Notify all controllers of a time entity state update. */
  static void notify_time_update(datetime::TimeEntity *obj);
#endif

#ifdef USE_DATETIME_DATETIME
  /** Notify all controllers of a datetime entity state update. */
  static void notify_datetime_update(datetime::DateTimeEntity *obj);
#endif

#ifdef USE_TEXT
  /** Notify all controllers of a text entity state update. */
  static void notify_text_update(text::Text *obj);
#endif

#ifdef USE_SELECT
  /** Notify all controllers of a select entity state update. */
  static void notify_select_update(select::Select *obj);
#endif

#ifdef USE_LOCK
  /** Notify all controllers of a lock state update. */
  static void notify_lock_update(lock::Lock *obj);
#endif

#ifdef USE_VALVE
  /** Notify all controllers of a valve state update. */
  static void notify_valve_update(valve::Valve *obj);
#endif

#ifdef USE_MEDIA_PLAYER
  /** Notify all controllers of a media player state update. */
  static void notify_media_player_update(media_player::MediaPlayer *obj);
#endif

#ifdef USE_ALARM_CONTROL_PANEL
  /** Notify all controllers of an alarm control panel state update. */
  static void notify_alarm_control_panel_update(alarm_control_panel::AlarmControlPanel *obj);
#endif

#ifdef USE_EVENT
  /** Notify all controllers of an event trigger. */
  static void notify_event(event::Event *obj, const std::string &event_type);
#endif

#ifdef USE_UPDATE
  /** Notify all controllers of an update entity state update. */
  static void notify_update(update::UpdateEntity *obj);
#endif

 protected:
  static std::vector<Controller *> controllers_;
};

}  // namespace esphome
