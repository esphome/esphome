#include "list_entities.h"
#include "esphome/core/util.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "api_connection.h"

namespace esphome {
namespace api {

#ifdef USE_BINARY_SENSOR
bool ListEntitiesIterator::on_binary_sensor(binary_sensor::BinarySensor *binary_sensor) {
  return this->client_->send_binary_sensor_info(binary_sensor);
}
#endif
#ifdef USE_COVER
bool ListEntitiesIterator::on_cover(cover::Cover *cover) { return this->client_->send_cover_info(cover); }
#endif
#ifdef USE_FAN
bool ListEntitiesIterator::on_fan(fan::Fan *fan) { return this->client_->send_fan_info(fan); }
#endif
#ifdef USE_LIGHT
bool ListEntitiesIterator::on_light(light::LightState *light) { return this->client_->send_light_info(light); }
#endif
#ifdef USE_SENSOR
bool ListEntitiesIterator::on_sensor(sensor::Sensor *sensor) { return this->client_->send_sensor_info(sensor); }
#endif
#ifdef USE_SWITCH
bool ListEntitiesIterator::on_switch(switch_::Switch *a_switch) { return this->client_->send_switch_info(a_switch); }
#endif
#ifdef USE_BUTTON
bool ListEntitiesIterator::on_button(button::Button *button) { return this->client_->send_button_info(button); }
#endif
#ifdef USE_TEXT_SENSOR
bool ListEntitiesIterator::on_text_sensor(text_sensor::TextSensor *text_sensor) {
  return this->client_->send_text_sensor_info(text_sensor);
}
#endif
#ifdef USE_LOCK
bool ListEntitiesIterator::on_lock(lock::Lock *a_lock) { return this->client_->send_lock_info(a_lock); }
#endif

bool ListEntitiesIterator::on_end() { return this->client_->send_list_info_done(); }
ListEntitiesIterator::ListEntitiesIterator(APIConnection *client) : client_(client) {}
bool ListEntitiesIterator::on_service(UserServiceDescriptor *service) {
  auto resp = service->encode_list_service_response();
  return this->client_->send_list_entities_services_response(resp);
}

#ifdef USE_ESP32_CAMERA
bool ListEntitiesIterator::on_camera(esp32_camera::ESP32Camera *camera) {
  return this->client_->send_camera_info(camera);
}
#endif

#ifdef USE_CLIMATE
bool ListEntitiesIterator::on_climate(climate::Climate *climate) { return this->client_->send_climate_info(climate); }
#endif

#ifdef USE_NUMBER
bool ListEntitiesIterator::on_number(number::Number *number) { return this->client_->send_number_info(number); }
#endif

#ifdef USE_TEXT
bool ListEntitiesIterator::on_text(text::Text *text) { return this->client_->send_text_info(text); }
#endif

#ifdef USE_SELECT
bool ListEntitiesIterator::on_select(select::Select *select) { return this->client_->send_select_info(select); }
#endif

#ifdef USE_MEDIA_PLAYER
bool ListEntitiesIterator::on_media_player(media_player::MediaPlayer *media_player) {
  return this->client_->send_media_player_info(media_player);
}
#endif
#ifdef USE_ALARM_CONTROL_PANEL
bool ListEntitiesIterator::on_alarm_control_panel(alarm_control_panel::AlarmControlPanel *a_alarm_control_panel) {
  return this->client_->send_alarm_control_panel_info(a_alarm_control_panel);
}
#endif

}  // namespace api
}  // namespace esphome
