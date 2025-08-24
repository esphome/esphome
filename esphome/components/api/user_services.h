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

template<typename... Ts> class UserServiceBase : public UserServiceDescriptor {
 public:
  UserServiceBase(std::string name, const std::array<std::string, sizeof...(Ts)> &arg_names)
      : name_(std::move(name)), arg_names_(arg_names) {
    this->key_ = fnv1_hash(this->name_);
  }

  ListEntitiesServicesResponse encode_list_service_response() override {
    ListEntitiesServicesResponse msg;
    msg.set_name(StringRef(this->name_));
    msg.key = this->key_;
    std::array<enums::ServiceArgType, sizeof...(Ts)> arg_types = {to_service_arg_type<Ts>()...};
    for (int i = 0; i < sizeof...(Ts); i++) {
      msg.args.emplace_back();
      auto &arg = msg.args.back();
      arg.type = arg_types[i];
      arg.set_name(StringRef(this->arg_names_[i]));
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
  template<int... S> void execute_(std::vector<ExecuteServiceArgument> args, seq<S...> type) {
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

}  // namespace esphome::api
#endif  // USE_API_SERVICES
