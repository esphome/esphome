#include "list_entities.h"
#ifdef USE_API
#include "api_connection.h"
#include "api_pb2.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"
#ifdef USE_API_USER_DEFINED_ACTIONS
#include "user_services.h"
#endif

namespace esphome::api {

// Generate entity handler implementations using macros
#ifdef USE_BINARY_SENSOR
LIST_ENTITIES_HANDLER(binary_sensor, binary_sensor::BinarySensor, ListEntitiesBinarySensorResponse)
#endif
#ifdef USE_COVER
LIST_ENTITIES_HANDLER(cover, cover::Cover, ListEntitiesCoverResponse)
#endif
#ifdef USE_FAN
LIST_ENTITIES_HANDLER(fan, fan::Fan, ListEntitiesFanResponse)
#endif
#ifdef USE_LIGHT
LIST_ENTITIES_HANDLER(light, light::LightState, ListEntitiesLightResponse)
#endif
#ifdef USE_SENSOR
LIST_ENTITIES_HANDLER(sensor, sensor::Sensor, ListEntitiesSensorResponse)
#endif
#ifdef USE_SWITCH
LIST_ENTITIES_HANDLER(switch, switch_::Switch, ListEntitiesSwitchResponse)
#endif
#ifdef USE_BUTTON
LIST_ENTITIES_HANDLER(button, button::Button, ListEntitiesButtonResponse)
#endif
#ifdef USE_TEXT_SENSOR
LIST_ENTITIES_HANDLER(text_sensor, text_sensor::TextSensor, ListEntitiesTextSensorResponse)
#endif
#ifdef USE_LOCK
LIST_ENTITIES_HANDLER(lock, lock::Lock, ListEntitiesLockResponse)
#endif
#ifdef USE_VALVE
LIST_ENTITIES_HANDLER(valve, valve::Valve, ListEntitiesValveResponse)
#endif
#ifdef USE_CAMERA
LIST_ENTITIES_HANDLER(camera, camera::Camera, ListEntitiesCameraResponse)
#endif
#ifdef USE_CLIMATE
LIST_ENTITIES_HANDLER(climate, climate::Climate, ListEntitiesClimateResponse)
#endif
#ifdef USE_NUMBER
LIST_ENTITIES_HANDLER(number, number::Number, ListEntitiesNumberResponse)
#endif
#ifdef USE_DATETIME_DATE
LIST_ENTITIES_HANDLER(date, datetime::DateEntity, ListEntitiesDateResponse)
#endif
#ifdef USE_DATETIME_TIME
LIST_ENTITIES_HANDLER(time, datetime::TimeEntity, ListEntitiesTimeResponse)
#endif
#ifdef USE_DATETIME_DATETIME
LIST_ENTITIES_HANDLER(datetime, datetime::DateTimeEntity, ListEntitiesDateTimeResponse)
#endif
#ifdef USE_TEXT
LIST_ENTITIES_HANDLER(text, text::Text, ListEntitiesTextResponse)
#endif
#ifdef USE_SELECT
LIST_ENTITIES_HANDLER(select, select::Select, ListEntitiesSelectResponse)
#endif
#ifdef USE_MEDIA_PLAYER
LIST_ENTITIES_HANDLER(media_player, media_player::MediaPlayer, ListEntitiesMediaPlayerResponse)
#endif
#ifdef USE_ALARM_CONTROL_PANEL
LIST_ENTITIES_HANDLER(alarm_control_panel, alarm_control_panel::AlarmControlPanel,
                      ListEntitiesAlarmControlPanelResponse)
#endif
#ifdef USE_EVENT
LIST_ENTITIES_HANDLER(event, event::Event, ListEntitiesEventResponse)
#endif
#ifdef USE_UPDATE
LIST_ENTITIES_HANDLER(update, update::UpdateEntity, ListEntitiesUpdateResponse)
#endif

// Special cases that don't follow the pattern
bool ListEntitiesIterator::on_end() { return this->client_->send_list_info_done(); }

ListEntitiesIterator::ListEntitiesIterator(APIConnection *client) : client_(client) {}

#ifdef USE_API_USER_DEFINED_ACTIONS
bool ListEntitiesIterator::on_service(UserServiceDescriptor *service) {
  auto resp = service->encode_list_service_response();
  return this->client_->send_message(resp, ListEntitiesServicesResponse::MESSAGE_TYPE);
}
#endif

}  // namespace esphome::api
#endif
