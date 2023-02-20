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
    case IteratorState::ALL_ENTITIES: {
      auto &all = App.get_entities_all_types();
      size_t sum{0};
      size_t index = 0;
      for (; index < all.size(); ++index) {
        if (this->at_ < all[index].size() + sum) {
          auto *entity = all[index][this->at_ - sum];
          if (entity->is_internal() && !this->include_internal_) {
            success = true;
          } else {
            if (index < this->callbacks_.size() && this->callbacks_[index]) {
              success = this->callbacks_[index](entity);
            } else {
              success = true;
            }
          }
          break;
        }
        sum += all[index].size();
      }
      if (index >= all.size()) {
        advance_platform = true;
      }
    } break;
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
}  // namespace esphome
