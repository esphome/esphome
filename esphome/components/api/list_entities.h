#pragma once

#include "esphome/core/component.h"
#include "esphome/core/component_iterator.h"
#include "esphome/core/defines.h"

namespace esphome {
namespace api {

class APIConnection;

class ListEntitiesIterator : public ComponentIterator {
 public:
  ListEntitiesIterator(APIConnection *client);
#ifdef USE_BINARY_SENSOR
  bool on_binary_sensor(binary_sensor::BinarySensor *binary_sensor) override;
#endif
#ifdef USE_COVER
  bool on_cover(cover::Cover *cover) override;
#endif
#ifdef USE_FAN
  bool on_fan(fan::Fan *fan) override;
#endif
#ifdef USE_LIGHT
  bool on_light(light::LightState *light) override;
#endif
#ifdef USE_SENSOR
  bool on_sensor(sensor::Sensor *sensor) override;
#endif
#ifdef USE_SWITCH
  bool on_switch(switch_::Switch *a_switch) override;
#endif
#ifdef USE_BUTTON
  bool on_button(button::Button *button) override;
#endif
#ifdef USE_TEXT_SENSOR
  bool on_text_sensor(text_sensor::TextSensor *text_sensor) override;
#endif
  bool on_service(UserServiceDescriptor *service) override;
#ifdef USE_ESP32_CAMERA
  bool on_camera(esp32_camera::ESP32Camera *camera) override;
#endif
#ifdef USE_CLIMATE
  bool on_climate(climate::Climate *climate) override;
#endif
#ifdef USE_NUMBER
  bool on_number(number::Number *number) override;
#endif
#ifdef USE_DATETIME_DATE
  bool on_date(datetime::DateEntity *date) override;
#endif
#ifdef USE_DATETIME_TIME
  bool on_time(datetime::TimeEntity *time) override;
#endif
#ifdef USE_TEXT
  bool on_text(text::Text *text) override;
#endif
#ifdef USE_SELECT
  bool on_select(select::Select *select) override;
#endif
#ifdef USE_LOCK
  bool on_lock(lock::Lock *a_lock) override;
#endif
#ifdef USE_VALVE
  bool on_valve(valve::Valve *valve) override;
#endif
#ifdef USE_MEDIA_PLAYER
  bool on_media_player(media_player::MediaPlayer *media_player) override;
#endif
#ifdef USE_ALARM_CONTROL_PANEL
  bool on_alarm_control_panel(alarm_control_panel::AlarmControlPanel *a_alarm_control_panel) override;
#endif
  bool on_end() override;

 protected:
  APIConnection *client_;
};

}  // namespace api
}  // namespace esphome
