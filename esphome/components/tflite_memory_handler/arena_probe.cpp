#include "arena_probe.h"

#ifdef USE_TFLITE_MICRO_HELPER
#include "memory_manager.h"
#include "esphome/components/tflite_op_processor/op_resolver.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <memory>
#include <set>
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_utils.h"

namespace esphome::tflite_memory_handler {

ESPHOME_LOG_TAG(TAG, "tflite_memory_handler");

size_t probe_arena_size(const uint8_t *model_start, size_t initial_size) {
  const tflite::Model *model = tflite::GetModel(model_start);
  if (model == nullptr || model->version() != TFLITE_SCHEMA_VERSION) {
    ESP_LOGE(TAG, "probe_arena_size: Model schema version mismatch");
    return 0;
  }

  // Collect the set of operators required by this model. operator_codes() is
  // optional in the flatbuffer, so it is checked before it is walked, and
  // GetBuiltinCode() is used because builtin_code() still reports PLACEHOLDER
  // for models converted with the newer schema.
  const auto *operator_codes = model->operator_codes();
  if (operator_codes == nullptr || operator_codes->size() == 0) {
    ESP_LOGE(TAG, "probe_arena_size: model has no operator codes");
    return 0;
  }
  std::set<tflite::BuiltinOperator> required_ops;
  for (size_t i = 0; i < operator_codes->size(); ++i) {
    required_ops.insert(tflite::GetBuiltinCode(operator_codes->Get(i)));
  }

  // Build a local resolver for probing. Uses the same registration path as
  // load_model_with_arena so the probe reflects the real runtime setup.
  auto local_resolver = std::make_unique<tflite::MicroMutableOpResolver<TFLITE_MICRO_MAX_OPERATORS>>();
  if (!tflite_op_processor::OpResolverManager::register_ops<TFLITE_MICRO_MAX_OPERATORS>(*local_resolver, required_ops,
                                                                                        TAG)) {
    ESP_LOGE(TAG, "probe_arena_size: Failed to register operators for probing");
    return 0;
  }

  // Try the requested size first, then 1.5x, then 2x. All sizes 16-byte aligned.
  const size_t attempt_sizes[] = {(initial_size + 15) & ~15, (initial_size * 3 / 2 + 15) & ~15,
                                  (initial_size * 2 + 15) & ~15};

  for (size_t attempt_size : attempt_sizes) {
    // Use MemoryManager so PSRAM/arena_location are honored identically to the real load.
    MemoryManager::AllocationResult probe_allocation = MemoryManager::allocate_tensor_arena(attempt_size);
    if (!probe_allocation) {
      ESP_LOGW(TAG, "probe_arena_size: Could not allocate %zu bytes for probing", attempt_size);
      continue;
    }
    uint8_t *probe_arena = probe_allocation.data.get();

    auto probe_interpreter =
        std::make_unique<tflite::MicroInterpreter>(model, *local_resolver, probe_arena, probe_allocation.actual_size);
    if (probe_interpreter->AllocateTensors() != kTfLiteOk) {
      // Not necessarily a size problem: a bad model or an unsupported tensor
      // type fails at every size, so each attempt is logged on its own.
      ESP_LOGW(TAG, "probe_arena_size: AllocateTensors() failed with a %zu byte arena", attempt_size);
      probe_interpreter.reset();
      probe_allocation.reset();
      continue;
    }

    // The size that just worked is a proven-good upper bound. What the model
    // reported as used is the smallest candidate, rounded up to 16 bytes with a
    // little headroom for the alignment TFLM adds internally.
    const size_t lower = (probe_interpreter->arena_used_bytes() + 16 + 15) & ~15;
    const size_t upper = attempt_size;
    probe_interpreter.reset();

    if (lower >= upper) {
      // Arena usage is already at/above the attempt size, so the attempt size is
      // the practical minimum for this bounded probe.
      ESP_LOGI(TAG, "probe_arena_size: arena usage at attempt size; using %zu bytes", upper);
      probe_allocation.reset();
      return upper;
    }

    // Test the lower bound first: when arena_used_bytes() plus headroom is
    // already enough (the expected case) this is the whole search.
    auto lower_interpreter = std::make_unique<tflite::MicroInterpreter>(model, *local_resolver, probe_arena, lower);
    const bool lower_ok = lower_interpreter->AllocateTensors() == kTfLiteOk;
    lower_interpreter.reset();
    if (lower_ok) {
      probe_allocation.reset();
      ESP_LOGI(TAG, "probe_arena_size: minimum required arena: %zu bytes", lower);
      return lower;
    }

    // Standard binary search between the failed lower bound and the proven upper
    // bound. Invariant: `upper` is always proven-good, `low` is the smallest
    // size not yet tested. The midpoint stays < upper so the loop progresses.
    size_t low = lower + 1;
    size_t high = upper;
    while (low < high) {
      const size_t mid = low + (high - low) / 2;
      auto test_interpreter = std::make_unique<tflite::MicroInterpreter>(model, *local_resolver, probe_arena, mid);
      const bool ok = test_interpreter->AllocateTensors() == kTfLiteOk;
      test_interpreter.reset();
      if (ok) {
        high = mid;
      } else {
        low = mid + 1;
      }
    }
    probe_allocation.reset();

    // low == high is the smallest size that allocated, but only at this probe
    // pointer: the arena is not 16-byte aligned any more, and TFLM aligns the
    // head itself, so a later arena landing worse can lose up to 12 usable
    // bytes. Add the same 16 bytes of headroom the lower-bound path has, and
    // never go above the size that was already proven to work.
    const size_t aligned_minimum = std::min(((low + 15) & ~15) + 16, upper);
    ESP_LOGI(TAG, "probe_arena_size: minimum required arena: %zu bytes", aligned_minimum);
    return aligned_minimum;
  }
  ESP_LOGE(TAG,
           "probe_arena_size: AllocateTensors() failed at every attempt size, up to %zu bytes (twice the "
           "requested size); the model or its operators are likely invalid rather than the arena being too small",
           attempt_sizes[2]);
  return 0;
}

}  // namespace esphome::tflite_memory_handler

#endif  // USE_TFLITE_MICRO_HELPER
