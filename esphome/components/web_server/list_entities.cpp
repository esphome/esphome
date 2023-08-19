#include "list_entities.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"
#include "web_server.h"

namespace esphome {
namespace web_server {

ListEntitiesIterator::ListEntitiesIterator(WebServer *web_server) : web_server_(web_server) {
#ifdef USE_BINARY_SENSOR
  on_entity_callback([this](binary_sensor::BinarySensor *binary_sensor) {
    this->web_server_->events_.send(
        this->web_server_->binary_sensor_json(binary_sensor, binary_sensor->state, DETAIL_ALL).c_str(), "state");
    return true;
  });
#endif
#ifdef USE_COVER
  on_entity_callback([this](cover::Cover *cover) {
    this->web_server_->events_.send(this->web_server_->cover_json(cover, DETAIL_ALL).c_str(), "state");
    return true;
  });
#endif
#ifdef USE_FAN
  on_entity_callback([this](fan::Fan *fan) {
    this->web_server_->events_.send(this->web_server_->fan_json(fan, DETAIL_ALL).c_str(), "state");
    return true;
  });
#endif
#ifdef USE_LIGHT
  on_entity_callback([this](light::LightState *light) {
    this->web_server_->events_.send(this->web_server_->light_json(light, DETAIL_ALL).c_str(), "state");
    return true;
  });
#endif
#ifdef USE_SENSOR
  on_entity_callback([this](sensor::Sensor *sensor) {
    this->web_server_->events_.send(this->web_server_->sensor_json(sensor, sensor->state, DETAIL_ALL).c_str(), "state");
    return true;
  });
#endif
#ifdef USE_SWITCH
  on_entity_callback([this](switch_::Switch *a_switch) {
    this->web_server_->events_.send(this->web_server_->switch_json(a_switch, a_switch->state, DETAIL_ALL).c_str(),
                                    "state");
    return true;
  });
#endif
#ifdef USE_BUTTON
  on_entity_callback([this](button::Button *button) {
    this->web_server_->events_.send(this->web_server_->button_json(button, DETAIL_ALL).c_str(), "state");
    return true;
  });
#endif
#ifdef USE_TEXT_SENSOR
  on_entity_callback([this](text_sensor::TextSensor *text_sensor) {
    this->web_server_->events_.send(
        this->web_server_->text_sensor_json(text_sensor, text_sensor->state, DETAIL_ALL).c_str(), "state");
    return true;
  });
#endif
#ifdef USE_LOCK
  on_entity_callback([this](lock::Lock *a_lock) {
    this->web_server_->events_.send(this->web_server_->lock_json(a_lock, a_lock->state, DETAIL_ALL).c_str(), "state");
    return true;
  });
#endif
#ifdef USE_CLIMATE
  on_entity_callback([this](climate::Climate *climate) {
    this->web_server_->events_.send(this->web_server_->climate_json(climate, DETAIL_ALL).c_str(), "state");
    return true;
  });
#endif
#ifdef USE_NUMBER
  on_entity_callback([this](number::Number *number) {
    this->web_server_->events_.send(this->web_server_->number_json(number, number->state, DETAIL_ALL).c_str(), "state");
    return true;
  });
#endif
#ifdef USE_SELECT
  on_entity_callback([this](select::Select *select) {
    this->web_server_->events_.send(this->web_server_->select_json(select, select->state, DETAIL_ALL).c_str(), "state");
    return true;
  });
#endif
#ifdef USE_ALARM_CONTROL_PANEL
  on_entity_callback([this](alarm_control_panel::AlarmControlPanel *a_alarm_control_panel) {
    this->web_server_->events_.send(
        this->web_server_
            ->alarm_control_panel_json(a_alarm_control_panel, a_alarm_control_panel->get_state(), DETAIL_ALL)
            .c_str(),
        "state");
    return true;
  });
#endif
}
}  // namespace web_server
}  // namespace esphome
