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
void log_op_not_compiled(const char *tag, const char *op_name);
void log_op_unknown(const char *tag, const char *op_name);
void log_op_failed(const char *tag, const char *op_name);

// Operators whose Add*() methods were only added in newer esp-tflite-micro
// releases are detected at compile time, so the resolver also builds against
// older releases. Expanding the operator table in traits mode defines one
// has_<method> trait per TFLM_OP_CONDITIONAL entry, so the traits come from the
// table and cannot drift from it.
#define TFLM_OP_AVAILABLE(op_suffix, method)
#define TFLM_OP_UNAVAILABLE(op_suffix)
#define TFLM_OP_CONDITIONAL(op_suffix, method) \
  template<typename Resolver, typename = void> struct has_##method : std::false_type {}; \
  template<typename Resolver> \
  struct has_##method<Resolver, std::void_t<decltype(std::declval<Resolver &>().method())>> : std::true_type {};

#define TFLM_OPERATORS_TRAITS
#include "tflm_operators.h"
#undef TFLM_OPERATORS_TRAITS
#undef TFLM_OP_AVAILABLE
#undef TFLM_OP_UNAVAILABLE
#undef TFLM_OP_CONDITIONAL

// Compiling every kernel in the table costs flash the firmware may not need.
// When tflite_micro_runtime codegen knows the operator set of every configured
// model, it emits USE_TFLITE_MICRO_SELECTIVE_OPS and one TFLM_OP_ENABLED_<suffix>
// flag per table entry (1 = compiled in, 0 = left out). Without those defines
// every operator stays available, which is what a model swap at run time needs.
#ifdef USE_TFLITE_MICRO_SELECTIVE_OPS
#define TFLM_OP_FLAG(op_suffix) TFLM_OP_ENABLED_##op_suffix
#else
#define TFLM_OP_FLAG(op_suffix) 1
#endif

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
      // For available operators: calls resolver.AddXxx() when the operator is
      // compiled in, reports it as left out otherwise
      // For unavailable operators: logs warning and returns false
      // For conditional operators: same, but only when the library resolver
      // provides the AddXxx() method
      switch (op) {
#define TFLM_OP_AVAILABLE(op_suffix, method) \
  case tflite::BuiltinOperator_##op_suffix: \
    if constexpr (TFLM_OP_FLAG(op_suffix)) { \
      add_status = resolver.method(); \
    } else { \
      log_op_not_compiled(tag, op_name); \
      return false; \
    } \
    break;

#define TFLM_OP_UNAVAILABLE(op_suffix) \
  case tflite::BuiltinOperator_##op_suffix: \
    log_op_unavailable(tag, op_name); \
    return false;

#define TFLM_OP_CONDITIONAL(op_suffix, method) \
  case tflite::BuiltinOperator_##op_suffix: \
    if constexpr (TFLM_OP_FLAG(op_suffix)) { \
      if constexpr (has_##method<Resolver>::value) { \
        add_status = resolver.method(); \
      } else { \
        log_op_unavailable(tag, op_name); \
        return false; \
      } \
    } else { \
      log_op_not_compiled(tag, op_name); \
      return false; \
    } \
    break;

#define TFLM_OPERATORS_ACTIVE
#include "tflm_operators.h"
#undef TFLM_OPERATORS_ACTIVE
#undef TFLM_OP_AVAILABLE
#undef TFLM_OP_UNAVAILABLE
#undef TFLM_OP_CONDITIONAL
#undef TFLM_OP_FLAG

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
