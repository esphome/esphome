#include "user_services.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/string_ref.h"

namespace esphome::api {

StringRef UserServiceStatic::str_(size_t idx, std::span<char> &scratch) const {
  const char *s = progmem_read_ptr(&this->strings_[idx]);
  if (s == nullptr)
    return {};
#ifdef USE_ESP8266
  // Codegen sizes the scratch buffer for the largest service; the bound only guards other callers
  if (scratch.empty())
    return {};
  size_t len = strnlen_P(s, scratch.size() - 1);
  progmem_memcpy(scratch.data(), s, len);
  scratch[len] = '\0';
  StringRef ref(scratch.data(), len);
  scratch = scratch.subspan(len + 1);
  return ref;
#else
  return StringRef(s);
#endif
}

ListEntitiesServicesResponse UserServiceStatic::encode_list_service_response_(
    std::span<const enums::ServiceArgType> arg_types, std::span<char> scratch) const {
  ListEntitiesServicesResponse msg;
  msg.name = this->str_(0, scratch);
  msg.key = this->key_;
  msg.supports_response = this->supports_response_;
#ifdef USE_API_USER_DEFINED_ACTION_METADATA
  msg.description = this->str_(1, scratch);
#endif
  msg.args.init(arg_types.size());
  for (size_t i = 0; i < arg_types.size(); i++) {
    size_t base = USER_ACTION_HEADER_STRINGS + i * USER_ACTION_ARG_STRINGS;
    auto &arg = msg.args.emplace_back();
    arg.type = arg_types[i];
    arg.name = this->str_(base, scratch);
#ifdef USE_API_USER_DEFINED_ACTION_METADATA
    arg.description = this->str_(base + 1, scratch);
    arg.example = this->str_(base + 2, scratch);
#endif
  }
  return msg;
}

template<> bool get_execute_arg_value<bool>(const ExecuteServiceArgument &arg) { return arg.bool_; }
template<> int32_t get_execute_arg_value<int32_t>(const ExecuteServiceArgument &arg) {
  if (arg.legacy_int != 0)
    return arg.legacy_int;
  return arg.int_;
}
template<> float get_execute_arg_value<float>(const ExecuteServiceArgument &arg) { return arg.float_; }
template<> std::string get_execute_arg_value<std::string>(const ExecuteServiceArgument &arg) { return arg.string_; }
// Zero-copy StringRef version for YAML-generated services (string_ is null-terminated after decode)
template<> StringRef get_execute_arg_value<StringRef>(const ExecuteServiceArgument &arg) { return arg.string_; }

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
// Zero-copy StringRef version for YAML-generated services
template<> enums::ServiceArgType to_service_arg_type<StringRef>() { return enums::SERVICE_ARG_TYPE_STRING; }

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
