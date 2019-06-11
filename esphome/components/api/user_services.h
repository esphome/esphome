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
  UserService(const std::string &name, const std::array<ServiceArgType, sizeof...(Ts)> &args);

  ListEntitiesServicesResponse encode_list_service_response() override;

  bool execute_service(const ExecuteServiceRequest &req) override;

 protected:
  template<int... S> void execute_(std::vector<ExecuteServiceArgument> args, seq<S...>);

  std::string name_;
  uint32_t key_{0};
  std::array<ServiceArgType, sizeof...(Ts)> args_;
};

template<typename T> T get_execute_arg_value(const ExecuteServiceArgument &arg);

template<typename... Ts>
template<int... S>
void UserService<Ts...>::execute_(std::vector<ExecuteServiceArgument> args, seq<S...>) {
  this->trigger((get_execute_arg_value<Ts>(args[S]))...);
}
template<typename... Ts> ListEntitiesServicesResponse UserService<Ts...>::encode_list_service_response() {
  ListEntitiesServicesResponse msg;
  msg.name = this->name_;
  msg.key = this->key_;

  // repeated ListServicesArgument args = 3;
  for (auto &arg : this->args_) {
    ListEntitiesServicesArgument msg2;
    msg2.name = arg.get_name();
    msg2.type = arg.get_type();
    msg.args.push_back(msg2);
  }
  return msg;
}
template<typename... Ts> bool UserService<Ts...>::execute_service(const ExecuteServiceRequest &req) {
  if (req.key != this->key_)
    return false;

  if (req.args.size() != this->args_.size()) {
    return false;
  }

  this->execute_(req.args, typename gens<sizeof...(Ts)>::type());
  return true;
}
template<typename... Ts>
UserService<Ts...>::UserService(const std::string &name, const std::array<ServiceArgType, sizeof...(Ts)> &args)
    : name_(name), args_(args) {
  this->key_ = fnv1_hash(this->name_);
}

}  // namespace api
}  // namespace esphome
