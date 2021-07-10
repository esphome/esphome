#pragma once

#include <utility>

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "api_pb2.h"

namespace esphome {
namespace api {

class UserServiceDescriptor {
 public:
  virtual ListEntitiesServicesResponse encode_list_service_response() = 0;

  virtual bool execute_service(const ExecuteServiceRequest &req) = 0;
};

template<typename T> T get_execute_arg_value(const ExecuteServiceArgument &arg);

template<typename T> enums::ServiceArgType to_service_arg_type();

template<typename... Ts> class UserServiceBase : public UserServiceDescriptor {
 public:
  UserServiceBase(std::string name, const std::array<std::string, sizeof...(Ts)> &arg_names)
      : name_(std::move(name)), arg_names_(arg_names) {
    this->key_ = fnv1_hash(this->name_);
  }

  ListEntitiesServicesResponse encode_list_service_response() override {
    ListEntitiesServicesResponse msg;
    msg.name = this->name_;
    msg.key = this->key_;
    std::array<enums::ServiceArgType, sizeof...(Ts)> arg_types = {to_service_arg_type<Ts>()...};
    for (int i = 0; i < sizeof...(Ts); i++) {
      ListEntitiesServicesArgument arg;
      arg.type = arg_types[i];
      arg.name = this->arg_names_[i];
      msg.args.push_back(arg);
    }
    return msg;
  }

  bool execute_service(const ExecuteServiceRequest &req) override {
    if (req.key != this->key_)
      return false;
    if (req.args.size() != this->arg_names_.size())
      return false;
    this->execute_(req.args, typename gens<sizeof...(Ts)>::type());
    return true;
  }

 protected:
  virtual void execute(Ts... x) = 0;
  template<int... S> void execute_(std::vector<ExecuteServiceArgument> args, seq<S...>) {
    this->execute((get_execute_arg_value<Ts>(args[S]))...);
  }

  std::string name_;
  uint32_t key_{0};
  std::array<std::string, sizeof...(Ts)> arg_names_;
};

template<typename... Ts> class UserServiceTrigger : public UserServiceBase<Ts...>, public Trigger<Ts...> {
 public:
  UserServiceTrigger(const std::string &name, const std::array<std::string, sizeof...(Ts)> &arg_names)
      : UserServiceBase<Ts...>(name, arg_names) {}

 protected:
  void execute(Ts... x) override { this->trigger(x...); }  // NOLINT
};

}  // namespace api
}  // namespace esphome
