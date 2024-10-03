#include "list_entities.h"
#ifdef USE_WEBSERVER
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"

#include "web_server.h"

namespace esphome {
namespace web_server {

ListEntitiesIterator::ListEntitiesIterator(WebServer *ws, DeferredUpdateEventSource *es) : web_server_(ws), es_(es) {}

#ifdef USE_BINARY_SENSOR
bool ListEntitiesIterator::on_binary_sensor(binary_sensor::BinarySensor *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [](WebServer* web_server, void* source) { return web_server->binary_sensor_json((binary_sensor::BinarySensor*)(source), ((binary_sensor::BinarySensor*)(source))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif
#ifdef USE_COVER
bool ListEntitiesIterator::on_cover(cover::Cover *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [](WebServer* web_server, void* source) { return web_server->cover_json((cover::Cover*)(source), ((cover::Cover*)(source))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif
#ifdef USE_FAN
bool ListEntitiesIterator::on_fan(fan::Fan *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [](WebServer* web_server, void* source) { return web_server->fan_json((fan::Fan*)(source), ((fan::Fan*)(source))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif
#ifdef USE_LIGHT
bool ListEntitiesIterator::on_light(light::LightState *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [](WebServer* web_server, void* source) { return web_server->light_json((light::LightState*)(source), ((light::LightState*)(source))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif
#ifdef USE_SENSOR
bool ListEntitiesIterator::on_sensor(sensor::Sensor *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [](WebServer* web_server, void* source) { return web_server->sensor_json((sensor::Sensor*)(source), ((sensor::Sensor*)(source))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif
#ifdef USE_SWITCH
bool ListEntitiesIterator::on_switch(switch_::Switch *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [](WebServer* web_server, void* source) { return web_server->switch_json((switch_::Switch*)(source), ((switch_::Switch*)(source))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif
#ifdef USE_BUTTON
bool ListEntitiesIterator::on_button(button::Button *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [](WebServer* web_server, void* source) { return web_server->button_json((button::Button*)(source), DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif
#ifdef USE_TEXT_SENSOR
bool ListEntitiesIterator::on_text_sensor(text_sensor::TextSensor *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [](WebServer* web_server, void* source) { return web_server->text_sensor_json((text_sensor::TextSensor*)(source), ((text_sensor::TextSensor*)(source))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif
#ifdef USE_LOCK
bool ListEntitiesIterator::on_lock(lock::Lock *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [](WebServer* web_server, void* source) { return web_server->lock_json((lock::Lock*)(source), ((lock::Lock*)(source))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif

#ifdef USE_VALVE
bool ListEntitiesIterator::on_valve(valve::Valve *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [](WebServer* web_server, void* source) { return web_server->valve_json((valve::Valve*)(source), ((valve::Valve*)(source))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif

#ifdef USE_CLIMATE
bool ListEntitiesIterator::on_climate(climate::Climate *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [](WebServer* web_server, void* source) { return web_server->climate_json((climate::Climate*)(source), ((climate::Climate*)(source))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif

#ifdef USE_NUMBER
bool ListEntitiesIterator::on_number(number::Number *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [](WebServer* web_server, void* source) { return web_server->number_json((number::Number*)(source), ((number::Number*)(source))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif

#ifdef USE_DATETIME_DATE
bool ListEntitiesIterator::on_date(datetime::DateEntity *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [](WebServer* web_server, void* source) { return web_server->date_json((datetime::DateEntity*)(source), ((datetime::DateEntity*)(source))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif

#ifdef USE_DATETIME_TIME
bool ListEntitiesIterator::on_time(datetime::TimeEntity *obj) {
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [](WebServer* web_server, void* source) { return web_server->time_json((datetime::TimeEntity*)(source), ((datetime::TimeEntity*)(source))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif

#ifdef USE_DATETIME_DATETIME
bool ListEntitiesIterator::on_datetime(datetime::DateTimeEntity *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [](WebServer* web_server, void* source) { return web_server->datetime_json((datetime::DateTimeEntity*)(source), DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif

#ifdef USE_TEXT
bool ListEntitiesIterator::on_text(text::Text *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [](WebServer* web_server, void* source) { return web_server->text_json((text::Text*)(source), ((text::Text*)(source))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif

#ifdef USE_SELECT
bool ListEntitiesIterator::on_select(select::Select *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [](WebServer* web_server, void* source) { return web_server->select_json((select::Select*)(source), ((select::Select*)(source))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif

#ifdef USE_ALARM_CONTROL_PANEL
bool ListEntitiesIterator::on_alarm_control_panel(alarm_control_panel::AlarmControlPanel *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [](WebServer* web_server, void* source) { return web_server->alarm_control_panel_json((alarm_control_panel::AlarmControlPanel*)(source), ((alarm_control_panel::AlarmControlPanel*)(source))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif

#ifdef USE_EVENT
bool ListEntitiesIterator::on_event(event::Event *obj) {
  // Null event type, since we are just iterating over entities
  const std::string null_event_type = "";
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [](WebServer* web_server, void* source) { return web_server->event_json((event::Event*)(source), ((event::Event*)(source))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif

#ifdef USE_UPDATE
bool ListEntitiesIterator::on_update(update::UpdateEntity *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [](WebServer* web_server, void* source) { return web_server->update_json((update::UpdateEntity*)(source), ((update::UpdateEntity*)(source))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif

}  // namespace web_server
}  // namespace esphome
#endif
