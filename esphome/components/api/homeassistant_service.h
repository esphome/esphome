#pragma once

#include "api_server.h"
#ifdef USE_API
#ifdef USE_API_HOMEASSISTANT_SERVICES
#include <functional>
#include <utility>
#include <vector>
#include "api_pb2.h"
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
#include "esphome/components/json/json_util.h"
#endif
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "esphome/core/string_ref.h"

namespace esphome::api {

template<typename... X> class TemplatableStringValue : public TemplatableValue<std::string, X...> {
  // Verify that const char* uses the base class STATIC_STRING optimization (no heap allocation)
  // rather than being wrapped in a lambda. The base class constructor for const char* is more
  // specialized than the templated constructor here, so it should be selected.
  static_assert(std::is_constructible_v<TemplatableValue<std::string, X...>, const char *>,
                "Base class must have const char* constructor for STATIC_STRING optimization");

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
  // Default constructor needed for FixedVector::emplace_back()
  TemplatableKeyValuePair() = default;

  // Keys are always string literals from YAML dictionary keys (e.g., "code", "event")
  // and never templatable values or lambdas. Only the value parameter can be a lambda/template.
  // Using const char* avoids std::string heap allocation - keys remain in flash.
  template<typename T> TemplatableKeyValuePair(const char *key, T value) : key(key), value(value) {}

  const char *key{nullptr};
  TemplatableStringValue<Ts...> value;
};

#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
// Represents the response data from a Home Assistant action
// Note: This class holds a StringRef to the error_message from the protobuf message.
// The protobuf message must outlive the ActionResponse (which is guaranteed since
// the callback is invoked synchronously while the message is on the stack).
class ActionResponse {
 public:
  ActionResponse(bool success, const std::string &error_message) : success_(success), error_message_(error_message) {}

#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  ActionResponse(bool success, const std::string &error_message, const uint8_t *data, size_t data_len)
      : success_(success), error_message_(error_message) {
    if (data == nullptr || data_len == 0)
      return;
    this->json_document_ = json::parse_json(data, data_len);
  }
#endif

  bool is_success() const { return this->success_; }
  // Returns reference to error message - can be implicitly converted to std::string if needed
  const StringRef &get_error_message() const { return this->error_message_; }

#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  // Get data as parsed JSON object (const version returns read-only view)
  JsonObjectConst get_json() const { return this->json_document_.as<JsonObjectConst>(); }
#endif

 protected:
  bool success_;
  StringRef error_message_;
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  JsonDocument json_document_;
#endif
};

// Callback type for action responses
template<typename... Ts> using ActionResponseCallback = std::function<void(const ActionResponse &, Ts...)>;
#endif

template<typename... Ts> class HomeAssistantServiceCallAction : public Action<Ts...> {
 public:
  explicit HomeAssistantServiceCallAction(APIServer *parent, bool is_event) : parent_(parent) {
    this->flags_.is_event = is_event;
  }

  template<typename T> void set_service(T service) { this->service_ = service; }

  // Initialize FixedVector members - called from Python codegen with compile-time known sizes.
  // Must be called before any add_* methods; capacity must match the number of subsequent add_* calls.
  void init_data(size_t count) { this->data_.init(count); }
  void init_data_template(size_t count) { this->data_template_.init(count); }
  void init_variables(size_t count) { this->variables_.init(count); }

  // Keys are always string literals from the Python code generation (e.g., cg.add(var.add_data("tag_id", templ))).
  // The value parameter can be a lambda/template, but keys are never templatable.
  // Using const char* for keys avoids std::string heap allocation - keys remain in flash.
  template<typename V> void add_data(const char *key, V &&value) {
    this->add_kv_(this->data_, key, std::forward<V>(value));
  }
  template<typename V> void add_data_template(const char *key, V &&value) {
    this->add_kv_(this->data_template_, key, std::forward<V>(value));
  }
  template<typename V> void add_variable(const char *key, V &&value) {
    this->add_kv_(this->variables_, key, std::forward<V>(value));
  }

#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
  template<typename T> void set_response_template(T response_template) {
    this->response_template_ = response_template;
    this->flags_.has_response_template = true;
  }

  void set_wants_status() { this->flags_.wants_status = true; }
  void set_wants_response() { this->flags_.wants_response = true; }

#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  Trigger<JsonObjectConst, Ts...> *get_success_trigger_with_response() const {
    return this->success_trigger_with_response_;
  }
#endif
  Trigger<Ts...> *get_success_trigger() const { return this->success_trigger_; }
  Trigger<std::string, Ts...> *get_error_trigger() const { return this->error_trigger_; }
#endif  // USE_API_HOMEASSISTANT_ACTION_RESPONSES

  void play(const Ts &...x) override {
    HomeassistantActionRequest resp;
    std::string service_value = this->service_.value(x...);
    resp.set_service(StringRef(service_value));
    resp.is_event = this->flags_.is_event;
    this->populate_service_map(resp.data, this->data_, x...);
    this->populate_service_map(resp.data_template, this->data_template_, x...);
    this->populate_service_map(resp.variables, this->variables_, x...);

#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
    if (this->flags_.wants_status) {
      // Generate a unique call ID for this service call
      static uint32_t call_id_counter = 1;
      uint32_t call_id = call_id_counter++;
      resp.call_id = call_id;
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
      if (this->flags_.wants_response) {
        resp.wants_response = true;
        // Set response template if provided
        if (this->flags_.has_response_template) {
          std::string response_template_value = this->response_template_.value(x...);
          resp.response_template = response_template_value;
        }
      }
#endif

      auto captured_args = std::make_tuple(x...);
      this->parent_->register_action_response_callback(call_id, [this, captured_args](const ActionResponse &response) {
        std::apply(
            [this, &response](auto &&...args) {
              if (response.is_success()) {
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
                if (this->flags_.wants_response) {
                  this->success_trigger_with_response_->trigger(response.get_json(), args...);
                } else
#endif
                {
                  this->success_trigger_->trigger(args...);
                }
              } else {
                this->error_trigger_->trigger(response.get_error_message(), args...);
              }
            },
            captured_args);
      });
    }
#endif

    this->parent_->send_homeassistant_action(resp);
  }

 protected:
  // Helper to add key-value pairs to FixedVectors
  // Keys are always string literals (const char*), values can be lambdas/templates
  template<typename V> void add_kv_(FixedVector<TemplatableKeyValuePair<Ts...>> &vec, const char *key, V &&value) {
    auto &kv = vec.emplace_back();
    kv.key = key;
    kv.value = std::forward<V>(value);
  }

  template<typename VectorType, typename SourceType>
  static void populate_service_map(VectorType &dest, SourceType &source, Ts... x) {
    dest.init(source.size());
    for (auto &it : source) {
      auto &kv = dest.emplace_back();
      kv.set_key(StringRef(it.key));
      kv.value = it.value.value(x...);
    }
  }

  APIServer *parent_;
  TemplatableStringValue<Ts...> service_{};
  FixedVector<TemplatableKeyValuePair<Ts...>> data_;
  FixedVector<TemplatableKeyValuePair<Ts...>> data_template_;
  FixedVector<TemplatableKeyValuePair<Ts...>> variables_;
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  TemplatableStringValue<Ts...> response_template_{""};
  Trigger<JsonObjectConst, Ts...> *success_trigger_with_response_ = new Trigger<JsonObjectConst, Ts...>();
#endif  // USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  Trigger<Ts...> *success_trigger_ = new Trigger<Ts...>();
  Trigger<std::string, Ts...> *error_trigger_ = new Trigger<std::string, Ts...>();
#endif  // USE_API_HOMEASSISTANT_ACTION_RESPONSES

  struct Flags {
    uint8_t is_event : 1;
    uint8_t wants_status : 1;
    uint8_t wants_response : 1;
    uint8_t has_response_template : 1;
    uint8_t reserved : 5;
  } flags_{0};
};

}  // namespace esphome::api

#endif
#endif
