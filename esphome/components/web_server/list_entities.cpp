#include "list_entities.h"
#ifdef USE_WEBSERVER
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"

#include "web_server.h"

namespace esphome {
namespace web_server {

#ifdef USE_ARDUINO
ListEntitiesIterator::ListEntitiesIterator(const WebServer *ws, DeferredUpdateEventSource *es)
    : web_server_(ws), events_(es) {}
#endif
#ifdef USE_ESP_IDF
ListEntitiesIterator::ListEntitiesIterator(const WebServer *ws, AsyncEventSource *es) : web_server_(ws), events_(es) {}
#endif
ListEntitiesIterator::~ListEntitiesIterator() {}

#ifdef USE_BINARY_SENSOR
bool ListEntitiesIterator::on_binary_sensor(binary_sensor::BinarySensor *obj) {
  if (this->events_->count() == 0)
    return true;
  this->events_->deferrable_send_state(obj, "state_detail_all", WebServer::binary_sensor_all_json_generator);
  return true;
}
#endif
#ifdef USE_COVER
bool ListEntitiesIterator::on_cover(cover::Cover *obj) {
  if (this->events_->count() == 0)
    return true;
  this->events_->deferrable_send_state(obj, "state_detail_all", WebServer::cover_all_json_generator);
  return true;
}
#endif
#ifdef USE_FAN
bool ListEntitiesIterator::on_fan(fan::Fan *obj) {
  if (this->events_->count() == 0)
    return true;
  this->events_->deferrable_send_state(obj, "state_detail_all", WebServer::fan_all_json_generator);
  return true;
}
#endif
#ifdef USE_LIGHT
bool ListEntitiesIterator::on_light(light::LightState *obj) {
  if (this->events_->count() == 0)
    return true;
  this->events_->deferrable_send_state(obj, "state_detail_all", WebServer::light_all_json_generator);
  return true;
}
#endif
#ifdef USE_SENSOR
bool ListEntitiesIterator::on_sensor(sensor::Sensor *obj) {
  if (this->events_->count() == 0)
    return true;
  this->events_->deferrable_send_state(obj, "state_detail_all", WebServer::sensor_all_json_generator);
  return true;
}
#endif
#ifdef USE_SWITCH
bool ListEntitiesIterator::on_switch(switch_::Switch *obj) {
  if (this->events_->count() == 0)
    return true;
  this->events_->deferrable_send_state(obj, "state_detail_all", WebServer::switch_all_json_generator);
  return true;
}
#endif
#ifdef USE_BUTTON
bool ListEntitiesIterator::on_button(button::Button *obj) {
  if (this->events_->count() == 0)
    return true;
  this->events_->deferrable_send_state(obj, "state_detail_all", WebServer::button_all_json_generator);
  return true;
}
#endif
#ifdef USE_TEXT_SENSOR
bool ListEntitiesIterator::on_text_sensor(text_sensor::TextSensor *obj) {
  if (this->events_->count() == 0)
    return true;
  this->events_->deferrable_send_state(obj, "state_detail_all", WebServer::text_sensor_all_json_generator);
  return true;
}
#endif
#ifdef USE_LOCK
bool ListEntitiesIterator::on_lock(lock::Lock *obj) {
  if (this->events_->count() == 0)
    return true;
  this->events_->deferrable_send_state(obj, "state_detail_all", WebServer::lock_all_json_generator);
  return true;
}
#endif

#ifdef USE_VALVE
bool ListEntitiesIterator::on_valve(valve::Valve *obj) {
  if (this->events_->count() == 0)
    return true;
  this->events_->deferrable_send_state(obj, "state_detail_all", WebServer::valve_all_json_generator);
  return true;
}
#endif

#ifdef USE_CLIMATE
bool ListEntitiesIterator::on_climate(climate::Climate *obj) {
  if (this->events_->count() == 0)
    return true;
  this->events_->deferrable_send_state(obj, "state_detail_all", WebServer::climate_all_json_generator);
  return true;
}
#endif

#ifdef USE_NUMBER
bool ListEntitiesIterator::on_number(number::Number *obj) {
  if (this->events_->count() == 0)
    return true;
  this->events_->deferrable_send_state(obj, "state_detail_all", WebServer::number_all_json_generator);
  return true;
}
#endif

#ifdef USE_DATETIME_DATE
bool ListEntitiesIterator::on_date(datetime::DateEntity *obj) {
  if (this->events_->count() == 0)
    return true;
  this->events_->deferrable_send_state(obj, "state_detail_all", WebServer::date_all_json_generator);
  return true;
}
#endif

#ifdef USE_DATETIME_TIME
bool ListEntitiesIterator::on_time(datetime::TimeEntity *obj) {
  if (this->events_->count() == 0)
    return true;
  this->events_->deferrable_send_state(obj, "state_detail_all", WebServer::time_all_json_generator);
  return true;
}
#endif

#ifdef USE_DATETIME_DATETIME
bool ListEntitiesIterator::on_datetime(datetime::DateTimeEntity *obj) {
  if (this->events_->count() == 0)
    return true;
  this->events_->deferrable_send_state(obj, "state_detail_all", WebServer::datetime_all_json_generator);
  return true;
}
#endif

#ifdef USE_TEXT
bool ListEntitiesIterator::on_text(text::Text *obj) {
  if (this->events_->count() == 0)
    return true;
  this->events_->deferrable_send_state(obj, "state_detail_all", WebServer::text_all_json_generator);
  return true;
}
#endif

#ifdef USE_SELECT
bool ListEntitiesIterator::on_select(select::Select *obj) {
  if (this->events_->count() == 0)
    return true;
  this->events_->deferrable_send_state(obj, "state_detail_all", WebServer::select_all_json_generator);
  return true;
}
#endif

#ifdef USE_ALARM_CONTROL_PANEL
bool ListEntitiesIterator::on_alarm_control_panel(alarm_control_panel::AlarmControlPanel *obj) {
  if (this->events_->count() == 0)
    return true;
  this->events_->deferrable_send_state(obj, "state_detail_all", WebServer::alarm_control_panel_all_json_generator);
  return true;
}
#endif

#ifdef USE_EVENT
bool ListEntitiesIterator::on_event(event::Event *obj) {
  if (this->events_->count() == 0)
    return true;
  // Null event type, since we are just iterating over entities
  this->events_->deferrable_send_state(obj, "state_detail_all", WebServer::event_all_json_generator);
  return true;
}
#endif

#ifdef USE_UPDATE
bool ListEntitiesIterator::on_update(update::UpdateEntity *obj) {
  if (this->events_->count() == 0)
    return true;
  this->events_->deferrable_send_state(obj, "state_detail_all", WebServer::update_all_json_generator);
  return true;
}
#endif

}  // namespace web_server
}  // namespace esphome
#endif
