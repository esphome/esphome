#include "component_iterator.h"

#include "esphome/core/application.h"

#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif
#ifdef USE_API_USER_DEFINED_ACTIONS
#include "esphome/components/api/user_services.h"
#endif

namespace esphome {

void ComponentIterator::begin(bool include_internal) {
  this->state_ = IteratorState::BEGIN;
  this->at_ = 0;
  this->include_internal_ = include_internal;
}

void ComponentIterator::advance_platform_() {
  this->state_ = static_cast<IteratorState>(static_cast<uint32_t>(this->state_) + 1);
  this->at_ = 0;
}

void ComponentIterator::advance() {
  switch (this->state_) {
    case IteratorState::NONE:
      // not started
      return;
    case IteratorState::BEGIN:
      if (this->on_begin()) {
        advance_platform_();
      }
      break;

#ifdef USE_BINARY_SENSOR
    case IteratorState::BINARY_SENSOR:
      this->process_platform_item_(App.get_binary_sensors(), &ComponentIterator::on_binary_sensor);
      break;
#endif

#ifdef USE_COVER
    case IteratorState::COVER:
      this->process_platform_item_(App.get_covers(), &ComponentIterator::on_cover);
      break;
#endif

#ifdef USE_FAN
    case IteratorState::FAN:
      this->process_platform_item_(App.get_fans(), &ComponentIterator::on_fan);
      break;
#endif

#ifdef USE_LIGHT
    case IteratorState::LIGHT:
      this->process_platform_item_(App.get_lights(), &ComponentIterator::on_light);
      break;
#endif

#ifdef USE_SENSOR
    case IteratorState::SENSOR:
      this->process_platform_item_(App.get_sensors(), &ComponentIterator::on_sensor);
      break;
#endif

#ifdef USE_SWITCH
    case IteratorState::SWITCH:
      this->process_platform_item_(App.get_switches(), &ComponentIterator::on_switch);
      break;
#endif

#ifdef USE_BUTTON
    case IteratorState::BUTTON:
      this->process_platform_item_(App.get_buttons(), &ComponentIterator::on_button);
      break;
#endif

#ifdef USE_TEXT_SENSOR
    case IteratorState::TEXT_SENSOR:
      this->process_platform_item_(App.get_text_sensors(), &ComponentIterator::on_text_sensor);
      break;
#endif

#ifdef USE_API_USER_DEFINED_ACTIONS
    case IteratorState::SERVICE:
      this->process_platform_item_(api::global_api_server->get_user_services(), &ComponentIterator::on_service);
      break;
#endif

#ifdef USE_CAMERA
    case IteratorState::CAMERA: {
      camera::Camera *camera_instance = camera::Camera::instance();
      if (camera_instance != nullptr && (!camera_instance->is_internal() || this->include_internal_)) {
        this->on_camera(camera_instance);
      }
      advance_platform_();
    } break;
#endif

#ifdef USE_CLIMATE
    case IteratorState::CLIMATE:
      this->process_platform_item_(App.get_climates(), &ComponentIterator::on_climate);
      break;
#endif

#ifdef USE_NUMBER
    case IteratorState::NUMBER:
      this->process_platform_item_(App.get_numbers(), &ComponentIterator::on_number);
      break;
#endif

#ifdef USE_DATETIME_DATE
    case IteratorState::DATETIME_DATE:
      this->process_platform_item_(App.get_dates(), &ComponentIterator::on_date);
      break;
#endif

#ifdef USE_DATETIME_TIME
    case IteratorState::DATETIME_TIME:
      this->process_platform_item_(App.get_times(), &ComponentIterator::on_time);
      break;
#endif

#ifdef USE_DATETIME_DATETIME
    case IteratorState::DATETIME_DATETIME:
      this->process_platform_item_(App.get_datetimes(), &ComponentIterator::on_datetime);
      break;
#endif

#ifdef USE_TEXT
    case IteratorState::TEXT:
      this->process_platform_item_(App.get_texts(), &ComponentIterator::on_text);
      break;
#endif

#ifdef USE_SELECT
    case IteratorState::SELECT:
      this->process_platform_item_(App.get_selects(), &ComponentIterator::on_select);
      break;
#endif

#ifdef USE_LOCK
    case IteratorState::LOCK:
      this->process_platform_item_(App.get_locks(), &ComponentIterator::on_lock);
      break;
#endif

#ifdef USE_VALVE
    case IteratorState::VALVE:
      this->process_platform_item_(App.get_valves(), &ComponentIterator::on_valve);
      break;
#endif

#ifdef USE_MEDIA_PLAYER
    case IteratorState::MEDIA_PLAYER:
      this->process_platform_item_(App.get_media_players(), &ComponentIterator::on_media_player);
      break;
#endif

#ifdef USE_ALARM_CONTROL_PANEL
    case IteratorState::ALARM_CONTROL_PANEL:
      this->process_platform_item_(App.get_alarm_control_panels(), &ComponentIterator::on_alarm_control_panel);
      break;
#endif

#ifdef USE_EVENT
    case IteratorState::EVENT:
      this->process_platform_item_(App.get_events(), &ComponentIterator::on_event);
      break;
#endif

#ifdef USE_UPDATE
    case IteratorState::UPDATE:
      this->process_platform_item_(App.get_updates(), &ComponentIterator::on_update);
      break;
#endif

    case IteratorState::MAX:
      if (this->on_end()) {
        this->state_ = IteratorState::NONE;
      }
      return;
  }
}

bool ComponentIterator::on_end() { return true; }
bool ComponentIterator::on_begin() { return true; }
#ifdef USE_API_USER_DEFINED_ACTIONS
bool ComponentIterator::on_service(api::UserServiceDescriptor *service) { return true; }
#endif
#ifdef USE_CAMERA
bool ComponentIterator::on_camera(camera::Camera *camera) { return true; }
#endif
#ifdef USE_MEDIA_PLAYER
bool ComponentIterator::on_media_player(media_player::MediaPlayer *media_player) { return true; }
#endif
}  // namespace esphome
