#include "list_entities.h"
#include "esphome/core/util.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "api_connection.h"

namespace esphome {
namespace api {

bool ListEntitiesIterator::on_end() { return this->client_->send_list_info_done(); }

ListEntitiesIterator::ListEntitiesIterator(APIConnection *client) : client_(client) {
#ifdef USE_BINARY_SENSOR
  on_entity_callback([this](binary_sensor::BinarySensor *binary_sensor) {
    return this->client_->send_binary_sensor_info(binary_sensor);
  });
#endif
#ifdef USE_COVER
  on_entity_callback([this](cover::Cover *cover) { return this->client_->send_cover_info(cover); });
#endif
#ifdef USE_FAN
  on_entity_callback([this](fan::Fan *fan) { return this->client_->send_fan_info(fan); });
#endif
#ifdef USE_LIGHT
  on_entity_callback([this](light::LightState *light) { return this->client_->send_light_info(light); });
#endif
#ifdef USE_SENSOR
  on_entity_callback([this](sensor::Sensor *sensor) { return this->client_->send_sensor_info(sensor); });
#endif
#ifdef USE_SWITCH
  on_entity_callback([this](switch_::Switch *a_switch) { return this->client_->send_switch_info(a_switch); });
#endif
#ifdef USE_BUTTON
  on_entity_callback([this](button::Button *button) { return this->client_->send_button_info(button); });
#endif
#ifdef USE_TEXT_SENSOR
  on_entity_callback(
      [this](text_sensor::TextSensor *text_sensor) { return this->client_->send_text_sensor_info(text_sensor); });
#endif
#ifdef USE_LOCK
  on_entity_callback([this](lock::Lock *a_lock) { return this->client_->send_lock_info(a_lock); });
#endif
#ifdef USE_CLIMATE
  on_entity_callback([this](climate::Climate *climate) { return this->client_->send_climate_info(climate); });
#endif

#ifdef USE_NUMBER
  on_entity_callback([this](number::Number *number) { return this->client_->send_number_info(number); });
#endif

#ifdef USE_SELECT
  on_entity_callback([this](select::Select *select) { return this->client_->send_select_info(select); });
#endif

#ifdef USE_MEDIA_PLAYER
  on_entity_callback(
      [this](media_player::MediaPlayer *media_player) { return this->client_->send_media_player_info(media_player); });
#endif

#ifdef USE_ALARM_CONTROL_PANEL
  on_entity_callback([this](alarm_control_panel::AlarmControlPanel *a_alarm_control_panel) {
    return this->client_->send_alarm_control_panel_info(a_alarm_control_panel);
  });
#endif
}

bool ListEntitiesIterator::on_service(UserServiceDescriptor *service) {
  auto resp = service->encode_list_service_response();
  return this->client_->send_list_entities_services_response(resp);
}

#ifdef USE_ESP32_CAMERA
bool ListEntitiesIterator::on_camera(esp32_camera::ESP32Camera *camera) {
  return this->client_->send_camera_info(camera);
}
#endif

}  // namespace api
}  // namespace esphome
