#include "component_iterator.h"

#include "esphome/core/application.h"

#ifdef USE_API
#include "esphome/components/api/api_server.h"
#include "esphome/components/api/user_services.h"
#endif

namespace esphome {

template<typename Callbacks> struct Visitor {
  Visitor(size_t at, bool include_internal, bool &success, Callbacks &callbacks)
      : at(at), include_internal(include_internal), success(success), callbacks(callbacks) {}
  template<typename T> void visit(const T &arg) {
    if (stop) {
      return;
    }
    if (at < arg.size() + sum) {
      stop = true;
      using entity_t = typename std::remove_reference<decltype(arg)>::type::value_type;
      auto callback = get_by_type<std::function<bool(entity_t)>>(callbacks);
      if (callback) {
        success = callback(arg[at - sum]);
      } else {
        success = true;
      }

    } else {
      sum += arg.size();
      ++index;
    }
  }

  template<typename Tuple, size_t... Indices>
  void visit_impl(const Tuple &tuple, index_sequence<Indices...> /*unused*/) {
    int dummy[] = {0, ((void) visit(std::get<Indices>(tuple)), 0)...};
    (void) dummy;
  }

  template<typename... Args> void visit(const std::tuple<Args...> &tuple) {
    visit_impl(tuple, make_index_sequence<sizeof...(Args)>{});
  }

  size_t sum{0};
  bool stop{false};
  size_t at;
  bool include_internal;
  bool &success;
  size_t index{0};
  Callbacks &callbacks;
};

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
      Visitor<entityCallbacks<entities_t>::type> visitor(this->at_, this->include_internal_, success, callbacks_);
      visitor.visit(App.get_entities());
      if (visitor.index >= std::tuple_size<entities_t>::value) {
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
