#include "util.h"
#include "api_server.h"
#include "user_services.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace api {

ComponentIterator::ComponentIterator(APIServer *server) : server_(server) {}
void ComponentIterator::begin() {
  this->state_ = IteratorState::BEGIN;
  this->at_ = 0;
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
      if (this->at_ >= App.get_binary_sensors().size()) {
        advance_platform = true;
      } else {
        auto *binary_sensor = App.get_binary_sensors()[this->at_];
        if (binary_sensor->is_internal()) {
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
      if (this->at_ >= App.get_covers().size()) {
        advance_platform = true;
      } else {
        auto *cover = App.get_covers()[this->at_];
        if (cover->is_internal()) {
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
      if (this->at_ >= App.get_fans().size()) {
        advance_platform = true;
      } else {
        auto *fan = App.get_fans()[this->at_];
        if (fan->is_internal()) {
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
      if (this->at_ >= App.get_lights().size()) {
        advance_platform = true;
      } else {
        auto *light = App.get_lights()[this->at_];
        if (light->is_internal()) {
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
      if (this->at_ >= App.get_sensors().size()) {
        advance_platform = true;
      } else {
        auto *sensor = App.get_sensors()[this->at_];
        if (sensor->is_internal()) {
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
      if (this->at_ >= App.get_switches().size()) {
        advance_platform = true;
      } else {
        auto *a_switch = App.get_switches()[this->at_];
        if (a_switch->is_internal()) {
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
      if (this->at_ >= App.get_buttons().size()) {
        advance_platform = true;
      } else {
        auto *button = App.get_buttons()[this->at_];
        if (button->is_internal()) {
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
      if (this->at_ >= App.get_text_sensors().size()) {
        advance_platform = true;
      } else {
        auto *text_sensor = App.get_text_sensors()[this->at_];
        if (text_sensor->is_internal()) {
          success = true;
          break;
        } else {
          success = this->on_text_sensor(text_sensor);
        }
      }
      break;
#endif
    case IteratorState ::SERVICE:
      if (this->at_ >= this->server_->get_user_services().size()) {
        advance_platform = true;
      } else {
        auto *service = this->server_->get_user_services()[this->at_];
        success = this->on_service(service);
      }
      break;
#ifdef USE_ESP32_CAMERA
    case IteratorState::CAMERA:
      if (esp32_camera::global_esp32_camera == nullptr) {
        advance_platform = true;
      } else {
        if (esp32_camera::global_esp32_camera->is_internal()) {
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
      if (this->at_ >= App.get_climates().size()) {
        advance_platform = true;
      } else {
        auto *climate = App.get_climates()[this->at_];
        if (climate->is_internal()) {
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
      if (this->at_ >= App.get_numbers().size()) {
        advance_platform = true;
      } else {
        auto *number = App.get_numbers()[this->at_];
        if (number->is_internal()) {
          success = true;
          break;
        } else {
          success = this->on_number(number);
        }
      }
      break;
#endif
#ifdef USE_SELECT
    case IteratorState::SELECT:
      if (this->at_ >= App.get_selects().size()) {
        advance_platform = true;
      } else {
        auto *select = App.get_selects()[this->at_];
        if (select->is_internal()) {
          success = true;
          break;
        } else {
          success = this->on_select(select);
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
bool ComponentIterator::on_service(UserServiceDescriptor *service) { return true; }
#ifdef USE_ESP32_CAMERA
bool ComponentIterator::on_camera(esp32_camera::ESP32Camera *camera) { return true; }
#endif

}  // namespace api
}  // namespace esphome
