#pragma once

#include <utility>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "api_pb2.h"

#ifdef USE_API_SERVICES
namespace esphome::api {

class UserServiceDescriptor {
 public:
  virtual ListEntitiesServicesResponse encode_list_service_response() = 0;

  virtual bool execute_service(const ExecuteServiceRequest &req) = 0;

  bool is_internal() { return false; }
};

template<typename T> T get_execute_arg_value(const ExecuteServiceArgument &arg);

template<typename T> enums::ServiceArgType to_service_arg_type();

// Base class for YAML-defined services (most common case)
// Stores only pointers to string literals in flash - no heap allocation
template<typename... Ts> class UserServiceBase : public UserServiceDescriptor {
 public:
  UserServiceBase(const char *name, const std::array<const char *, sizeof...(Ts)> &arg_names)
      : name_(name), arg_names_(arg_names) {
    this->key_ = fnv1_hash(name);
  }

  ListEntitiesServicesResponse encode_list_service_response() override {
    ListEntitiesServicesResponse msg;
    msg.set_name(StringRef(this->name_));
    msg.key = this->key_;
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
    this->execute_(req.args, typename gens<sizeof...(Ts)>::type());
    return true;
  }

 protected:
  virtual void execute(Ts... x) = 0;
  template<typename ArgsContainer, int... S> void execute_(const ArgsContainer &args, seq<S...> type) {
    this->execute((get_execute_arg_value<Ts>(args[S]))...);
  }

  // Pointers to string literals in flash - no heap allocation
  const char *name_;
  std::array<const char *, sizeof...(Ts)> arg_names_;
  uint32_t key_{0};
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
    this->execute_(req.args, typename gens<sizeof...(Ts)>::type());
    return true;
  }

 protected:
  virtual void execute(Ts... x) = 0;
  template<typename ArgsContainer, int... S> void execute_(const ArgsContainer &args, seq<S...> type) {
    this->execute((get_execute_arg_value<Ts>(args[S]))...);
  }

  // Heap-allocated strings for runtime-generated names
  std::string name_;
  std::array<std::string, sizeof...(Ts)> arg_names_;
  uint32_t key_{0};
};

template<typename... Ts> class UserServiceTrigger : public UserServiceBase<Ts...>, public Trigger<Ts...> {
 public:
  // Constructor for static names (YAML-defined services - used by code generator)
  UserServiceTrigger(const char *name, const std::array<const char *, sizeof...(Ts)> &arg_names)
      : UserServiceBase<Ts...>(name, arg_names) {}

 protected:
  void execute(Ts... x) override { this->trigger(x...); }  // NOLINT
};

}  // namespace esphome::api
#endif  // USE_API_SERVICES
