#pragma once

#include "esphome/core/defines.h"

#ifdef USE_RUNTIME_STATS

#include <cstdint>
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {

class Component;  // Forward declaration

namespace runtime_stats {

static const char *const TAG = "runtime_stats";

class RuntimeStatsCollector {
 public:
  RuntimeStatsCollector();

  void set_log_interval(uint32_t log_interval) {
    this->log_interval_ = log_interval;
    this->next_log_time_ = millis() + log_interval;
  }
  uint32_t get_log_interval() const { return this->log_interval_; }

  // Process any pending stats printing (should be called after component loop)
  void process_pending_stats(uint32_t current_time);

 protected:
  void log_stats_();
  // Static comparators — member functions have friend access, lambdas do not
  static bool compare_period_time(Component *a, Component *b);
  static bool compare_total_time(Component *a, Component *b);

  uint32_t log_interval_;
  uint32_t next_log_time_{0};
};

}  // namespace runtime_stats

extern runtime_stats::RuntimeStatsCollector
    *global_runtime_stats;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome

#endif  // USE_RUNTIME_STATS
