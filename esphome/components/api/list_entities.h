#pragma once

#include "esphome/core/defines.h"
#ifdef USE_API
#include "esphome/core/component.h"
#include "esphome/core/component_iterator.h"
namespace esphome::api {

class APIConnection;

// Macro for generating ListEntitiesIterator handlers
// Calls schedule_message_ with try_send_*_info
#define LIST_ENTITIES_HANDLER(entity_type, EntityClass, ResponseType) \
  bool ListEntitiesIterator::on_##entity_type(EntityClass *entity) { /* NOLINT(bugprone-macro-parentheses) */ \
    return this->client_->schedule_message_(entity, &APIConnection::try_send_##entity_type##_info, \
                                            ResponseType::MESSAGE_TYPE, ResponseType::ESTIMATED_SIZE); \
  }

class ListEntitiesIterator : public ComponentIterator {
 public:
  ListEntitiesIterator(APIConnection *client);
#ifdef USE_BINARY_SENSOR
  bool on_binary_sensor(binary_sensor::BinarySensor *entity) override;
#endif
#ifdef USE_COVER
  bool on_cover(cover::Cover *entity) override;
#endif
#ifdef USE_FAN
  bool on_fan(fan::Fan *entity) override;
#endif
#ifdef USE_LIGHT
  bool on_light(light::LightState *entity) override;
#endif
#ifdef USE_SENSOR
  bool on_sensor(sensor::Sensor *entity) override;
#endif
#ifdef USE_SWITCH
  bool on_switch(switch_::Switch *entity) override;
#endif
#ifdef USE_BUTTON
  bool on_button(button::Button *entity) override;
#endif
#ifdef USE_TEXT_SENSOR
  bool on_text_sensor(text_sensor::TextSensor *entity) override;
#endif
#ifdef USE_API_USER_DEFINED_ACTIONS
  bool on_service(UserServiceDescriptor *service) override;
#endif
#ifdef USE_CAMERA
  bool on_camera(camera::Camera *entity) override;
#endif
#ifdef USE_CLIMATE
  bool on_climate(climate::Climate *entity) override;
#endif
#ifdef USE_NUMBER
  bool on_number(number::Number *entity) override;
#endif
#ifdef USE_DATETIME_DATE
  bool on_date(datetime::DateEntity *entity) override;
#endif
#ifdef USE_DATETIME_TIME
  bool on_time(datetime::TimeEntity *entity) override;
#endif
#ifdef USE_DATETIME_DATETIME
  bool on_datetime(datetime::DateTimeEntity *entity) override;
#endif
#ifdef USE_TEXT
  bool on_text(text::Text *entity) override;
#endif
#ifdef USE_SELECT
  bool on_select(select::Select *entity) override;
#endif
#ifdef USE_LOCK
  bool on_lock(lock::Lock *entity) override;
#endif
#ifdef USE_VALVE
  bool on_valve(valve::Valve *entity) override;
#endif
#ifdef USE_MEDIA_PLAYER
  bool on_media_player(media_player::MediaPlayer *entity) override;
#endif
#ifdef USE_ALARM_CONTROL_PANEL
  bool on_alarm_control_panel(alarm_control_panel::AlarmControlPanel *entity) override;
#endif
#ifdef USE_EVENT
  bool on_event(event::Event *entity) override;
#endif
#ifdef USE_UPDATE
  bool on_update(update::UpdateEntity *entity) override;
#endif
  bool on_end() override;
  bool completed() { return this->state_ == IteratorState::NONE; }

 protected:
  APIConnection *client_;
};

}  // namespace esphome::api
#endif
