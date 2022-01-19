#pragma once

#include "esphome/core/helpers.h"
#include "esphome/core/automation.h"
#include "api_pb2.h"
#include "api_server.h"

namespace esphome {
namespace api {

template<typename... X> class TemplatableStringValue : public TemplatableValue<std::string, X...> {
 public:
  TemplatableStringValue() : TemplatableValue<std::string, X...>() {}

  template<typename F, enable_if_t<!is_invocable<F, X...>::value, int> = 0>
  TemplatableStringValue(F value) : TemplatableValue<std::string, X...>(value) {}

  template<typename F, enable_if_t<is_invocable<F, X...>::value, int> = 0>
  TemplatableStringValue(F f)
      : TemplatableValue<std::string, X...>([f](X... x) -> std::string { return to_string(f(x...)); }) {}
};

template<typename... Ts> class TemplatableKeyValuePair {
 public:
  template<typename T> TemplatableKeyValuePair(std::string key, T value) : key(std::move(key)), value(value) {}
  std::string key;
  TemplatableStringValue<Ts...> value;
};

template<typename... Ts> class HomeAssistantServiceCallAction : public Action<Ts...> {
 public:
  explicit HomeAssistantServiceCallAction(APIServer *parent, bool is_event) : parent_(parent), is_event_(is_event) {}

  template<typename T> void set_service(T service) { this->service_ = service; }

  template<typename T> void add_data(std::string key, T value) {
    this->data_.push_back(TemplatableKeyValuePair<Ts...>(key, value));
  }
  template<typename T> void add_data_template(std::string key, T value) {
    this->data_template_.push_back(TemplatableKeyValuePair<Ts...>(key, value));
  }
  template<typename T> void add_variable(std::string key, T value) {
    this->variables_.push_back(TemplatableKeyValuePair<Ts...>(key, value));
  }

  void play(Ts... x) override {
    HomeassistantServiceResponse resp;
    resp.service = this->service_.value(x...);
    resp.is_event = this->is_event_;
    for (auto &it : this->data_) {
      HomeassistantServiceMap kv;
      kv.key = it.key;
      kv.value = it.value.value(x...);
      resp.data.push_back(kv);
    }
    for (auto &it : this->data_template_) {
      HomeassistantServiceMap kv;
      kv.key = it.key;
      kv.value = it.value.value(x...);
      resp.data_template.push_back(kv);
    }
    for (auto &it : this->variables_) {
      HomeassistantServiceMap kv;
      kv.key = it.key;
      kv.value = it.value.value(x...);
      resp.variables.push_back(kv);
    }
    this->parent_->send_homeassistant_service_call(resp);
  }

 protected:
  APIServer *parent_;
  bool is_event_;
  TemplatableStringValue<Ts...> service_{};
  std::vector<TemplatableKeyValuePair<Ts...>> data_;
  std::vector<TemplatableKeyValuePair<Ts...>> data_template_;
  std::vector<TemplatableKeyValuePair<Ts...>> variables_;
};

}  // namespace api
}  // namespace esphome
