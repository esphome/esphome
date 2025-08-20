#pragma once

#include "api_server.h"
#ifdef USE_API
#ifdef USE_API_HOMEASSISTANT_SERVICES
#include "api_pb2.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include <vector>

namespace esphome::api {

template<typename... X> class TemplatableStringValue : public TemplatableValue<std::string, X...> {
 private:
  // Helper to convert value to string - handles the case where value is already a string
  template<typename T> static std::string value_to_string(T &&val) { return to_string(std::forward<T>(val)); }

  // Overloads for string types - needed because std::to_string doesn't support them
  static std::string value_to_string(char *val) {
    return val ? std::string(val) : std::string();
  }  // For lambdas returning char* (e.g., itoa)
  static std::string value_to_string(const char *val) { return std::string(val); }  // For lambdas returning .c_str()
  static std::string value_to_string(const std::string &val) { return val; }
  static std::string value_to_string(std::string &&val) { return std::move(val); }

 public:
  TemplatableStringValue() : TemplatableValue<std::string, X...>() {}

  template<typename F, enable_if_t<!is_invocable<F, X...>::value, int> = 0>
  TemplatableStringValue(F value) : TemplatableValue<std::string, X...>(value) {}

  template<typename F, enable_if_t<is_invocable<F, X...>::value, int> = 0>
  TemplatableStringValue(F f)
      : TemplatableValue<std::string, X...>([f](X... x) -> std::string { return value_to_string(f(x...)); }) {}
};

template<typename... Ts> class TemplatableKeyValuePair {
 public:
  // Keys are always string literals from YAML dictionary keys (e.g., "code", "event")
  // and never templatable values or lambdas. Only the value parameter can be a lambda/template.
  // Using pass-by-value with std::move allows optimal performance for both lvalues and rvalues.
  template<typename T> TemplatableKeyValuePair(std::string key, T value) : key(std::move(key)), value(value) {}
  std::string key;
  TemplatableStringValue<Ts...> value;
};

template<typename... Ts> class HomeAssistantServiceCallAction : public Action<Ts...> {
 public:
  explicit HomeAssistantServiceCallAction(APIServer *parent, bool is_event) : parent_(parent), is_event_(is_event) {}

  template<typename T> void set_service(T service) { this->service_ = service; }

  // Keys are always string literals from the Python code generation (e.g., cg.add(var.add_data("tag_id", templ))).
  // The value parameter can be a lambda/template, but keys are never templatable.
  // Using pass-by-value allows the compiler to optimize for both lvalues and rvalues.
  template<typename T> void add_data(std::string key, T value) { this->data_.emplace_back(std::move(key), value); }
  template<typename T> void add_data_template(std::string key, T value) {
    this->data_template_.emplace_back(std::move(key), value);
  }
  template<typename T> void add_variable(std::string key, T value) {
    this->variables_.emplace_back(std::move(key), value);
  }

  void play(Ts... x) override {
    HomeassistantServiceResponse resp;
    std::string service_value = this->service_.value(x...);
    resp.set_service(StringRef(service_value));
    resp.is_event = this->is_event_;
    for (auto &it : this->data_) {
      resp.data.emplace_back();
      auto &kv = resp.data.back();
      kv.set_key(StringRef(it.key));
      kv.value = it.value.value(x...);
    }
    for (auto &it : this->data_template_) {
      resp.data_template.emplace_back();
      auto &kv = resp.data_template.back();
      kv.set_key(StringRef(it.key));
      kv.value = it.value.value(x...);
    }
    for (auto &it : this->variables_) {
      resp.variables.emplace_back();
      auto &kv = resp.variables.back();
      kv.set_key(StringRef(it.key));
      kv.value = it.value.value(x...);
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

}  // namespace esphome::api
#endif
#endif
