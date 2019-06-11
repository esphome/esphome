#include "user_services.h"
#include "esphome/core/log.h"

namespace esphome {
namespace api {

template<> bool get_execute_arg_value<bool>(const ExecuteServiceArgument &arg) { return arg.bool_; }
template<> int get_execute_arg_value<int>(const ExecuteServiceArgument &arg) { return arg.int_; }
template<> float get_execute_arg_value<float>(const ExecuteServiceArgument &arg) { return arg.float_; }
template<> std::string get_execute_arg_value<std::string>(const ExecuteServiceArgument &arg) { return arg.string_; }

}  // namespace api
}  // namespace esphome
