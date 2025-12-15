#pragma once

#include <tuple>
#include <utility>
#include <vector>

#include "api_pb2.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES_JSON
#include "esphome/components/json/json_util.h"
#endif

#ifdef USE_API_USER_DEFINED_ACTIONS
namespace esphome::api {

// Forward declaration - full definition in api_server.h
class APIServer;

class UserServiceDescriptor {
 public:
  virtual ListEntitiesServicesResponse encode_list_service_response() = 0;

  virtual bool execute_service(const ExecuteServiceRequest &req) = 0;
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES
  // Overload that accepts server-generated action_call_id (avoids client call_id collisions)
  virtual bool execute_service(const ExecuteServiceRequest &req, uint32_t action_call_id) = 0;
#endif

  bool is_internal() { return false; }
};

template<typename T> T get_execute_arg_value(const ExecuteServiceArgument &arg);

template<typename T> enums::ServiceArgType to_service_arg_type();

// Base class for YAML-defined services (most common case)
// Stores only pointers to string literals in flash - no heap allocation
template<typename... Ts> class UserServiceBase : public UserServiceDescriptor {
 public:
  UserServiceBase(const char *name, const std::array<const char *, sizeof...(Ts)> &arg_names,
                  enums::SupportsResponseType supports_response = enums::SUPPORTS_RESPONSE_NONE)
      : name_(name), arg_names_(arg_names), supports_response_(supports_response) {
    this->key_ = fnv1_hash(name);
  }

  ListEntitiesServicesResponse encode_list_service_response() override {
    ListEntitiesServicesResponse msg;
    msg.set_name(StringRef(this->name_));
    msg.key = this->key_;
    msg.supports_response = this->supports_response_;
    std::array<enums::ServiceArgType, sizeof...(Ts)> arg_types = {to_service_arg_type<Ts>()...};
    msg.args.init(sizeof...(Ts));
    for (size_t i = 0; i < sizeof...(Ts); i++) {
      auto &arg = msg.args.emplace_back();
      arg.type = arg_types[i];
      arg.set_name(StringRef(this->arg_names_[i]));
    }
    return msg;
  }

  bool execute_service(const ExecuteServiceRequest &req) override {
    if (req.key != this->key_)
      return false;
    if (req.args.size() != sizeof...(Ts))
      return false;
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES
    this->execute_(req.args, req.call_id, req.return_response, std::make_index_sequence<sizeof...(Ts)>{});
#else
    this->execute_(req.args, 0, false, std::make_index_sequence<sizeof...(Ts)>{});
#endif
    return true;
  }

#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES
  bool execute_service(const ExecuteServiceRequest &req, uint32_t action_call_id) override {
    if (req.key != this->key_)
      return false;
    if (req.args.size() != sizeof...(Ts))
      return false;
    this->execute_(req.args, action_call_id, req.return_response, std::make_index_sequence<sizeof...(Ts)>{});
    return true;
  }
#endif

 protected:
  virtual void execute(uint32_t call_id, bool return_response, Ts... x) = 0;
  template<typename ArgsContainer, size_t... S>
  void execute_(const ArgsContainer &args, uint32_t call_id, bool return_response, std::index_sequence<S...> /*type*/) {
    this->execute(call_id, return_response, (get_execute_arg_value<Ts>(args[S]))...);
  }

  // Pointers to string literals in flash - no heap allocation
  const char *name_;
  std::array<const char *, sizeof...(Ts)> arg_names_;
  uint32_t key_{0};
  enums::SupportsResponseType supports_response_{enums::SUPPORTS_RESPONSE_NONE};
};

// Separate class for custom_api_device services (rare case)
// Stores copies of runtime-generated names
template<typename... Ts> class UserServiceDynamic : public UserServiceDescriptor {
 public:
  UserServiceDynamic(std::string name, const std::array<std::string, sizeof...(Ts)> &arg_names)
      : name_(std::move(name)), arg_names_(arg_names) {
    this->key_ = fnv1_hash(this->name_.c_str());
  }

  ListEntitiesServicesResponse encode_list_service_response() override {
    ListEntitiesServicesResponse msg;
    msg.set_name(StringRef(this->name_));
    msg.key = this->key_;
    msg.supports_response = enums::SUPPORTS_RESPONSE_NONE;  // Dynamic services don't support responses yet
    std::array<enums::ServiceArgType, sizeof...(Ts)> arg_types = {to_service_arg_type<Ts>()...};
    msg.args.init(sizeof...(Ts));
    for (size_t i = 0; i < sizeof...(Ts); i++) {
      auto &arg = msg.args.emplace_back();
      arg.type = arg_types[i];
      arg.set_name(StringRef(this->arg_names_[i]));
    }
    return msg;
  }

  bool execute_service(const ExecuteServiceRequest &req) override {
    if (req.key != this->key_)
      return false;
    if (req.args.size() != sizeof...(Ts))
      return false;
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES
    this->execute_(req.args, req.call_id, req.return_response, std::make_index_sequence<sizeof...(Ts)>{});
#else
    this->execute_(req.args, 0, false, std::make_index_sequence<sizeof...(Ts)>{});
#endif
    return true;
  }

#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES
  // Dynamic services don't support responses yet, but need to implement the interface
  bool execute_service(const ExecuteServiceRequest &req, uint32_t action_call_id) override {
    if (req.key != this->key_)
      return false;
    if (req.args.size() != sizeof...(Ts))
      return false;
    this->execute_(req.args, action_call_id, req.return_response, std::make_index_sequence<sizeof...(Ts)>{});
    return true;
  }
#endif

 protected:
  virtual void execute(uint32_t call_id, bool return_response, Ts... x) = 0;
  template<typename ArgsContainer, size_t... S>
  void execute_(const ArgsContainer &args, uint32_t call_id, bool return_response, std::index_sequence<S...> /*type*/) {
    this->execute(call_id, return_response, (get_execute_arg_value<Ts>(args[S]))...);
  }

  // Heap-allocated strings for runtime-generated names
  std::string name_;
  std::array<std::string, sizeof...(Ts)> arg_names_;
  uint32_t key_{0};
};

// Primary template declaration
template<enums::SupportsResponseType Mode, typename... Ts> class UserServiceTrigger;

// Specialization for NONE - no extra trigger arguments
template<typename... Ts>
class UserServiceTrigger<enums::SUPPORTS_RESPONSE_NONE, Ts...> : public UserServiceBase<Ts...>, public Trigger<Ts...> {
 public:
  UserServiceTrigger(const char *name, const std::array<const char *, sizeof...(Ts)> &arg_names)
      : UserServiceBase<Ts...>(name, arg_names, enums::SUPPORTS_RESPONSE_NONE) {}

 protected:
  void execute(uint32_t /*call_id*/, bool /*return_response*/, Ts... x) override { this->trigger(x...); }
};

// Specialization for OPTIONAL - call_id and return_response trigger arguments
template<typename... Ts>
class UserServiceTrigger<enums::SUPPORTS_RESPONSE_OPTIONAL, Ts...> : public UserServiceBase<Ts...>,
                                                                     public Trigger<uint32_t, bool, Ts...> {
 public:
  UserServiceTrigger(const char *name, const std::array<const char *, sizeof...(Ts)> &arg_names)
      : UserServiceBase<Ts...>(name, arg_names, enums::SUPPORTS_RESPONSE_OPTIONAL) {}

 protected:
  void execute(uint32_t call_id, bool return_response, Ts... x) override {
    this->trigger(call_id, return_response, x...);
  }
};

// Specialization for ONLY - just call_id trigger argument
template<typename... Ts>
class UserServiceTrigger<enums::SUPPORTS_RESPONSE_ONLY, Ts...> : public UserServiceBase<Ts...>,
                                                                 public Trigger<uint32_t, Ts...> {
 public:
  UserServiceTrigger(const char *name, const std::array<const char *, sizeof...(Ts)> &arg_names)
      : UserServiceBase<Ts...>(name, arg_names, enums::SUPPORTS_RESPONSE_ONLY) {}

 protected:
  void execute(uint32_t call_id, bool /*return_response*/, Ts... x) override { this->trigger(call_id, x...); }
};

// Specialization for STATUS - just call_id trigger argument (reports success/error without data)
template<typename... Ts>
class UserServiceTrigger<enums::SUPPORTS_RESPONSE_STATUS, Ts...> : public UserServiceBase<Ts...>,
                                                                   public Trigger<uint32_t, Ts...> {
 public:
  UserServiceTrigger(const char *name, const std::array<const char *, sizeof...(Ts)> &arg_names)
      : UserServiceBase<Ts...>(name, arg_names, enums::SUPPORTS_RESPONSE_STATUS) {}

 protected:
  void execute(uint32_t call_id, bool /*return_response*/, Ts... x) override { this->trigger(call_id, x...); }
};

}  // namespace esphome::api
#endif  // USE_API_USER_DEFINED_ACTIONS

#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES
// Include full definition of APIServer for template implementation
// Must be outside namespace to avoid including STL headers inside namespace
#include "api_server.h"

namespace esphome::api {

template<typename... Ts> class APIRespondAction : public Action<Ts...> {
 public:
  explicit APIRespondAction(APIServer *parent) : parent_(parent) {}

  template<typename V> void set_success(V success) { this->success_ = success; }
  template<typename V> void set_error_message(V error) { this->error_message_ = error; }
  void set_is_optional_mode(bool is_optional) { this->is_optional_mode_ = is_optional; }

#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES_JSON
  void set_data(std::function<void(Ts..., JsonObject)> func) {
    this->json_builder_ = std::move(func);
    this->has_data_ = true;
  }
#endif

  void play(const Ts &...x) override {
    // Extract call_id from first argument - it's always first for optional/only/status modes
    auto args = std::make_tuple(x...);
    uint32_t call_id = std::get<0>(args);

    bool success = this->success_.value(x...);
    std::string error_message = this->error_message_.value(x...);

#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES_JSON
    if (this->has_data_) {
      // For optional mode, check return_response (second arg) to decide if client wants data
      // Use nested if constexpr to avoid compile error when tuple doesn't have enough elements
      // (std::tuple_element_t is evaluated before the && short-circuit, so we must nest)
      if constexpr (sizeof...(Ts) >= 2) {
        if constexpr (std::is_same_v<std::tuple_element_t<1, std::tuple<Ts...>>, bool>) {
          if (this->is_optional_mode_) {
            bool return_response = std::get<1>(args);
            if (!return_response) {
              // Client doesn't want response data, just send success/error
              this->parent_->send_action_response(call_id, success, error_message);
              return;
            }
          }
        }
      }
      // Build and send JSON response
      json::JsonBuilder builder;
      this->json_builder_(x..., builder.root());
      std::string json_str = builder.serialize();
      this->parent_->send_action_response(call_id, success, error_message,
                                          reinterpret_cast<const uint8_t *>(json_str.data()), json_str.size());
      return;
    }
#endif
    this->parent_->send_action_response(call_id, success, error_message);
  }

 protected:
  APIServer *parent_;
  TemplatableValue<bool, Ts...> success_{true};
  TemplatableValue<std::string, Ts...> error_message_{""};
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES_JSON
  std::function<void(Ts..., JsonObject)> json_builder_;
  bool has_data_{false};
#endif
  bool is_optional_mode_{false};
};

// Action to unregister a service call after execution completes
// Automatically appended to the end of action lists for non-none response modes
template<typename... Ts> class APIUnregisterServiceCallAction : public Action<Ts...> {
 public:
  explicit APIUnregisterServiceCallAction(APIServer *parent) : parent_(parent) {}

  void play(const Ts &...x) override {
    // Extract call_id from first argument - same convention as APIRespondAction
    auto args = std::make_tuple(x...);
    uint32_t call_id = std::get<0>(args);
    if (call_id != 0) {
      this->parent_->unregister_active_action_call(call_id);
    }
  }

 protected:
  APIServer *parent_;
};

}  // namespace esphome::api
#endif  // USE_API_USER_DEFINED_ACTION_RESPONSES
