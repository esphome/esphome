#pragma once

#include "esphome/core/defines.h"

#ifdef USE_RUNTIME_STATS

#include <cstdint>
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
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

  // Process any pending stats printing. Called on every Application::loop()
  // tick, so the common "not yet time to log" path must be cheap — inline
  // the gate check and keep the actual logging work out-of-line.
  void ESPHOME_ALWAYS_INLINE process_pending_stats(uint32_t current_time) {
    if ((int32_t) (current_time - this->next_log_time_) >= 0) [[unlikely]] {
      this->process_pending_stats_slow_(current_time);
    }
  }

  // Record the wall time of one main loop iteration excluding the yield/sleep.
  // Called once per loop from Application::loop().
  //   active_us = total time between loop start and just before yield.
  //   before_us = time spent in Phase A (scheduler tick) excluding time
  //               already attributed to per-component stats.
  //   tail_us   = time spent in after_component_phase_ + the trailing record/stats
  //               prefix. Only meaningful on component-phase ticks; reported
  //               as 0 on Phase A-only ticks (no component phase ran, so any
  //               overhead between Phase A and stats belongs to "residual").
  // Residual overhead at log time = active − Σ(component) − before − tail,
  // which captures per-iteration inter-component bookkeeping (set_current_component,
  // WarnIfComponentBlockingGuard construction/destruction, feed_wdt_with_time calls,
  // the for-loop itself).
  void record_loop_active(uint32_t active_us, uint32_t before_us, uint32_t tail_us) {
    this->period_active_count_++;
    this->period_active_time_us_ += active_us;
    if (active_us > this->period_active_max_us_)
      this->period_active_max_us_ = active_us;
    this->total_active_count_++;
    this->total_active_time_us_ += active_us;
    if (active_us > this->total_active_max_us_)
      this->total_active_max_us_ = active_us;

    this->period_before_time_us_ += before_us;
    this->total_before_time_us_ += before_us;
    this->period_tail_time_us_ += tail_us;
    this->total_tail_time_us_ += tail_us;
  }

 protected:
  void process_pending_stats_slow_(uint32_t current_time);
  void log_stats_();
  // Static comparators — member functions have friend access, lambdas do not
  static bool compare_period_time(Component *a, Component *b);
  static bool compare_total_time(Component *a, Component *b);

  uint32_t log_interval_;
  uint32_t next_log_time_{0};

  // Main loop active-time stats (wall time per iteration, excluding yield/sleep).
  // Counters are uint64_t — at sub-millisecond loop times a uint32_t can wrap in
  // a few weeks of uptime, which is well within ESPHome device lifetimes.
  uint64_t period_active_count_{0};
  uint64_t period_active_time_us_{0};
  uint32_t period_active_max_us_{0};
  uint64_t total_active_count_{0};
  uint64_t total_active_time_us_{0};
  uint32_t total_active_max_us_{0};

  // Split of overhead sections — accumulated per iteration.
  uint64_t period_before_time_us_{0};
  uint64_t total_before_time_us_{0};
  uint64_t period_tail_time_us_{0};
  uint64_t total_tail_time_us_{0};
};

}  // namespace runtime_stats

extern runtime_stats::RuntimeStatsCollector
    *global_runtime_stats;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome

#endif  // USE_RUNTIME_STATS
