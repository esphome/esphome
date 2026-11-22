#pragma once

#include <span>
#include <utility>
#include <vector>

#include "api_pb2.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

#ifdef USE_API_USER_DEFINED_ACTIONS
namespace esphome::api {

// Forward declaration - full definition in api_server.h
class APIServer;

class UserServiceDescriptor {
 public:
  /// Build the list-entities message; its strings may point into flash (PROGMEM).
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

// Non-template base for YAML-defined services so the list-entities encoder is compiled once.
// All strings live in one PROGMEM pointer table emitted by codegen (see _action_strings in
// __init__.py), so each service costs a single pointer of RAM. Layout: the action name, then
// each argument name; with USE_API_USER_DEFINED_ACTION_METADATA the action description follows
// the name and every argument is (name, description, example); with
// USE_API_USER_DEFINED_ACTION_OPTIONAL_ARGS every argument ends with its default: nullptr for a
// required argument, "" for an optional one without a default. Unset is nullptr.
#ifdef USE_API_USER_DEFINED_ACTION_METADATA
static constexpr size_t USER_ACTION_HEADER_STRINGS = 2;
static constexpr size_t USER_ACTION_ARG_BASE_STRINGS = 3;
#else
static constexpr size_t USER_ACTION_HEADER_STRINGS = 1;
static constexpr size_t USER_ACTION_ARG_BASE_STRINGS = 1;
#endif
#ifdef USE_API_USER_DEFINED_ACTION_OPTIONAL_ARGS
static constexpr size_t USER_ACTION_ARG_STRINGS = USER_ACTION_ARG_BASE_STRINGS + 1;
#else
static constexpr size_t USER_ACTION_ARG_STRINGS = USER_ACTION_ARG_BASE_STRINGS;
#endif
class UserServiceStatic : public UserServiceDescriptor {
 public:
  UserServiceStatic(const char *const *strings, uint32_t key,
                    enums::SupportsResponseType supports_response = enums::SUPPORTS_RESPONSE_NONE)
      : strings_(strings), key_(key), supports_response_(supports_response) {}

 protected:
  ListEntitiesServicesResponse encode_list_service_response_(std::span<const enums::ServiceArgType> arg_types) const;
  /// Table entry `idx` as it sits in flash; nullptr gives an empty string.
  ProgmemStringRef str_(size_t idx) const;

  const char *const *strings_;  // PROGMEM pointer table, read with progmem_read_ptr()
  uint32_t key_;
  enums::SupportsResponseType supports_response_;
};

template<typename... Ts> class UserServiceBase : public UserServiceStatic {
 public:
  using UserServiceStatic::UserServiceStatic;

  ListEntitiesServicesResponse encode_list_service_response() override {
    std::array<enums::ServiceArgType, sizeof...(Ts)> arg_types = {to_service_arg_type<Ts>()...};
    return this->encode_list_service_response_(arg_types);
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
    msg.name = ProgmemStringRef(StringRef(this->name_));
    msg.key = this->key_;
    msg.supports_response = enums::SUPPORTS_RESPONSE_NONE;  // Dynamic services don't support responses yet
    std::array<enums::ServiceArgType, sizeof...(Ts)> arg_types = {to_service_arg_type<Ts>()...};
    msg.args.init(sizeof...(Ts));
    for (size_t i = 0; i < sizeof...(Ts); i++) {
      auto &arg = msg.args.emplace_back();
      arg.type = arg_types[i];
      arg.name = ProgmemStringRef(StringRef(this->arg_names_[i]));
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

/// A YAML-defined action that calls the generated function Fn. Fn takes the call id first unless
/// Mode is SUPPORTS_RESPONSE_NONE, then return_response for SUPPORTS_RESPONSE_OPTIONAL.
template<auto Fn, enums::SupportsResponseType Mode, typename... Ts>
class UserServiceCallback final : public UserServiceBase<Ts...> {
 public:
  UserServiceCallback(const char *const *strings, uint32_t key) : UserServiceBase<Ts...>(strings, key, Mode) {}

 protected:
  void execute([[maybe_unused]] uint32_t call_id, [[maybe_unused]] bool return_response, Ts... x) override {
    if constexpr (Mode == enums::SUPPORTS_RESPONSE_NONE) {
      Fn(x...);
    } else if constexpr (Mode == enums::SUPPORTS_RESPONSE_OPTIONAL) {
      Fn(call_id, return_response, x...);
    } else {
      Fn(call_id, x...);
    }
  }
};

}  // namespace esphome::api
#endif  // USE_API_USER_DEFINED_ACTIONS
