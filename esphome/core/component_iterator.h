#pragma once

#include "esphome/core/component.h"
#include "esphome/core/controller.h"
#include "esphome/core/helpers.h"

#ifdef USE_ESP32_CAMERA
#include "esphome/components/esp32_camera/esp32_camera.h"
#endif

namespace esphome {

#ifdef USE_API
namespace api {
class UserServiceDescriptor;
}  // namespace api
#endif

class ComponentIterator {
 public:
  void begin(bool include_internal = false);
  void advance();
  virtual bool on_begin();
#ifdef USE_BINARY_SENSOR
  virtual bool on_binary_sensor(binary_sensor::BinarySensor *binary_sensor) = 0;
#endif
#ifdef USE_COVER
  virtual bool on_cover(cover::Cover *cover) = 0;
#endif
#ifdef USE_FAN
  virtual bool on_fan(fan::Fan *fan) = 0;
#endif
#ifdef USE_LIGHT
  virtual bool on_light(light::LightState *light) = 0;
#endif
#ifdef USE_SENSOR
  virtual bool on_sensor(sensor::Sensor *sensor) = 0;
#endif
#ifdef USE_SWITCH
  virtual bool on_switch(switch_::Switch *a_switch) = 0;
#endif
#ifdef USE_BUTTON
  virtual bool on_button(button::Button *button) = 0;
#endif
#ifdef USE_TEXT_SENSOR
  virtual bool on_text_sensor(text_sensor::TextSensor *text_sensor) = 0;
#endif
#ifdef USE_API
  virtual bool on_service(api::UserServiceDescriptor *service);
#endif
#ifdef USE_ESP32_CAMERA
  virtual bool on_camera(esp32_camera::ESP32Camera *camera);
#endif
#ifdef USE_CLIMATE
  virtual bool on_climate(climate::Climate *climate) = 0;
#endif
#ifdef USE_NUMBER
  virtual bool on_number(number::Number *number) = 0;
#endif
#ifdef USE_TEXT
  virtual bool on_text(text::Text *text) = 0;
#endif
#ifdef USE_SELECT
  virtual bool on_select(select::Select *select) = 0;
#endif
#ifdef USE_LOCK
  virtual bool on_lock(lock::Lock *a_lock) = 0;
#endif
#ifdef USE_MEDIA_PLAYER
  virtual bool on_media_player(media_player::MediaPlayer *media_player);
#endif
#ifdef USE_ALARM_CONTROL_PANEL
  virtual bool on_alarm_control_panel(alarm_control_panel::AlarmControlPanel *a_alarm_control_panel) = 0;
#endif
  virtual bool on_end();

 protected:
  enum class IteratorState {
    NONE = 0,
    BEGIN,
#ifdef USE_BINARY_SENSOR
    BINARY_SENSOR,
#endif
#ifdef USE_COVER
    COVER,
#endif
#ifdef USE_FAN
    FAN,
#endif
#ifdef USE_LIGHT
    LIGHT,
#endif
#ifdef USE_SENSOR
    SENSOR,
#endif
#ifdef USE_SWITCH
    SWITCH,
#endif
#ifdef USE_BUTTON
    BUTTON,
#endif
#ifdef USE_TEXT_SENSOR
    TEXT_SENSOR,
#endif
#ifdef USE_API
    SERVICE,
#endif
#ifdef USE_ESP32_CAMERA
    CAMERA,
#endif
#ifdef USE_CLIMATE
    CLIMATE,
#endif
#ifdef USE_NUMBER
    NUMBER,
#endif
#ifdef USE_TEXT
    TEXT,
#endif
#ifdef USE_SELECT
    SELECT,
#endif
#ifdef USE_LOCK
    LOCK,
#endif
#ifdef USE_MEDIA_PLAYER
    MEDIA_PLAYER,
#endif
#ifdef USE_ALARM_CONTROL_PANEL
    ALARM_CONTROL_PANEL,
#endif
    MAX,
  } state_{IteratorState::NONE};
  size_t at_{0};
  bool include_internal_{false};
};

}  // namespace esphome
