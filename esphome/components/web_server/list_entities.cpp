#include "list_entities.h"
#ifdef USE_WEBSERVER
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"

#include "web_server.h"

namespace esphome {
namespace web_server {

ListEntitiesIterator::ListEntitiesIterator(WebServer *web_server) : web_server_(web_server) {}

#ifdef USE_BINARY_SENSOR
bool ListEntitiesIterator::on_binary_sensor(binary_sensor::BinarySensor *binary_sensor) {
  if (this->web_server_->events_.count() == 0)
    return true;
  this->web_server_->events_.send(
      this->web_server_->binary_sensor_json(binary_sensor, binary_sensor->state, DETAIL_ALL).c_str(), "state");
  return true;
}
#endif
#ifdef USE_COVER
bool ListEntitiesIterator::on_cover(cover::Cover *cover) {
  if (this->web_server_->events_.count() == 0)
    return true;
  this->web_server_->events_.send(this->web_server_->cover_json(cover, DETAIL_ALL).c_str(), "state");
  return true;
}
#endif
#ifdef USE_FAN
bool ListEntitiesIterator::on_fan(fan::Fan *fan) {
  if (this->web_server_->events_.count() == 0)
    return true;
  this->web_server_->events_.send(this->web_server_->fan_json(fan, DETAIL_ALL).c_str(), "state");
  return true;
}
#endif
#ifdef USE_LIGHT
bool ListEntitiesIterator::on_light(light::LightState *light) {
  if (this->web_server_->events_.count() == 0)
    return true;
  this->web_server_->events_.send(this->web_server_->light_json(light, DETAIL_ALL).c_str(), "state");
  return true;
}
#endif
#ifdef USE_SENSOR
bool ListEntitiesIterator::on_sensor(sensor::Sensor *sensor) {
  if (this->web_server_->events_.count() == 0)
    return true;
  this->web_server_->events_.send(this->web_server_->sensor_json(sensor, sensor->state, DETAIL_ALL).c_str(), "state");
  return true;
}
#endif
#ifdef USE_SWITCH
bool ListEntitiesIterator::on_switch(switch_::Switch *a_switch) {
  if (this->web_server_->events_.count() == 0)
    return true;
  this->web_server_->events_.send(this->web_server_->switch_json(a_switch, a_switch->state, DETAIL_ALL).c_str(),
                                  "state");
  return true;
}
#endif
#ifdef USE_BUTTON
bool ListEntitiesIterator::on_button(button::Button *button) {
  if (this->web_server_->events_.count() == 0)
    return true;
  this->web_server_->events_.send(this->web_server_->button_json(button, DETAIL_ALL).c_str(), "state");
  return true;
}
#endif
#ifdef USE_TEXT_SENSOR
bool ListEntitiesIterator::on_text_sensor(text_sensor::TextSensor *text_sensor) {
  if (this->web_server_->events_.count() == 0)
    return true;
  this->web_server_->events_.send(
      this->web_server_->text_sensor_json(text_sensor, text_sensor->state, DETAIL_ALL).c_str(), "state");
  return true;
}
#endif
#ifdef USE_LOCK
bool ListEntitiesIterator::on_lock(lock::Lock *a_lock) {
  if (this->web_server_->events_.count() == 0)
    return true;
  this->web_server_->events_.send(this->web_server_->lock_json(a_lock, a_lock->state, DETAIL_ALL).c_str(), "state");
  return true;
}
#endif

#ifdef USE_VALVE
bool ListEntitiesIterator::on_valve(valve::Valve *valve) {
  if (this->web_server_->events_.count() == 0)
    return true;
  this->web_server_->events_.send(this->web_server_->valve_json(valve, DETAIL_ALL).c_str(), "state");
  return true;
}
#endif

#ifdef USE_CLIMATE
bool ListEntitiesIterator::on_climate(climate::Climate *climate) {
  if (this->web_server_->events_.count() == 0)
    return true;
  this->web_server_->events_.send(this->web_server_->climate_json(climate, DETAIL_ALL).c_str(), "state");
  return true;
}
#endif

#ifdef USE_NUMBER
bool ListEntitiesIterator::on_number(number::Number *number) {
  if (this->web_server_->events_.count() == 0)
    return true;
  this->web_server_->events_.send(this->web_server_->number_json(number, number->state, DETAIL_ALL).c_str(), "state");
  return true;
}
#endif

#ifdef USE_DATETIME_DATE
bool ListEntitiesIterator::on_date(datetime::DateEntity *date) {
  if (this->web_server_->events_.count() == 0)
    return true;
  this->web_server_->events_.send(this->web_server_->date_json(date, DETAIL_ALL).c_str(), "state");
  return true;
}
#endif

#ifdef USE_DATETIME_TIME
bool ListEntitiesIterator::on_time(datetime::TimeEntity *time) {
  this->web_server_->events_.send(this->web_server_->time_json(time, DETAIL_ALL).c_str(), "state");
  return true;
}
#endif

#ifdef USE_DATETIME_DATETIME
bool ListEntitiesIterator::on_datetime(datetime::DateTimeEntity *datetime) {
  if (this->web_server_->events_.count() == 0)
    return true;
  this->web_server_->events_.send(this->web_server_->datetime_json(datetime, DETAIL_ALL).c_str(), "state");
  return true;
}
#endif

#ifdef USE_TEXT
bool ListEntitiesIterator::on_text(text::Text *text) {
  if (this->web_server_->events_.count() == 0)
    return true;
  this->web_server_->events_.send(this->web_server_->text_json(text, text->state, DETAIL_ALL).c_str(), "state");
  return true;
}
#endif

#ifdef USE_SELECT
bool ListEntitiesIterator::on_select(select::Select *select) {
  if (this->web_server_->events_.count() == 0)
    return true;
  this->web_server_->events_.send(this->web_server_->select_json(select, select->state, DETAIL_ALL).c_str(), "state");
  return true;
}
#endif

#ifdef USE_ALARM_CONTROL_PANEL
bool ListEntitiesIterator::on_alarm_control_panel(alarm_control_panel::AlarmControlPanel *a_alarm_control_panel) {
  if (this->web_server_->events_.count() == 0)
    return true;
  this->web_server_->events_.send(
      this->web_server_->alarm_control_panel_json(a_alarm_control_panel, a_alarm_control_panel->get_state(), DETAIL_ALL)
          .c_str(),
      "state");
  return true;
}
#endif

#ifdef USE_EVENT
bool ListEntitiesIterator::on_event(event::Event *event) {
  // Null event type, since we are just iterating over entities
  const std::string null_event_type = "";
  this->web_server_->events_.send(this->web_server_->event_json(event, null_event_type, DETAIL_ALL).c_str(), "state");
  return true;
}
#endif

#ifdef USE_UPDATE
bool ListEntitiesIterator::on_update(update::UpdateEntity *update) {
  if (this->web_server_->events_.count() == 0)
    return true;
  this->web_server_->events_.send(this->web_server_->update_json(update, DETAIL_ALL).c_str(), "state");
  return true;
}
#endif

}  // namespace web_server
}  // namespace esphome
#endif
