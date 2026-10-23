#include "arena_probe.h"

#ifdef USE_TFLITE_MICRO_HELPER
#include "memory_manager.h"
#include "esphome/components/tflite_op_processor/op_resolver.h"
#include "esphome/core/log.h"
#include <memory>
#include <set>
#include "tensorflow/lite/micro/micro_interpreter.h"

namespace esphome::tflite_memory_handler {

ESPHOME_LOG_TAG(TAG, "tflite_memory_handler");

size_t probe_arena_size(const uint8_t *model_start, size_t initial_size) {
  const tflite::Model *model = tflite::GetModel(model_start);
  if (model == nullptr || model->version() != TFLITE_SCHEMA_VERSION) {
    ESP_LOGE(TAG, "probe_arena_size: Model schema version mismatch");
    return 0;
  }

  // Collect the set of operators required by this model.
  std::set<tflite::BuiltinOperator> required_ops;
  for (size_t i = 0; i < model->operator_codes()->size(); ++i) {
    const auto *op_code = model->operator_codes()->Get(i);
    required_ops.insert(op_code->builtin_code());
  }

  // Build a local resolver for probing. Uses the same registration path as
  // load_model_with_arena so the probe reflects the real runtime setup.
  auto local_resolver = std::make_unique<tflite::MicroMutableOpResolver<TFLITE_MICRO_MAX_OPERATORS>>();
  if (!tflite_op_processor::OpResolverManager::register_ops<TFLITE_MICRO_MAX_OPERATORS>(*local_resolver, required_ops, TAG)) {
    ESP_LOGE(TAG, "probe_arena_size: Failed to register operators for probing");
    return 0;
  }

  // Try the requested size first, then 1.5x, then 2x. All sizes 16-byte aligned.
  size_t attempt_sizes[] = {(initial_size + 15) & ~15, (initial_size * 3 / 2 + 15) & ~15,
                            (initial_size * 2 + 15) & ~15};

  for (size_t attempt_size : attempt_sizes) {
    // Use MemoryManager so PSRAM/override flags are honored identically to the real load.
    MemoryManager::AllocationResult probe_allocation = MemoryManager::allocate_tensor_arena(attempt_size);
    if (!probe_allocation) {
      ESP_LOGW(TAG, "probe_arena_size: Could not allocate %zu bytes for probing", attempt_size);
      continue;
    }
    uint8_t *probe_arena = probe_allocation.data.get();

    auto probe_interpreter =
        std::make_unique<tflite::MicroInterpreter>(model, *local_resolver, probe_arena, probe_allocation.actual_size);
    if (probe_interpreter->AllocateTensors() != kTfLiteOk) {
      probe_interpreter.reset();
      probe_allocation.data.reset();
      continue;
    }

    // The successful attempt size is the proven-good upper bound. The smallest
    // candidate is arena_used_bytes + 16 (aligned to 16 bytes with headroom).
    size_t lower = (probe_interpreter->arena_used_bytes() + 16 + 15) & ~15;
    size_t upper = attempt_size;
    probe_interpreter.reset();

    if (lower >= upper) {
      // Arena usage is already at/above the attempt size. Using more than the
      // allocated buffer would over-run it, so the attempt size is the
      // practical minimum for this bounded probe.
      ESP_LOGI(TAG, "probe_arena_size: arena usage at attempt size; using %zu bytes", upper);
      return upper;
    }

    // Standard binary search down to the smallest proven-working size.
    // Invariant: `upper` is always proven-good; `lower` is a lower bound that
    // grows toward it. The midpoint stays < upper so the loop always makes
    // progress.
    while (lower < upper) {
      size_t mid = lower + (upper - lower) / 2;
      auto test_interpreter = std::make_unique<tflite::MicroInterpreter>(model, *local_resolver, probe_arena, mid);
      bool ok = test_interpreter->AllocateTensors() == kTfLiteOk;
      test_interpreter.reset();
      if (ok) {
        // mid works -- shrink the upper bound.
        upper = mid;
      } else {
        // mid is too small -- raise the lower bound past it.
        lower = mid + 1;
      }
    }

    // lower == upper is the minimum proven-good size. Round up to 16-byte
    // alignment and re-test to confirm (the result is >= the proven size, so it
    // is guaranteed to work -- the re-test is a sanity check).
    size_t aligned_minimum = (lower + 15) & ~15;
    auto final_interpreter =
        std::make_unique<tflite::MicroInterpreter>(model, *local_resolver, probe_arena, aligned_minimum);
    bool final_ok = final_interpreter->AllocateTensors() == kTfLiteOk;
    final_interpreter.reset();
    probe_allocation.data.reset();

    if (final_ok) {
      ESP_LOGI(TAG, "probe_arena_size: minimum required arena: %zu bytes", aligned_minimum);
      return aligned_minimum;
    }
    // upper was proven to work, so fall back to it.
    ESP_LOGI(TAG, "probe_arena_size: using proven size: %zu bytes", upper);
    return upper;
  }
  ESP_LOGE(TAG, "probe_arena_size: all attempt sizes failed");
  return 0;
}

}  // namespace esphome::tflite_memory_handler

#endif  // USE_TFLITE_MICRO_HELPER
