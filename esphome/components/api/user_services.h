#pragma once

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

template<typename... Ts> class UserService : public UserServiceDescriptor, public Trigger<Ts...> {
 public:
  UserService(const std::string &name, const std::array<std::string, sizeof...(Ts)> &arg_names);

  ListEntitiesServicesResponse encode_list_service_response() override;

  bool execute_service(const ExecuteServiceRequest &req) override;

 protected:
  template<int... S> void execute_(std::vector<ExecuteServiceArgument> args, seq<S...>);

  std::string name_;
  uint32_t key_{0};
  std::array<std::string, sizeof...(Ts)> arg_names_;
};

template<typename T> T get_execute_arg_value(const ExecuteServiceArgument &arg);

template<typename... Ts>
template<int... S>
void UserService<Ts...>::execute_(std::vector<ExecuteServiceArgument> args, seq<S...>) {
  this->trigger((get_execute_arg_value<Ts>(args[S]))...);
}
template<typename T>
ServiceArgType to_service_arg_type();

template<typename... Ts> ListEntitiesServicesResponse UserService<Ts...>::encode_list_service_response() {
  ListEntitiesServicesResponse msg;
  msg.name = this->name_;
  msg.key = this->key_;
  std::array<ServiceArgType, sizeof...(Ts)> arg_types = {to_service_arg_type<Ts>()...};
  for (int i = 0; i < sizeof...(Ts); i++) {
    ListEntitiesServicesArgument arg;
    arg.type = arg_types[i];
    arg.name = this->arg_names_[i];
    msg.args.push_back(arg);
  }

  return msg;
}
template<typename... Ts> bool UserService<Ts...>::execute_service(const ExecuteServiceRequest &req) {
  if (req.key != this->key_)
    return false;

  if (req.args.size() != this->arg_names_.size()) {
    return false;
  }

  this->execute_(req.args, typename gens<sizeof...(Ts)>::type());
  return true;
}
template<typename... Ts>
UserService<Ts...>::UserService(const std::string &name, const std::array<std::string, sizeof...(Ts)> &arg_names)
    : name_(name), arg_names_(arg_names) {
  this->key_ = fnv1_hash(this->name_);
}

}  // namespace api
}  // namespace esphome
