#pragma once

#include "api_server.h"
#ifdef USE_API
#ifdef USE_API_HOMEASSISTANT_SERVICES
#include <functional>
#include <utility>
#include <vector>
#include "api_pb2.h"
#include "esphome/components/json/json_util.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

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

// Represents the response data from a Home Assistant action
class ActionResponse {
 public:
  ActionResponse(bool success, std::string error_message = "")
      : success_(success), error_message_(std::move(error_message)) {}

  bool is_success() const { return this->success_; }
  const std::string &get_error_message() const { return this->error_message_; }
  const std::string &get_data() const { return this->data_; }
  // Get data as parsed JSON object
  // Returns unbound JsonObject if data is empty or invalid JSON
  JsonObject get_json() {
    if (this->data_.empty())
      return JsonObject();  // Return unbound JsonObject if no data

    if (!this->parsed_json_) {
      this->json_document_ = json::parse_json(this->data_);
      this->json_ = this->json_document_.as<JsonObject>();
      this->parsed_json_ = true;
    }
    return this->json_;
  }

  void set_data(const std::string &data) { this->data_ = data; }

 protected:
  bool success_;
  std::string error_message_;
  std::string data_;
  JsonDocument json_document_;
  JsonObject json_;
  bool parsed_json_{false};
};

// Callback type for action responses
template<typename... Ts> using ActionResponseCallback = std::function<void(std::shared_ptr<ActionResponse>, Ts...)>;

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

  template<typename T> void set_response_template(T response_template) {
    this->response_template_ = response_template;
    this->has_response_template_ = true;
  }

  void set_response_callback(ActionResponseCallback<Ts...> callback) {
    this->wants_response_ = true;
    this->response_callback_ = callback;
  }

  void play(Ts... x) override {
    HomeassistantActionRequest resp;
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

    if (this->wants_response_) {
      // Generate a unique call ID for this service call
      static uint32_t call_id_counter = 1;
      uint32_t call_id = call_id_counter++;
      resp.call_id = call_id;
      // Set response template if provided
      if (this->has_response_template_) {
        std::string response_template_value = this->response_template_.value(x...);
        resp.response_template = response_template_value;
      }

      auto captured_args = std::make_tuple(x...);
      this->parent_->register_action_response_callback(call_id, [this, captured_args](
                                                                    std::shared_ptr<ActionResponse> response) {
        std::apply([this, &response](auto &&...args) { this->response_callback_(response, args...); }, captured_args);
      });
    }

    this->parent_->send_homeassistant_action(resp);
  }

 protected:
  APIServer *parent_;
  bool is_event_;
  TemplatableStringValue<Ts...> service_{};
  std::vector<TemplatableKeyValuePair<Ts...>> data_;
  std::vector<TemplatableKeyValuePair<Ts...>> data_template_;
  std::vector<TemplatableKeyValuePair<Ts...>> variables_;
  TemplatableStringValue<Ts...> response_template_{""};
  ActionResponseCallback<Ts...> response_callback_;
  bool wants_response_{false};
  bool has_response_template_{false};
};

template<typename... Ts>
class HomeAssistantActionResponseTrigger : public Trigger<std::shared_ptr<ActionResponse>, Ts...> {
 public:
  HomeAssistantActionResponseTrigger(HomeAssistantServiceCallAction<Ts...> *action) {
    action->set_response_callback(
        [this](std::shared_ptr<ActionResponse> response, Ts... x) { this->trigger(response, x...); });
  }
};

}  // namespace esphome::api
#endif
#endif
