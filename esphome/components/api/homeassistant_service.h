#pragma once

#include "api_server.h"
#ifdef USE_API
#ifdef USE_API_HOMEASSISTANT_SERVICES
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include "api_pb2.h"
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
#include "esphome/components/json/json_util.h"
#endif
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "esphome/core/progmem.h"
#include "esphome/core/string_ref.h"

namespace esphome::api {

// Converts a lambda result to the string sent to Home Assistant
template<typename T>
  requires(!std::is_pointer_v<std::remove_cvref_t<T>>)
std::string field_to_string(T &&val) {
  return to_string(std::forward<T>(val));  // NOLINT
}
inline std::string field_to_string(const char *val) { return val ? std::string(val) : std::string(); }
inline std::string field_to_string(std::string val) { return val; }
inline std::string field_to_string(StringRef val) { return val.str(); }

/// A key and value from codegen; on ESP8266 the table and its strings are in flash.
/// The value is the constant `value`, or the result of `fn` when it is set.
template<typename... Ts> struct HomeAssistantField {
  const char *key;
  const char *value;
  std::string (*fn)(const Ts &...);

  template<typename F> static constexpr HomeAssistantField from_lambda(const char *key, F /*lambda*/) {
    return {key, nullptr, &call_lambda<F>};
  }
  template<typename F> static std::string call_lambda(const Ts &...x) { return field_to_string(F{}(x...)); }
};

#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
// Represents the response data from a Home Assistant action
// Note: This class holds a StringRef to the error_message from the protobuf message.
// The protobuf message must outlive the ActionResponse (which is guaranteed since
// the callback is invoked synchronously while the message is on the stack).
class ActionResponse {
 public:
  ActionResponse(bool success, StringRef error_message) : success_(success), error_message_(error_message) {}

#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  ActionResponse(bool success, StringRef error_message, const uint8_t *data, size_t data_len)
      : success_(success), error_message_(error_message) {
    if (data == nullptr || data_len == 0)
      return;
    JsonDocument tmp = json::parse_json(data, data_len);
    swap(this->json_document_, tmp);
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

/// Called with the request just before it is sent, and the context passed with it.
using HomeAssistantPrepareFn = void (*)(HomeassistantActionRequest &, void *);

/// Builds the request from `fields` and the trigger args, calls `prepare` (when set), then sends it.
/// `fields` is a codegen table: the action or event name (no key), then the data, data_template
/// and variables entries.
template<typename... Ts>
void send_homeassistant_fields_with(HomeAssistantPrepareFn prepare, void *context, APIServer *server, bool is_event,
                                    const HomeAssistantField<Ts...> *fields, uint8_t data_count,
                                    uint8_t data_template_count, uint8_t variables_count,
                                    const std::type_identity_t<Ts> &...x) {
  using Field = HomeAssistantField<Ts...>;
  const size_t total = 1 + data_count + data_template_count + variables_count;

  // Lambda results must live until the send; the constant strings are encoded from flash where they are
  size_t lambda_count = 0;
  for (size_t i = 0; i < total; i++)
    lambda_count += fields[i].fn != nullptr;
  FixedVector<std::string> results;
  results.init(lambda_count);
  auto string_ref = [](const char *str) { return ProgmemStringRef(str, ESPHOME_strlen_P(str)); };
  auto value_ref = [&](const Field &field) {
    if (field.fn == nullptr)
      return string_ref(field.value);
    results.push_back(field.fn(x...));
    return ProgmemStringRef(StringRef(results.back()));
  };
  auto fill = [&](FixedVector<HomeassistantServiceMap> &dest, uint8_t count) {
    dest.init(count);
    for (uint8_t i = 0; i < count; i++, fields++) {
      auto &kv = dest.emplace_back();
      kv.key = string_ref(fields->key);
      kv.value = value_ref(*fields);
    }
  };

  HomeassistantActionRequest resp;
  resp.service = value_ref(*fields++);
  resp.is_event = is_event;
  fill(resp.data, data_count);
  fill(resp.data_template, data_template_count);
  fill(resp.variables, variables_count);
  if (prepare != nullptr)
    prepare(resp, context);
  server->send_homeassistant_action(resp);
}

/// Sends an action or event that does not wait for a response; generated actions call it.
template<typename... Ts>
void send_homeassistant_fields(APIServer *server, bool is_event, const HomeAssistantField<Ts...> *fields,
                               uint8_t data_count, uint8_t data_template_count, uint8_t variables_count,
                               const std::type_identity_t<Ts> &...x) {
  send_homeassistant_fields_with(nullptr, nullptr, server, is_event, fields, data_count, data_template_count,
                                 variables_count, x...);
}

#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
/// An action whose on_success or on_error automation runs when Home Assistant responds.
template<typename... Ts> class HomeAssistantServiceCallAction final : public Action<Ts...> {
 public:
  using Field = HomeAssistantField<Ts...>;

  /// `fields` is a table as for `send_homeassistant_fields_with`.
  HomeAssistantServiceCallAction(APIServer *parent, const Field *fields, uint8_t data_count,
                                 uint8_t data_template_count, uint8_t variables_count)
      : parent_(parent),
        fields_(fields),
        data_count_(data_count),
        data_template_count_(data_template_count),
        variables_count_(variables_count) {}

#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  template<typename T> void set_response_template(T response_template) { this->response_template_ = response_template; }

  void set_wants_response() { this->wants_response_ = true; }

  Trigger<JsonObjectConst, Ts...> *get_success_trigger_with_response() { return &this->success_trigger_with_response_; }
#endif
  Trigger<Ts...> *get_success_trigger() { return &this->success_trigger_; }
  Trigger<std::string, Ts...> *get_error_trigger() { return &this->error_trigger_; }

  void play(const Ts &...x) override {
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
    // IMPORTANT: Declare at outer scope so it lives until send_homeassistant_action returns.
    std::string response_template_value;
#endif
    auto prepare = [&](HomeassistantActionRequest &resp) {
      // Generate a unique call ID for this service call
      static uint32_t call_id_counter = 1;
      uint32_t call_id = call_id_counter++;
      resp.call_id = call_id;
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
      if (this->wants_response_) {
        resp.wants_response = true;
        // Set response template if provided
        if (this->response_template_.has_value()) {
          response_template_value = this->response_template_.value(x...);
          resp.response_template = StringRef(response_template_value);
        }
      }
#endif

      auto captured_args = std::make_tuple(x...);
      this->parent_->register_action_response_callback(call_id, [this, captured_args](const ActionResponse &response) {
        std::apply(
            [this, &response](auto &&...args) {
              if (response.is_success()) {
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
                if (this->wants_response_) {
                  this->success_trigger_with_response_.trigger(response.get_json(), args...);
                } else
#endif
                {
                  this->success_trigger_.trigger(args...);
                }
              } else {
                this->error_trigger_.trigger(response.get_error_message(), args...);
              }
            },
            captured_args);
      });
    };
    // One body per Ts, shared with the sends that do not wait for a response
    send_homeassistant_fields_with(
        [](HomeassistantActionRequest &resp, void *context) { (*static_cast<decltype(prepare) *>(context))(resp); },
        &prepare, this->parent_, false, this->fields_, this->data_count_, this->data_template_count_,
        this->variables_count_, x...);
  }

 protected:
  APIServer *parent_;
  const Field *fields_;
  uint8_t data_count_;
  uint8_t data_template_count_;
  uint8_t variables_count_;
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  bool wants_response_{false};
  TemplatableValue<std::string, Ts...> response_template_{};
  Trigger<JsonObjectConst, Ts...> success_trigger_with_response_;
#endif  // USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  Trigger<Ts...> success_trigger_;
  Trigger<std::string, Ts...> error_trigger_;
};
#endif  // USE_API_HOMEASSISTANT_ACTION_RESPONSES

}  // namespace esphome::api

#endif
#endif
