#include "subscribe_state.h"
#include "api_connection.h"
#include "esphome/core/log.h"

namespace esphome {
namespace api {

InitialStateIterator::InitialStateIterator(APIConnection *client) : client_(client) {
#ifdef USE_BINARY_SENSOR
  on_entity_callback([this](binary_sensor::BinarySensor *binary_sensor) {
    return this->client_->send_binary_sensor_state(binary_sensor, binary_sensor->state);
  });
#endif
#ifdef USE_COVER
  on_entity_callback([this](cover::Cover *cover) { return this->client_->send_cover_state(cover); });
#endif
#ifdef USE_FAN
  on_entity_callback([this](fan::Fan *fan) { return this->client_->send_fan_state(fan); });
#endif
#ifdef USE_LIGHT
  on_entity_callback([this](light::LightState *light) { return this->client_->send_light_state(light); });
#endif
#ifdef USE_SENSOR
  on_entity_callback(
      [this](sensor::Sensor *sensor) { return this->client_->send_sensor_state(sensor, sensor->state); });
#endif
#ifdef USE_SWITCH
  on_entity_callback(
      [this](switch_::Switch *a_switch) { return this->client_->send_switch_state(a_switch, a_switch->state); });
#endif
#ifdef USE_TEXT_SENSOR
  on_entity_callback([this](text_sensor::TextSensor *text_sensor) {
    return this->client_->send_text_sensor_state(text_sensor, text_sensor->state);
  });
#endif
#ifdef USE_CLIMATE
  on_entity_callback([this](climate::Climate *climate) { return this->client_->send_climate_state(climate); });
#endif
#ifdef USE_NUMBER
  on_entity_callback(
      [this](number::Number *number) { return this->client_->send_number_state(number, number->state); });
#endif
#ifdef USE_SELECT
  on_entity_callback(
      [this](select::Select *select) { return this->client_->send_select_state(select, select->state); });
#endif
#ifdef USE_LOCK
  on_entity_callback([this](lock::Lock *a_lock) { return this->client_->send_lock_state(a_lock, a_lock->state); });
#endif
#ifdef USE_MEDIA_PLAYER
  on_entity_callback(
      [this](media_player::MediaPlayer *media_player) { return this->client_->send_media_player_state(media_player); });
#endif
#ifdef USE_ALARM_CONTROL_PANEL
  on_entity_callback([this](alarm_control_panel::AlarmControlPanel *a_alarm_control_panel) {
    return this->client_->send_alarm_control_panel_state(a_alarm_control_panel);
  });
#endif
}

}  // namespace api
}  // namespace esphome
