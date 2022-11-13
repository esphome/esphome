#include "subscribe_state.h"
#include "api_connection.h"
#include "esphome/core/log.h"

namespace esphome {
namespace api {

#ifdef USE_BINARY_SENSOR
bool InitialStateIterator::on_binary_sensor(binary_sensor::BinarySensor *binary_sensor) {
  return this->client_->send_binary_sensor_state(binary_sensor, binary_sensor->state);
}
#endif
#ifdef USE_COVER
bool InitialStateIterator::on_cover(cover::Cover *cover) { return this->client_->send_cover_state(cover); }
#endif
#ifdef USE_FAN
bool InitialStateIterator::on_fan(fan::Fan *fan) { return this->client_->send_fan_state(fan); }
#endif
#ifdef USE_LIGHT
bool InitialStateIterator::on_light(light::LightState *light) { return this->client_->send_light_state(light); }
#endif
#ifdef USE_SENSOR
bool InitialStateIterator::on_sensor(sensor::Sensor *sensor) {
  return this->client_->send_sensor_state(sensor, sensor->state);
}
#endif
#ifdef USE_SWITCH
bool InitialStateIterator::on_switch(switch_::Switch *a_switch) {
  return this->client_->send_switch_state(a_switch, a_switch->state);
}
#endif
#ifdef USE_TEXT_SENSOR
bool InitialStateIterator::on_text_sensor(text_sensor::TextSensor *text_sensor) {
  return this->client_->send_text_sensor_state(text_sensor, text_sensor->state);
}
#endif
#ifdef USE_CLIMATE
bool InitialStateIterator::on_climate(climate::Climate *climate) { return this->client_->send_climate_state(climate); }
#endif
#ifdef USE_NUMBER
bool InitialStateIterator::on_number(number::Number *number) {
  return this->client_->send_number_state(number, number->state);
}
#endif
#ifdef USE_SELECT
bool InitialStateIterator::on_select(select::Select *select) {
  return this->client_->send_select_state(select, select->state);
}
#endif
#ifdef USE_LOCK
bool InitialStateIterator::on_lock(lock::Lock *a_lock) { return this->client_->send_lock_state(a_lock, a_lock->state); }
#endif
#ifdef USE_MEDIA_PLAYER
bool InitialStateIterator::on_media_player(media_player::MediaPlayer *media_player) {
  return this->client_->send_media_player_state(media_player);
}
#endif
InitialStateIterator::InitialStateIterator(APIConnection *client) : client_(client) {}

}  // namespace api
}  // namespace esphome
