#pragma once

#include "esphome/core/defines.h"

#ifdef USE_RUNTIME_STATS

#include <map>
#include <cstdint>
#include <cstring>
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {

class Component;  // Forward declaration

namespace runtime_stats {

static const char *const TAG = "runtime_stats";

class ComponentRuntimeStats {
 public:
  ComponentRuntimeStats()
      : period_count_(0),
        period_time_us_(0),
        period_max_time_us_(0),
        total_count_(0),
        total_time_us_(0),
        total_max_time_us_(0) {}

  void record_time(uint32_t duration_us) {
    // Update period counters
    this->period_count_++;
    this->period_time_us_ += duration_us;
    if (duration_us > this->period_max_time_us_)
      this->period_max_time_us_ = duration_us;

    // Update total counters (uint64_t to avoid overflow — uint32_t would overflow after ~10 hours)
    this->total_count_++;
    this->total_time_us_ += duration_us;
    if (duration_us > this->total_max_time_us_)
      this->total_max_time_us_ = duration_us;
  }

  void reset_period_stats() {
    this->period_count_ = 0;
    this->period_time_us_ = 0;
    this->period_max_time_us_ = 0;
  }

  // Period stats (reset each logging interval)
  uint32_t get_period_count() const { return this->period_count_; }
  uint32_t get_period_time_us() const { return this->period_time_us_; }
  uint32_t get_period_max_time_us() const { return this->period_max_time_us_; }
  float get_period_avg_time_us() const {
    return this->period_count_ > 0 ? this->period_time_us_ / static_cast<float>(this->period_count_) : 0.0f;
  }

  // Total stats (persistent until reboot, uint64_t to avoid overflow)
  uint32_t get_total_count() const { return this->total_count_; }
  uint64_t get_total_time_us() const { return this->total_time_us_; }
  uint32_t get_total_max_time_us() const { return this->total_max_time_us_; }
  float get_total_avg_time_us() const {
    return this->total_count_ > 0 ? this->total_time_us_ / static_cast<float>(this->total_count_) : 0.0f;
  }

 protected:
  // Period stats (reset each logging interval)
  uint32_t period_count_;
  uint32_t period_time_us_;
  uint32_t period_max_time_us_;

  // Total stats (persistent until reboot)
  uint32_t total_count_;
  uint64_t total_time_us_;
  uint32_t total_max_time_us_;
};

class RuntimeStatsCollector {
 public:
  RuntimeStatsCollector();

  void set_log_interval(uint32_t log_interval) {
    this->log_interval_ = log_interval;
    this->next_log_time_ = millis() + log_interval;
  }
  uint32_t get_log_interval() const { return this->log_interval_; }

  void record_component_time(Component *component, uint32_t duration_us);

  // Process any pending stats printing (should be called after component loop)
  void process_pending_stats(uint32_t current_time);

 protected:
  void log_stats_();

  void reset_stats_() {
    for (auto &it : this->component_stats_) {
      it.second.reset_period_stats();
    }
  }

  // Map from component to its stats
  // We use Component* as the key since each component is unique
  std::map<Component *, ComponentRuntimeStats> component_stats_;
  uint32_t log_interval_;
  uint32_t next_log_time_{0};
};

}  // namespace runtime_stats

extern runtime_stats::RuntimeStatsCollector
    *global_runtime_stats;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome

#endif  // USE_RUNTIME_STATS
