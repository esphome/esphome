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

// Upper bound for MicroMutableOpResolver<N>; codegen raises it to the widest
// configured model's operator count. The name is prefixed because the define is global.
#ifndef TFLITE_MICRO_MAX_OPERATORS
#define TFLITE_MICRO_MAX_OPERATORS 30  // NOLINT
#endif

// Defined in op_resolver.cpp: logging macros do not belong in a header.
void log_op_registering(const char *tag, const char *op_name);
void log_op_unavailable(const char *tag, const char *op_name);
void log_op_not_compiled(const char *tag, const char *op_name);
void log_op_unknown(const char *tag, const char *op_name);
void log_op_failed(const char *tag, const char *op_name);

// One has_<method> trait per TFLM_OP_CONDITIONAL entry, so the traits come from
// the table and a resolver without that Add*() method still compiles.
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

// Compiling every kernel costs flash, so a build that knows its models emits
// USE_TFLITE_MICRO_SELECTIVE_OPS and one TFLM_OP_ENABLED_<suffix> flag per entry;
// otherwise every operator stays available and the flags are enumerated from the
// table. Read them unqualified: in a selective build the preprocessor replaces
// TFLM_OP_ENABLED_X inside a qualified name and leaves "::" followed by a number.
// Both modes paste the name from the suffix, so ARG_MAX (65536 on ESP-IDF) is never
// macro-expanded into TFLM_OP_ENABLED_65536.
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
// A selective build leaves a kernel in because a model needs it, so a missing
// Add*() method has to fail the build instead of the load.
#define TFLM_OP_REQUIRE_METHOD(method) \
  static_assert(has_##method<Resolver>::value, \
                "esp-tflite-micro does not provide " #method "(), which a configured model needs");
#else
// Every kernel is in, so one the library lacks is reported when a model asks for it.
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
      log_op_registering(tag, op_name);
      TfLiteStatus add_status = kTfLiteError;
      // One case per table entry: register it, or say why it cannot be registered.
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
