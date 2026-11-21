#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <cstdint>
#include <set>
#include <type_traits>
#include <utility>
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

namespace esphome::tflite_op_processor {

// Upper bound for tflite::MicroMutableOpResolver<N>; tflite_micro_runtime
// codegen raises it for the widest configured model with cg.add_define().
#ifndef TFLITE_MICRO_MAX_OPERATORS
#define TFLITE_MICRO_MAX_OPERATORS 30  // NOLINT
#endif

// Non-template logging helpers, defined in op_resolver.cpp to keep the macros out of this header.
void log_op_registering(const char *tag, const char *op_name);
void log_op_unavailable(const char *tag, const char *op_name);
void log_op_not_compiled(const char *tag, const char *op_name);
void log_op_unknown(const char *tag, const char *op_name);
void log_op_failed(const char *tag, const char *op_name);

// TFLM_OP_CONDITIONAL entries get a has_<method> trait from the table, so the
// resolver builds against releases without that Add*() method.
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

// With USE_TFLITE_MICRO_SELECTIVE_OPS, codegen emits one TFLM_OP_ENABLED_<suffix>
// flag per table entry (1 compiled in, 0 left out); without it every operator is
// available, which a model swap at run time needs. The suffix is pasted from the
// table because a macro argument would arrive expanded: ARG_MAX is 65536 on
// ESP-IDF, which would become TFLM_OP_ENABLED_65536. The flags are read
// unqualified: a global define in a selective build, an enumerator otherwise.
#ifndef USE_TFLITE_MICRO_SELECTIVE_OPS
#define TFLM_OP_AVAILABLE(op_suffix, method) TFLM_OP_ENABLED_##op_suffix = 1,
#define TFLM_OP_UNAVAILABLE(op_suffix)
#define TFLM_OP_CONDITIONAL(op_suffix, method) TFLM_OP_ENABLED_##op_suffix = 1,
enum : uint8_t {
#define TFLM_OPERATORS_FLAGS
#include "tflm_operators.h"
#undef TFLM_OPERATORS_FLAGS
};
#undef TFLM_OP_AVAILABLE
#undef TFLM_OP_UNAVAILABLE
#undef TFLM_OP_CONDITIONAL
#endif
#ifdef USE_TFLITE_MICRO_SELECTIVE_OPS
// A selective build fails here when the library lacks an Add*() a model needs,
// instead of reporting it at registration time on the device.
#define TFLM_OP_REQUIRE_METHOD(method) \
  static_assert(has_##method<Resolver>::value, \
                "esp-tflite-micro does not provide " #method "(), which a configured model needs");
#else
// The table-driven build keeps every kernel; one the library lacks is reported at load.
#define TFLM_OP_REQUIRE_METHOD(method)
#endif
class OpResolverManager {
 public:
  template<size_t tOpCount>
  static bool register_ops(tflite::MicroMutableOpResolver<tOpCount> &resolver,
                           const std::set<tflite::BuiltinOperator> &required_ops, const char *tag) {
    using Resolver = tflite::MicroMutableOpResolver<tOpCount>;

    for (auto op : required_ops) {
      const char *op_name = tflite::EnumNameBuiltinOperator(op);
      if (op_name == nullptr)
        op_name = "(unknown)";
      log_op_registering(tag, op_name);

      TfLiteStatus add_status = kTfLiteError;

      // X-macro: one case per table entry. A kernel left out of the build, an
      // unavailable operator and an unknown code are reported and fail the load.
      switch (op) {
#define TFLM_OP_AVAILABLE(op_suffix, method) \
  case tflite::BuiltinOperator_##op_suffix: \
    if constexpr (TFLM_OP_ENABLED_##op_suffix) { \
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
    if constexpr (TFLM_OP_ENABLED_##op_suffix) { \
      if constexpr (has_##method<Resolver>::value) { \
        add_status = resolver.method(); \
      } else { \
        TFLM_OP_REQUIRE_METHOD(method) \
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
#undef TFLM_OP_REQUIRE_METHOD

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
