#include "user_services.h"
#include "esphome/core/log.h"

namespace esphome::api {

template<> bool get_execute_arg_value<bool>(const ExecuteServiceArgument &arg) { return arg.bool_; }
template<> int32_t get_execute_arg_value<int32_t>(const ExecuteServiceArgument &arg) {
  if (arg.legacy_int != 0)
    return arg.legacy_int;
  return arg.int_;
}
template<> float get_execute_arg_value<float>(const ExecuteServiceArgument &arg) { return arg.float_; }
template<> std::string get_execute_arg_value<std::string>(const ExecuteServiceArgument &arg) { return arg.string_; }

// Legacy std::vector versions for external components using custom_api_device.h - optimized with reserve
template<> std::vector<bool> get_execute_arg_value<std::vector<bool>>(const ExecuteServiceArgument &arg) {
  std::vector<bool> result;
  result.reserve(arg.bool_array.size());
  result.insert(result.end(), arg.bool_array.begin(), arg.bool_array.end());
  return result;
}
template<> std::vector<int32_t> get_execute_arg_value<std::vector<int32_t>>(const ExecuteServiceArgument &arg) {
  std::vector<int32_t> result;
  result.reserve(arg.int_array.size());
  result.insert(result.end(), arg.int_array.begin(), arg.int_array.end());
  return result;
}
template<> std::vector<float> get_execute_arg_value<std::vector<float>>(const ExecuteServiceArgument &arg) {
  std::vector<float> result;
  result.reserve(arg.float_array.size());
  result.insert(result.end(), arg.float_array.begin(), arg.float_array.end());
  return result;
}
template<> std::vector<std::string> get_execute_arg_value<std::vector<std::string>>(const ExecuteServiceArgument &arg) {
  std::vector<std::string> result;
  result.reserve(arg.string_array.size());
  result.insert(result.end(), arg.string_array.begin(), arg.string_array.end());
  return result;
}

// New FixedVector const reference versions for YAML-generated services - zero-copy
template<>
const FixedVector<bool> &get_execute_arg_value<const FixedVector<bool> &>(const ExecuteServiceArgument &arg) {
  return arg.bool_array;
}
template<>
const FixedVector<int32_t> &get_execute_arg_value<const FixedVector<int32_t> &>(const ExecuteServiceArgument &arg) {
  return arg.int_array;
}
template<>
const FixedVector<float> &get_execute_arg_value<const FixedVector<float> &>(const ExecuteServiceArgument &arg) {
  return arg.float_array;
}
template<>
const FixedVector<std::string> &get_execute_arg_value<const FixedVector<std::string> &>(
    const ExecuteServiceArgument &arg) {
  return arg.string_array;
}

template<> enums::ServiceArgType to_service_arg_type<bool>() { return enums::SERVICE_ARG_TYPE_BOOL; }
template<> enums::ServiceArgType to_service_arg_type<int32_t>() { return enums::SERVICE_ARG_TYPE_INT; }
template<> enums::ServiceArgType to_service_arg_type<float>() { return enums::SERVICE_ARG_TYPE_FLOAT; }
template<> enums::ServiceArgType to_service_arg_type<std::string>() { return enums::SERVICE_ARG_TYPE_STRING; }

// Legacy std::vector versions for external components using custom_api_device.h
template<> enums::ServiceArgType to_service_arg_type<std::vector<bool>>() { return enums::SERVICE_ARG_TYPE_BOOL_ARRAY; }
template<> enums::ServiceArgType to_service_arg_type<std::vector<int32_t>>() {
  return enums::SERVICE_ARG_TYPE_INT_ARRAY;
}
template<> enums::ServiceArgType to_service_arg_type<std::vector<float>>() {
  return enums::SERVICE_ARG_TYPE_FLOAT_ARRAY;
}
template<> enums::ServiceArgType to_service_arg_type<std::vector<std::string>>() {
  return enums::SERVICE_ARG_TYPE_STRING_ARRAY;
}

// New FixedVector const reference versions for YAML-generated services
template<> enums::ServiceArgType to_service_arg_type<const FixedVector<bool> &>() {
  return enums::SERVICE_ARG_TYPE_BOOL_ARRAY;
}
template<> enums::ServiceArgType to_service_arg_type<const FixedVector<int32_t> &>() {
  return enums::SERVICE_ARG_TYPE_INT_ARRAY;
}
template<> enums::ServiceArgType to_service_arg_type<const FixedVector<float> &>() {
  return enums::SERVICE_ARG_TYPE_FLOAT_ARRAY;
}
template<> enums::ServiceArgType to_service_arg_type<const FixedVector<std::string> &>() {
  return enums::SERVICE_ARG_TYPE_STRING_ARRAY;
}

}  // namespace esphome::api
