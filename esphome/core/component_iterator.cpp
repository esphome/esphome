#include "component_iterator.h"

#include "esphome/core/application.h"

#ifdef USE_API
#include "esphome/components/api/api_server.h"
#include "esphome/components/api/user_services.h"
#endif

namespace esphome {

void ComponentIterator::begin(bool include_internal) {
  this->state_ = IteratorState::BEGIN;
  this->at_ = 0;
  this->include_internal_ = include_internal;
}
void ComponentIterator::advance() {
  bool advance_platform = false;
  bool success = true;
  switch (this->state_) {
    case IteratorState::NONE:
      // not started
      return;
    case IteratorState::BEGIN:
      if (this->on_begin()) {
        advance_platform = true;
      } else {
        return;
      }
      break;
#ifdef USE_BINARY_SENSOR
    case IteratorState::BINARY_SENSOR:
      if (this->at_ >= App.get_entities<binary_sensor::BinarySensor>().size()) {
        advance_platform = true;
      } else {
        auto *binary_sensor = App.get_entities<binary_sensor::BinarySensor>()[this->at_];
        if (binary_sensor->is_internal() && !this->include_internal_) {
          success = true;
          break;
        } else {
          success = this->on_binary_sensor(binary_sensor);
        }
      }
      break;
#endif
#ifdef USE_COVER
    case IteratorState::COVER:
      if (this->at_ >= App.get_entities<cover::Cover>().size()) {
        advance_platform = true;
      } else {
        auto *cover = App.get_entities<cover::Cover>()[this->at_];
        if (cover->is_internal() && !this->include_internal_) {
          success = true;
          break;
        } else {
          success = this->on_cover(cover);
        }
      }
      break;
#endif
#ifdef USE_FAN
    case IteratorState::FAN:
      if (this->at_ >= App.get_entities<fan::Fan>().size()) {
        advance_platform = true;
      } else {
        auto *fan = App.get_entities<fan::Fan>()[this->at_];
        if (fan->is_internal() && !this->include_internal_) {
          success = true;
          break;
        } else {
          success = this->on_fan(fan);
        }
      }
      break;
#endif
#ifdef USE_LIGHT
    case IteratorState::LIGHT:
      if (this->at_ >= App.get_entities<light::LightState>().size()) {
        advance_platform = true;
      } else {
        auto *light = App.get_entities<light::LightState>()[this->at_];
        if (light->is_internal() && !this->include_internal_) {
          success = true;
          break;
        } else {
          success = this->on_light(light);
        }
      }
      break;
#endif
#ifdef USE_SENSOR
    case IteratorState::SENSOR:
      if (this->at_ >= App.get_entities<sensor::Sensor>().size()) {
        advance_platform = true;
      } else {
        auto *sensor = App.get_entities<sensor::Sensor>()[this->at_];
        if (sensor->is_internal() && !this->include_internal_) {
          success = true;
          break;
        } else {
          success = this->on_sensor(sensor);
        }
      }
      break;
#endif
#ifdef USE_SWITCH
    case IteratorState::SWITCH:
      if (this->at_ >= App.get_entities<switch_::Switch>().size()) {
        advance_platform = true;
      } else {
        auto *a_switch = App.get_entities<switch_::Switch>()[this->at_];
        if (a_switch->is_internal() && !this->include_internal_) {
          success = true;
          break;
        } else {
          success = this->on_switch(a_switch);
        }
      }
      break;
#endif
#ifdef USE_BUTTON
    case IteratorState::BUTTON:
      if (this->at_ >= App.get_entities<button::Button>().size()) {
        advance_platform = true;
      } else {
        auto *button = App.get_entities<button::Button>()[this->at_];
        if (button->is_internal() && !this->include_internal_) {
          success = true;
          break;
        } else {
          success = this->on_button(button);
        }
      }
      break;
#endif
#ifdef USE_TEXT_SENSOR
    case IteratorState::TEXT_SENSOR:
      if (this->at_ >= App.get_entities<text_sensor::TextSensor>().size()) {
        advance_platform = true;
      } else {
        auto *text_sensor = App.get_entities<text_sensor::TextSensor>()[this->at_];
        if (text_sensor->is_internal() && !this->include_internal_) {
          success = true;
          break;
        } else {
          success = this->on_text_sensor(text_sensor);
        }
      }
      break;
#endif
#ifdef USE_API
    case IteratorState ::SERVICE:
      if (this->at_ >= api::global_api_server->get_user_services().size()) {
        advance_platform = true;
      } else {
        auto *service = api::global_api_server->get_user_services()[this->at_];
        success = this->on_service(service);
      }
      break;
#endif
#ifdef USE_ESP32_CAMERA
    case IteratorState::CAMERA:
      if (esp32_camera::global_esp32_camera == nullptr) {
        advance_platform = true;
      } else {
        if (esp32_camera::global_esp32_camera->is_internal() && !this->include_internal_) {
          advance_platform = success = true;
          break;
        } else {
          advance_platform = success = this->on_camera(esp32_camera::global_esp32_camera);
        }
      }
      break;
#endif
#ifdef USE_CLIMATE
    case IteratorState::CLIMATE:
      if (this->at_ >= App.get_entities<climate::Climate>().size()) {
        advance_platform = true;
      } else {
        auto *climate = App.get_entities<climate::Climate>()[this->at_];
        if (climate->is_internal() && !this->include_internal_) {
          success = true;
          break;
        } else {
          success = this->on_climate(climate);
        }
      }
      break;
#endif
#ifdef USE_NUMBER
    case IteratorState::NUMBER:
      if (this->at_ >= App.get_entities<number::Number>().size()) {
        advance_platform = true;
      } else {
        auto *number = App.get_entities<number::Number>()[this->at_];
        if (number->is_internal() && !this->include_internal_) {
          success = true;
          break;
        } else {
          success = this->on_number(number);
        }
      }
      break;
#endif
#ifdef USE_DATETIME_DATE
    case IteratorState::DATETIME_DATE:
      if (this->at_ >= App.get_entities<datetime::DateEntity>().size()) {
        advance_platform = true;
      } else {
        auto *date = App.get_entities<datetime::DateEntity>()[this->at_];
        if (date->is_internal() && !this->include_internal_) {
          success = true;
          break;
        } else {
          success = this->on_date(date);
        }
      }
      break;
#endif
#ifdef USE_DATETIME_TIME
    case IteratorState::DATETIME_TIME:
      if (this->at_ >= App.get_entities<datetime::TimeEntity>().size()) {
        advance_platform = true;
      } else {
        auto *time = App.get_entities<datetime::TimeEntity>()[this->at_];
        if (time->is_internal() && !this->include_internal_) {
          success = true;
          break;
        } else {
          success = this->on_time(time);
        }
      }
      break;
#endif
#ifdef USE_DATETIME_DATETIME
    case IteratorState::DATETIME_DATETIME:
      if (this->at_ >= App.get_entities<datetime::DateTimeEntity>().size()) {
        advance_platform = true;
      } else {
        auto *datetime = App.get_entities<datetime::DateTimeEntity>()[this->at_];
        if (datetime->is_internal() && !this->include_internal_) {
          success = true;
          break;
        } else {
          success = this->on_datetime(datetime);
        }
      }
      break;
#endif
#ifdef USE_TEXT
    case IteratorState::TEXT:
      if (this->at_ >= App.get_entities<text::Text>().size()) {
        advance_platform = true;
      } else {
        auto *text = App.get_entities<text::Text>()[this->at_];
        if (text->is_internal() && !this->include_internal_) {
          success = true;
          break;
        } else {
          success = this->on_text(text);
        }
      }
      break;
#endif
#ifdef USE_SELECT
    case IteratorState::SELECT:
      if (this->at_ >= App.get_entities<select::Select>().size()) {
        advance_platform = true;
      } else {
        auto *select = App.get_entities<select::Select>()[this->at_];
        if (select->is_internal() && !this->include_internal_) {
          success = true;
          break;
        } else {
          success = this->on_select(select);
        }
      }
      break;
#endif
#ifdef USE_LOCK
    case IteratorState::LOCK:
      if (this->at_ >= App.get_entities<lock::Lock>().size()) {
        advance_platform = true;
      } else {
        auto *a_lock = App.get_entities<lock::Lock>()[this->at_];
        if (a_lock->is_internal() && !this->include_internal_) {
          success = true;
          break;
        } else {
          success = this->on_lock(a_lock);
        }
      }
      break;
#endif
#ifdef USE_VALVE
    case IteratorState::VALVE:
      if (this->at_ >= App.get_entities<valve::Valve>().size()) {
        advance_platform = true;
      } else {
        auto *valve = App.get_entities<valve::Valve>()[this->at_];
        if (valve->is_internal() && !this->include_internal_) {
          success = true;
          break;
        } else {
          success = this->on_valve(valve);
        }
      }
      break;
#endif
#ifdef USE_MEDIA_PLAYER
    case IteratorState::MEDIA_PLAYER:
      if (this->at_ >= App.get_entities<media_player::MediaPlayer>().size()) {
        advance_platform = true;
      } else {
        auto *media_player = App.get_entities<media_player::MediaPlayer>()[this->at_];
        if (media_player->is_internal() && !this->include_internal_) {
          success = true;
          break;
        } else {
          success = this->on_media_player(media_player);
        }
      }
      break;
#endif
#ifdef USE_ALARM_CONTROL_PANEL
    case IteratorState::ALARM_CONTROL_PANEL:
      if (this->at_ >= App.get_entities<alarm_control_panel::AlarmControlPanel>().size()) {
        advance_platform = true;
      } else {
        auto *a_alarm_control_panel = App.get_entities<alarm_control_panel::AlarmControlPanel>()[this->at_];
        if (a_alarm_control_panel->is_internal() && !this->include_internal_) {
          success = true;
          break;
        } else {
          success = this->on_alarm_control_panel(a_alarm_control_panel);
        }
      }
      break;
#endif
#ifdef USE_EVENT
    case IteratorState::EVENT:
      if (this->at_ >= App.get_entities<event::Event>().size()) {
        advance_platform = true;
      } else {
        auto *event = App.get_entities<event::Event>()[this->at_];
        if (event->is_internal() && !this->include_internal_) {
          success = true;
          break;
        } else {
          success = this->on_event(event);
        }
      }
      break;
#endif
#ifdef USE_UPDATE
    case IteratorState::UPDATE:
      if (this->at_ >= App.get_entities<update::UpdateEntity>().size()) {
        advance_platform = true;
      } else {
        auto *update = App.get_entities<update::UpdateEntity>()[this->at_];
        if (update->is_internal() && !this->include_internal_) {
          success = true;
          break;
        } else {
          success = this->on_update(update);
        }
      }
      break;
#endif
    case IteratorState::MAX:
      if (this->on_end()) {
        this->state_ = IteratorState::NONE;
      }
      return;
  }

  if (advance_platform) {
    this->state_ = static_cast<IteratorState>(static_cast<uint32_t>(this->state_) + 1);
    this->at_ = 0;
  } else if (success) {
    this->at_++;
  }
}
bool ComponentIterator::on_end() { return true; }
bool ComponentIterator::on_begin() { return true; }
#ifdef USE_API
bool ComponentIterator::on_service(api::UserServiceDescriptor *service) { return true; }
#endif
#ifdef USE_ESP32_CAMERA
bool ComponentIterator::on_camera(esp32_camera::ESP32Camera *camera) { return true; }
#endif
#ifdef USE_MEDIA_PLAYER
bool ComponentIterator::on_media_player(media_player::MediaPlayer *media_player) { return true; }
#endif
}  // namespace esphome
