#include "user_services.h"
#include "esphome/core/log.h"

namespace esphome {
namespace api {

template<> bool get_execute_arg_value<bool>(const ExecuteServiceArgument &arg) { return arg.bool_; }
template<> int get_execute_arg_value<int>(const ExecuteServiceArgument &arg) {
  if (arg.legacy_int != 0)
    return arg.legacy_int;
  return arg.int_;
}
template<> float get_execute_arg_value<float>(const ExecuteServiceArgument &arg) { return arg.float_; }
template<> std::string get_execute_arg_value<std::string>(const ExecuteServiceArgument &arg) { return arg.string_; }
template<> std::vector<bool> get_execute_arg_value<std::vector<bool>>(const ExecuteServiceArgument &arg) {
  return arg.bool_array;
}
template<> std::vector<int32_t> get_execute_arg_value<std::vector<int32_t>>(const ExecuteServiceArgument &arg) {
  return arg.int_array;
}
template<> std::vector<float> get_execute_arg_value<std::vector<float>>(const ExecuteServiceArgument &arg) {
  return arg.float_array;
}
template<> std::vector<std::string> get_execute_arg_value<std::vector<std::string>>(const ExecuteServiceArgument &arg) {
  return arg.string_array;
}

template<> enums::ServiceArgType to_service_arg_type<bool>() { return enums::SERVICE_ARG_TYPE_BOOL; }
template<> enums::ServiceArgType to_service_arg_type<int>() { return enums::SERVICE_ARG_TYPE_INT; }
template<> enums::ServiceArgType to_service_arg_type<float>() { return enums::SERVICE_ARG_TYPE_FLOAT; }
template<> enums::ServiceArgType to_service_arg_type<std::string>() { return enums::SERVICE_ARG_TYPE_STRING; }
template<> enums::ServiceArgType to_service_arg_type<std::vector<bool>>() { return enums::SERVICE_ARG_TYPE_BOOL_ARRAY; }
template<> enums::ServiceArgType to_service_arg_type<std::vector<int>>() { return enums::SERVICE_ARG_TYPE_INT_ARRAY; }
template<> enums::ServiceArgType to_service_arg_type<std::vector<float>>() {
  return enums::SERVICE_ARG_TYPE_FLOAT_ARRAY;
}
template<> enums::ServiceArgType to_service_arg_type<std::vector<std::string>>() {
  return enums::SERVICE_ARG_TYPE_STRING_ARRAY;
}

}  // namespace api
}  // namespace esphome
