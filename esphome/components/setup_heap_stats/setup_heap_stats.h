#pragma once

#include "esphome/core/defines.h"

#ifdef USE_SETUP_HEAP_STATS

#include <cstdint>

namespace esphome {

class Component;  // Forward declaration

namespace setup_heap_stats {

struct ComponentHeapEntry {
  Component *component;
  int32_t construction_delta;
  int32_t setup_delta;
};

class SetupHeapStatsCollector {
 public:
  explicit SetupHeapStatsCollector(uint32_t initial_baseline);

  /// Called from register_component_() to record heap delta since last registration
  void record_component_registered(Component *comp);

  /// Called before component->call_setup()
  void record_before_setup(Component *comp);

  /// Called after component->call_setup()
  void record_after_setup(Component *comp);

  /// Log sorted summary and free storage
  void log_summary();

  /// Get free internal heap size (platform-specific)
  static uint32_t get_free_internal_heap();

 protected:
  ComponentHeapEntry *entries_{nullptr};
  uint16_t entry_count_{0};
  uint32_t last_heap_snapshot_{0};
  // Temporary storage for before/after setup measurement
  Component *setup_component_{nullptr};
  uint32_t setup_before_heap_{0};
};

}  // namespace setup_heap_stats

extern setup_heap_stats::SetupHeapStatsCollector
    *global_setup_heap_stats;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome

#endif  // USE_SETUP_HEAP_STATS
