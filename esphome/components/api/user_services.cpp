#include "user_services.h"
#include "esphome/core/log.h"

namespace esphome {
namespace api {

template<> bool ExecuteServiceArgument::get_value<bool>() { return this->value_bool_; }
template<> int ExecuteServiceArgument::get_value<int>() { return this->value_int_; }
template<> float ExecuteServiceArgument::get_value<float>() { return this->value_float_; }
template<> std::string ExecuteServiceArgument::get_value<std::string>() { return this->value_string_; }

APIMessageType ExecuteServiceArgument::message_type() const { return APIMessageType::EXECUTE_SERVICE_REQUEST; }
bool ExecuteServiceArgument::decode_varint(uint32_t field_id, uint32_t value) {
  switch (field_id) {
    case 1:  // bool bool_ = 1;
      this->value_bool_ = value;
      return true;
    case 2:  // int32 int_ = 2;
      this->value_int_ = value;
      return true;
    default:
      return false;
  }
}
bool ExecuteServiceArgument::decode_32bit(uint32_t field_id, uint32_t value) {
  switch (field_id) {
    case 3:  // float float_ = 3;
      this->value_float_ = as_float(value);
      return true;
    default:
      return false;
  }
}
bool ExecuteServiceArgument::decode_length_delimited(uint32_t field_id, const uint8_t *value, size_t len) {
  switch (field_id) {
    case 4:  // string string_ = 4;
      this->value_string_ = as_string(value, len);
      return true;
    default:
      return false;
  }
}

bool ExecuteServiceRequest::decode_32bit(uint32_t field_id, uint32_t value) {
  switch (field_id) {
    case 1:  // fixed32 key = 1;
      this->key_ = value;
      return true;
    default:
      return false;
  }
}
bool ExecuteServiceRequest::decode_length_delimited(uint32_t field_id, const uint8_t *value, size_t len) {
  switch (field_id) {
    case 2: {  // repeated ExecuteServiceArgument args = 2;
      ExecuteServiceArgument arg;
      arg.decode(value, len);
      this->args_.push_back(arg);
      return true;
    }
    default:
      return false;
  }
}
APIMessageType ExecuteServiceRequest::message_type() const { return APIMessageType::EXECUTE_SERVICE_REQUEST; }
const std::vector<ExecuteServiceArgument> &ExecuteServiceRequest::get_args() const { return this->args_; }
uint32_t ExecuteServiceRequest::get_key() const { return this->key_; }

ServiceTypeArgument::ServiceTypeArgument(const std::string &name, ServiceArgType type) : name_(name), type_(type) {}
const std::string &ServiceTypeArgument::get_name() const { return this->name_; }
ServiceArgType ServiceTypeArgument::get_type() const { return this->type_; }

}  // namespace api
}  // namespace esphome
