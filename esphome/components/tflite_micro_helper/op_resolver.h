#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <set>
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "esphome/core/log.h"

namespace esphome::tflite_micro_helper {

// Non-template logging helpers - defined in op_resolver.cpp to avoid logging macros in headers.
void log_op_registering(const char *tag, const char *op_name);
void log_op_unavailable(const char *tag, const char *op_name);
void log_op_unknown(const char *tag, const char *op_name);
void log_op_failed(const char *tag, const char *op_name);

class OpResolverManager {
 public:
  template<size_t tOpCount>
  static bool RegisterOps(tflite::MicroMutableOpResolver<tOpCount> &resolver,
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

#define TFLM_OPERATORS_ACTIVE
#include "tflm_operators.h"
#undef TFLM_OPERATORS_ACTIVE
#undef TFLM_OP_AVAILABLE
#undef TFLM_OP_UNAVAILABLE

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
