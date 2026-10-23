#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <set>
#include <type_traits>
#include <utility>
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

namespace esphome::tflite_op_processor {

// Upper bound for tflite::MicroMutableOpResolver<N> used by the components that
// register operators (model loader, arena probe). tflite_micro_runtime codegen
// raises it to the largest operator count of the configured models through
// cg.add_define(), so the name is prefixed to stay out of the global namespace.
#ifndef TFLITE_MICRO_MAX_OPERATORS
#define TFLITE_MICRO_MAX_OPERATORS 30  // NOLINT
#endif

// Non-template logging helpers - defined in op_resolver.cpp to avoid logging
// macros in headers.
void log_op_registering(const char *tag, const char *op_name);
void log_op_unavailable(const char *tag, const char *op_name);
void log_op_unknown(const char *tag, const char *op_name);
void log_op_failed(const char *tag, const char *op_name);

// Operators whose Add*() methods were only added in newer esp-tflite-micro
// releases are detected at compile time, so the resolver also builds against
// older releases. The operator table marks them TFLM_OP_CONDITIONAL and
// register_ops() reports them as unavailable instead of failing to compile.
#define TFLITE_MICRO_HELPER_DEFINE_HAS_METHOD(method_name) \
  template<typename Resolver, typename = void> struct has_##method_name : std::false_type {}; \
  template<typename Resolver> \
  struct has_## \
      method_name<Resolver, std::void_t<decltype(std::declval<Resolver &>().method_name())>> : std::true_type {};

TFLITE_MICRO_HELPER_DEFINE_HAS_METHOD(AddDynamicUpdateSlice)
TFLITE_MICRO_HELPER_DEFINE_HAS_METHOD(AddReduceMin)
TFLITE_MICRO_HELPER_DEFINE_HAS_METHOD(AddReduceAll)
TFLITE_MICRO_HELPER_DEFINE_HAS_METHOD(AddReverseV2)
#undef TFLITE_MICRO_HELPER_DEFINE_HAS_METHOD

class OpResolverManager {
 public:
  template<size_t tOpCount>
  static bool register_ops(tflite::MicroMutableOpResolver<tOpCount> &resolver,
                           const std::set<tflite::BuiltinOperator> &required_ops, const char *tag) {
    using Resolver = tflite::MicroMutableOpResolver<tOpCount>;

    for (auto op : required_ops) {
      const char *op_name = tflite::EnumNameBuiltinOperator(op);
      log_op_registering(tag, op_name);

      TfLiteStatus add_status = kTfLiteError;

      // X-Macro: generates case statements from tflm_operators.h
      // For available operators: calls resolver.AddXxx()
      // For unavailable operators: logs warning and returns false
      // For conditional operators: same, but only when the library resolver
      // provides the AddXxx() method
      switch (op) {
#define TFLM_OP_AVAILABLE(op_suffix, method) \
  case tflite::BuiltinOperator_##op_suffix: \
    add_status = resolver.method(); \
    break;

#define TFLM_OP_UNAVAILABLE(op_suffix) \
  case tflite::BuiltinOperator_##op_suffix: \
    log_op_unavailable(tag, op_name); \
    return false;

#define TFLM_OP_CONDITIONAL(op_suffix, method) \
  case tflite::BuiltinOperator_##op_suffix: \
    if constexpr (has_##method<Resolver>::value) { \
      add_status = resolver.method(); \
    } else { \
      log_op_unavailable(tag, op_name); \
      return false; \
    } \
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

}  // namespace esphome::tflite_op_processor

#endif  // USE_TFLITE_MICRO_HELPER
