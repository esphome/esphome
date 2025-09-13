#pragma once

#include "esphome/core/defines.h"
#ifdef USE_WEBSERVER
#include "esphome/core/component.h"
#include "esphome/core/component_iterator.h"
namespace esphome {
#ifdef USE_ESP_IDF
namespace web_server_idf {
class AsyncEventSource;
}
#endif
namespace web_server {

#ifdef USE_ARDUINO
class DeferredUpdateEventSource;
#endif
class WebServer;

class ListEntitiesIterator : public ComponentIterator {
 public:
#ifdef USE_ARDUINO
  ListEntitiesIterator(const WebServer *ws, DeferredUpdateEventSource *es);
#endif
#ifdef USE_ESP_IDF
  ListEntitiesIterator(const WebServer *ws, esphome::web_server_idf::AsyncEventSource *es);
#endif
  virtual ~ListEntitiesIterator();
#ifdef USE_BINARY_SENSOR
  bool on_binary_sensor(binary_sensor::BinarySensor *obj) override;
#endif
#ifdef USE_COVER
  bool on_cover(cover::Cover *obj) override;
#endif
#ifdef USE_FAN
  bool on_fan(fan::Fan *obj) override;
#endif
#ifdef USE_LIGHT
  bool on_light(light::LightState *obj) override;
#endif
#ifdef USE_SENSOR
  bool on_sensor(sensor::Sensor *obj) override;
#endif
#ifdef USE_SWITCH
  bool on_switch(switch_::Switch *obj) override;
#endif
#ifdef USE_BUTTON
  bool on_button(button::Button *obj) override;
#endif
#ifdef USE_TEXT_SENSOR
  bool on_text_sensor(text_sensor::TextSensor *obj) override;
#endif
#ifdef USE_CLIMATE
  bool on_climate(climate::Climate *obj) override;
#endif
#ifdef USE_NUMBER
  bool on_number(number::Number *obj) override;
#endif
#ifdef USE_DATETIME_DATE
  bool on_date(datetime::DateEntity *obj) override;
#endif
#ifdef USE_DATETIME_TIME
  bool on_time(datetime::TimeEntity *obj) override;
#endif
#ifdef USE_DATETIME_DATETIME
  bool on_datetime(datetime::DateTimeEntity *obj) override;
#endif
#ifdef USE_TEXT
  bool on_text(text::Text *obj) override;
#endif
#ifdef USE_SELECT
  bool on_select(select::Select *obj) override;
#endif
#ifdef USE_LOCK
  bool on_lock(lock::Lock *obj) override;
#endif
#ifdef USE_VALVE
  bool on_valve(valve::Valve *obj) override;
#endif
#ifdef USE_ALARM_CONTROL_PANEL
  bool on_alarm_control_panel(alarm_control_panel::AlarmControlPanel *obj) override;
#endif
#ifdef USE_EVENT
  bool on_event(event::Event *obj) override;
#endif
#ifdef USE_UPDATE
  bool on_update(update::UpdateEntity *obj) override;
#endif
  bool completed() { return this->state_ == IteratorState::NONE; }

 protected:
  const WebServer *web_server_;
#ifdef USE_ARDUINO
  DeferredUpdateEventSource *events_;
#endif
#ifdef USE_ESP_IDF
  esphome::web_server_idf::AsyncEventSource *events_;
#endif
};

}  // namespace web_server
}  // namespace esphome
#endif
