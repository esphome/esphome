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
  event->message_generator_ = [this, event]() { return this->web_server_->binary_sensor_json((binary_sensor::BinarySensor*)(event->source_), ((binary_sensor::BinarySensor*)(event->source_))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif
#ifdef USE_COVER
bool ListEntitiesIterator::on_cover(cover::Cover *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [this, event]() { return this->web_server_->cover_json((cover::Cover*)(event->source_), ((cover::Cover*)(event->source_))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif
#ifdef USE_FAN
bool ListEntitiesIterator::on_fan(fan::Fan *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [this, event]() { return this->web_server_->fan_json((fan::Fan*)(event->source_), ((fan::Fan*)(event->source_))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif
#ifdef USE_LIGHT
bool ListEntitiesIterator::on_light(light::LightState *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [this, event]() { return this->web_server_->light_json((light::LightState*)(event->source_), ((light::LightState*)(event->source_))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif
#ifdef USE_SENSOR
bool ListEntitiesIterator::on_sensor(sensor::Sensor *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [this, event]() { return this->web_server_->sensor_json((sensor::Sensor*)(event->source_), ((sensor::Sensor*)(event->source_))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif
#ifdef USE_SWITCH
bool ListEntitiesIterator::on_switch(switch_::Switch *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [this, event]() { return this->web_server_->switch_json((switch_::Switch*)(event->source_), ((switch_::Switch*)(event->source_))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif
#ifdef USE_BUTTON
bool ListEntitiesIterator::on_button(button::Button *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [this, event]() { return this->web_server_->button_json((button::Button*)(event->source_), DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif
#ifdef USE_TEXT_SENSOR
bool ListEntitiesIterator::on_text_sensor(text_sensor::TextSensor *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [this, event]() { return this->web_server_->text_sensor_json((text_sensor::TextSensor*)(event->source_), ((text_sensor::TextSensor*)(event->source_))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif
#ifdef USE_LOCK
bool ListEntitiesIterator::on_lock(lock::Lock *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [this, event]() { return this->web_server_->lock_json((lock::Lock*)(event->source_), ((lock::Lock*)(event->source_))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif

#ifdef USE_VALVE
bool ListEntitiesIterator::on_valve(valve::Valve *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [this, event]() { return this->web_server_->valve_json((valve::Valve*)(event->source_), ((valve::Valve*)(event->source_))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif

#ifdef USE_CLIMATE
bool ListEntitiesIterator::on_climate(climate::Climate *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [this, event]() { return this->web_server_->climate_json((climate::Climate*)(event->source_), ((climate::Climate*)(event->source_))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif

#ifdef USE_NUMBER
bool ListEntitiesIterator::on_number(number::Number *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [this, event]() { return this->web_server_->number_json((number::Number*)(event->source_), ((number::Number*)(event->source_))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif

#ifdef USE_DATETIME_DATE
bool ListEntitiesIterator::on_date(datetime::DateEntity *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [this, event]() { return this->web_server_->date_json((datetime::DateEntity*)(event->source_), ((datetime::DateEntity*)(event->source_))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif

#ifdef USE_DATETIME_TIME
bool ListEntitiesIterator::on_time(datetime::TimeEntity *obj) {
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [this, event]() { return this->web_server_->time_json((datetime::TimeEntity*)(event->source_), ((datetime::TimeEntity*)(event->source_))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif

#ifdef USE_DATETIME_DATETIME
bool ListEntitiesIterator::on_datetime(datetime::DateTimeEntity *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [this, event]() { return this->web_server_->datetime_json((datetime::DateTimeEntity*)(event->source_), DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif

#ifdef USE_TEXT
bool ListEntitiesIterator::on_text(text::Text *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [this, event]() { return this->web_server_->text_json((text::Text*)(event->source_), ((text::Text*)(event->source_))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif

#ifdef USE_SELECT
bool ListEntitiesIterator::on_select(select::Select *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [this, event]() { return this->web_server_->select_json((select::Select*)(event->source_), ((select::Select*)(event->source_))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif

#ifdef USE_ALARM_CONTROL_PANEL
bool ListEntitiesIterator::on_alarm_control_panel(alarm_control_panel::AlarmControlPanel *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [this, event]() { return this->web_server_->alarm_control_panel_json((alarm_control_panel::AlarmControlPanel*)(event->source_), ((alarm_control_panel::AlarmControlPanel*)(event->source_))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif

#ifdef USE_EVENT
bool ListEntitiesIterator::on_event(event::Event *obj) {
  // Null event type, since we are just iterating over entities
  const std::string null_event_type = "";
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [this, event]() { return this->web_server_->event_json((event::Event*)(event->source_), ((event::Event*)(event->source_))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif

#ifdef USE_UPDATE
bool ListEntitiesIterator::on_update(update::UpdateEntity *obj) {
  if (this->es_->count() == 0)
    return true;
  DeferredEvent* event = new DeferredEvent(obj, "state_detail_all");
  event->message_generator_ = [this, event]() { return this->web_server_->update_json((update::UpdateEntity*)(event->source_), ((update::UpdateEntity*)(event->source_))->state, DETAIL_ALL).c_str(); };
  this->es_->send(event);
  return true;
}
#endif

}  // namespace web_server
}  // namespace esphome
#endif
