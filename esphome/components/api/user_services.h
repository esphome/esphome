#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "api_message.h"

namespace esphome {
namespace api {

enum ServiceArgType {
  SERVICE_ARG_TYPE_BOOL = 0,
  SERVICE_ARG_TYPE_INT = 1,
  SERVICE_ARG_TYPE_FLOAT = 2,
  SERVICE_ARG_TYPE_STRING = 3,
};

class ServiceTypeArgument {
 public:
  ServiceTypeArgument(const std::string &name, ServiceArgType type);
  const std::string &get_name() const;
  ServiceArgType get_type() const;

 protected:
  std::string name_;
  ServiceArgType type_;
};

class ExecuteServiceArgument : public APIMessage {
 public:
  APIMessageType message_type() const override;
  template<typename T> T get_value();

  bool decode_varint(uint32_t field_id, uint32_t value) override;
  bool decode_length_delimited(uint32_t field_id, const uint8_t *value, size_t len) override;
  bool decode_32bit(uint32_t field_id, uint32_t value) override;

 protected:
  bool value_bool_{false};
  int value_int_{0};
  float value_float_{0.0f};
  std::string value_string_{};
};

class ExecuteServiceRequest : public APIMessage {
 public:
  bool decode_length_delimited(uint32_t field_id, const uint8_t *value, size_t len) override;
  bool decode_32bit(uint32_t field_id, uint32_t value) override;
  APIMessageType message_type() const override;

  uint32_t get_key() const;
  const std::vector<ExecuteServiceArgument> &get_args() const;

 protected:
  uint32_t key_;
  std::vector<ExecuteServiceArgument> args_;
};

class UserServiceDescriptor {
 public:
  virtual void encode_list_service_response(APIBuffer &buffer) = 0;

  virtual bool execute_service(const ExecuteServiceRequest &req) = 0;
};

template<typename... Ts> class UserService : public UserServiceDescriptor, public Trigger<Ts...> {
 public:
  UserService(const std::string &name, const std::array<ServiceTypeArgument, sizeof...(Ts)> &args);

  void encode_list_service_response(APIBuffer &buffer) override;

  bool execute_service(const ExecuteServiceRequest &req) override;

 protected:
  template<int... S> void execute_(std::vector<ExecuteServiceArgument> args, seq<S...>);

  std::string name_;
  uint32_t key_{0};
  std::array<ServiceTypeArgument, sizeof...(Ts)> args_;
};

template<typename... Ts>
template<int... S>
void UserService<Ts...>::execute_(std::vector<ExecuteServiceArgument> args, seq<S...>) {
  this->trigger((args[S].get_value<Ts>())...);
}
template<typename... Ts> void UserService<Ts...>::encode_list_service_response(APIBuffer &buffer) {
  // string name = 1;
  buffer.encode_string(1, this->name_);
  // fixed32 key = 2;
  buffer.encode_fixed32(2, this->key_);

  // repeated ListServicesArgument args = 3;
  for (auto &arg : this->args_) {
    auto nested = buffer.begin_nested(3);
    // string name = 1;
    buffer.encode_string(1, arg.get_name());
    // Type type = 2;
    buffer.encode_int32(2, arg.get_type());
    buffer.end_nested(nested);
  }
}
template<typename... Ts> bool UserService<Ts...>::execute_service(const ExecuteServiceRequest &req) {
  if (req.get_key() != this->key_)
    return false;

  if (req.get_args().size() != this->args_.size()) {
    return false;
  }

  this->execute_(req.get_args(), typename gens<sizeof...(Ts)>::type());
  return true;
}
template<typename... Ts>
UserService<Ts...>::UserService(const std::string &name, const std::array<ServiceTypeArgument, sizeof...(Ts)> &args)
    : name_(name), args_(args) {
  this->key_ = fnv1_hash(this->name_);
}

template<> bool ExecuteServiceArgument::get_value<bool>();
template<> int ExecuteServiceArgument::get_value<int>();
template<> float ExecuteServiceArgument::get_value<float>();
template<> std::string ExecuteServiceArgument::get_value<std::string>();

}  // namespace api
}  // namespace esphome
