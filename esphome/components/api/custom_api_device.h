#pragma once

#include "user_services.h"
#include "api_server.h"

namespace esphome {
namespace api {

template<typename T, typename... Ts> class CustomAPIDeviceService : public UserServiceBase<Ts...> {
 public:
  CustomAPIDeviceService(const std::string &name, const std::array<std::string, sizeof...(Ts)> &arg_names, T *obj,
                         void (T::*callback)(Ts...))
      : UserServiceBase<Ts...>(name, arg_names), obj_(obj), callback_(callback) {}

 protected:
  void execute_(Ts... x) override { (this->obj_->*this->callback_)(x...); }

  T *obj_;
  void (T::*callback_)(Ts...);
};

class CustomAPIDevice {
 public:
  bool is_connected() const { return global_api_server->is_connected(); }

  template<typename T, typename... Ts>
  void register_service(void (T::*callback)(Ts...), const std::string &name,
                        const std::array<std::string, sizeof...(Ts)> &arg_names) {
    auto *service = new CustomAPIDeviceService<T, Ts...>(name, arg_names, (T *) this, callback);
    global_api_server->register_user_service(service);
  }

  template<typename T> void register_service(void (T::*callback)(), const std::string &name) {
    auto *service = new CustomAPIDeviceService<T>(name, {}, (T *) this, callback);
    global_api_server->register_user_service(service);
  }

  template<typename T>
  void subscribe_homeassistant_state(void (T::*callback)(std::string), const std::string &entity_id) {
    auto f = std::bind(callback, (T *) this, std::placeholders::_1);
    global_api_server->subscribe_home_assistant_state(entity_id, f);
  }

  template<typename T>
  void subscribe_homeassistant_state(void (T::*callback)(std::string, std::string), const std::string &entity_id) {
    auto f = std::bind(callback, (T *) this, entity_id, std::placeholders::_1);
    global_api_server->subscribe_home_assistant_state(entity_id, f);
  }

  void call_homeassistant_service(const std::string &service_name) {
    ServiceCallResponse resp;
    resp.service = service_name;
    global_api_server->send_service_call(resp);
  }

  void call_homeassistant_service(const std::string &service_name,
                                  const std::vector<std::pair<const std::string, const std::string>> &data) {
    ServiceCallResponse resp;
    resp.service = service_name;
    for (auto &it : data) {
      ServiceCallMap kv;
      kv.key = it.first;
      kv.value = it.second;
      resp.data.push_back(kv);
    }
    global_api_server->send_service_call(resp);
  }
};

}  // namespace api
}  // namespace esphome
