#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <set>
#include <type_traits>
#include <utility>
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "esphome/core/log.h"

namespace esphome::tflite_micro_helper {

// Non-template logging helpers - defined in op_resolver.cpp to avoid logging macros in headers.
void log_op_registering(const char *tag, const char *op_name);
void log_op_unavailable(const char *tag, const char *op_name);
void log_op_unknown(const char *tag, const char *op_name);
void log_op_failed(const char *tag, const char *op_name);

// Helpers for operators whose registration methods were only added in newer
// esp-tflite-micro releases (>= 1.3.7). Older library versions (e.g. the
// 1.3.3~1 the clang-tidy build resolves) lack the Add* methods on
// MicroMutableOpResolver, so detect them at compile time and fall back to
// "unavailable" instead of failing to compile.
#define TFLITE_MICRO_HELPER_DEFINE_OPTIONAL_ADD(snake_name, method_name) \
  template<typename Resolver, typename = void> struct has_##snake_name : std::false_type {}; \
  template<typename Resolver> \
  struct has_##snake_name<Resolver, \
                          std::void_t<decltype(std::declval<Resolver &>().method_name())>> \
      : std::true_type {}; \
  template<typename Resolver> \
  TfLiteStatus add_optional_##snake_name(Resolver &resolver, const char *tag, const char *op_name) { \
    if constexpr (has_##snake_name<Resolver>::value) { \
      return resolver.method_name(); \
    } \
    log_op_unavailable(tag, op_name); \
    return kTfLiteError; \
  }

TFLITE_MICRO_HELPER_DEFINE_OPTIONAL_ADD(dynamic_update_slice, AddDynamicUpdateSlice)
TFLITE_MICRO_HELPER_DEFINE_OPTIONAL_ADD(reduce_min, AddReduceMin)
TFLITE_MICRO_HELPER_DEFINE_OPTIONAL_ADD(reduce_all, AddReduceAll)
TFLITE_MICRO_HELPER_DEFINE_OPTIONAL_ADD(reverse_v2, AddReverseV2)
#undef TFLITE_MICRO_HELPER_DEFINE_OPTIONAL_ADD

class OpResolverManager {
 public:
  template<size_t tOpCount>
  static bool register_ops(tflite::MicroMutableOpResolver<tOpCount> &resolver,
                           const std::set<tflite::BuiltinOperator> &required_ops, const char *tag) {
    for (auto op : required_ops) {
      const char *op_name = tflite::EnumNameBuiltinOperator(op);
      log_op_registering(tag, op_name);

      TfLiteStatus add_status = kTfLiteError;

      // X-Macro: generates case statements from tflm_operators.h
      // For available operators: calls resolver.AddXxx()
      // For unavailable operators: logs warning and returns false
      switch (op) {
#define TFLM_OP_AVAILABLE(op_name, method) \
  case tflite::BuiltinOperator_##op_name: \
    add_status = resolver.method(); \
    break;

#define TFLM_OP_UNAVAILABLE(op_name) \
  case tflite::BuiltinOperator_##op_name: \
    log_op_unavailable(tag, #op_name); \
    return false; \
    break;

#define TFLM_OP_CONDITIONAL(op_name, snake_name) \
  case tflite::BuiltinOperator_##op_name: \
    add_status = add_optional_##snake_name(resolver, tag, op_name); \
    break;

#define TFLM_OPERATORS_ACTIVE
#include "tflm_operators.h"
#undef TFLM_OPERATORS_ACTIVE
#undef TFLM_OP_AVAILABLE
#undef TFLM_OP_UNAVAILABLE
#undef TFLM_OP_CONDITIONAL

        default:
          log_op_unknown(tag, op_name);
          return false;
      }

      if (add_status != kTfLiteOk) {
        log_op_failed(tag, op_name);
        return false;
      }
    }
    return true;
  }
};

}  // namespace esphome::tflite_micro_helper

#endif  // USE_TFLITE_MICRO_HELPER
