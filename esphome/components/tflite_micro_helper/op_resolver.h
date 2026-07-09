#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <set>
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "op_resolver_utils.h"

namespace esphome {
namespace tflite_micro_helper {

class OpResolverManager {
 public:
  template<size_t tOpCount>
  static bool RegisterOps(tflite::MicroMutableOpResolver<tOpCount> &resolver,
                          const std::set<tflite::BuiltinOperator> &required_ops, const char *tag) {
    for (auto op : required_ops) {
      const char *op_name = tflite::EnumNameBuiltinOperator(op);
      op_resolver_log_register(tag, op_name);

      TfLiteStatus add_status = kTfLiteError;

      // X-Macro: generate case statements from tflm_operators.h
      // For available operators: call resolver.AddXxx()
      // For unavailable operators: log warning and return false
      switch (op) {
#define TFLM_OP_AVAILABLE(op_name, method) \
  case tflite::BuiltinOperator_##op_name: \
    add_status = resolver.method(); \
    break;

#define TFLM_OP_UNAVAILABLE(op_name) \
  case tflite::BuiltinOperator_##op_name: \
    op_resolver_log_unavailable(tag, #op_name); \
    return false; \
    break;

#define TFLM_OPERATORS_ACTIVE
#include "tflm_operators.h"
#undef TFLM_OPERATORS_ACTIVE
#undef TFLM_OP_AVAILABLE
#undef TFLM_OP_UNAVAILABLE

        default:
          op_resolver_log_unknown(tag, op_name);
          return false;
      }

      if (add_status != kTfLiteOk) {
        op_resolver_log_failed(tag, op_name);
        return false;
      }
    }
    return true;
  }
};

}  // namespace tflite_micro_helper
}  // namespace esphome

#endif  // USE_TFLITE_MICRO_HELPER
