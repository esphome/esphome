#include "runtime_stats.h"

#ifdef USE_RUNTIME_STATS

#include "esphome/core/application.h"
#include "esphome/core/component.h"
#include <algorithm>

namespace esphome {

namespace runtime_stats {

RuntimeStatsCollector::RuntimeStatsCollector() : log_interval_(60000), next_log_time_(60000) {
  global_runtime_stats = this;
}

void RuntimeStatsCollector::log_stats_() {
  auto &components = App.components_;

  // Single pass: collect active components into stack buffer
  SmallBufferWithHeapFallback<256, Component *> buffer(components.size());
  Component **sorted = buffer.get();
  size_t count = 0;
  for (auto *component : components) {
    if (component->runtime_stats_.period_count > 0) {
      sorted[count++] = component;
    }
  }

  ESP_LOGI(TAG,
           "Component Runtime Statistics\n"
           " Period stats (last %" PRIu32 "ms): %zu active components",
           this->log_interval_, count);

  // Sum component time so we can derive main-loop overhead
  // (active loop time minus time attributable to component loop()s).
  // Period sum iterates the active-in-period subset; total sum must iterate
  // all components since total_active_time_us_ includes iterations where
  // currently-idle components previously ran.
  uint64_t period_component_sum_us = 0;
  for (size_t i = 0; i < count; i++) {
    period_component_sum_us += sorted[i]->runtime_stats_.period_time_us;
  }
  uint64_t total_component_sum_us = 0;
  for (auto *component : components) {
    total_component_sum_us += component->runtime_stats_.total_time_us;
  }

  if (count > 0) {
    // Sort by period runtime (descending)
    std::sort(sorted, sorted + count, compare_period_time);

    // Log top components by period runtime
    for (size_t i = 0; i < count; i++) {
      const auto &stats = sorted[i]->runtime_stats_;
      ESP_LOGI(TAG, "  %s: count=%" PRIu32 ", avg=%.3fms, max=%.2fms, total=%.1fms",
               LOG_STR_ARG(sorted[i]->get_component_log_str()), stats.period_count,
               stats.period_count > 0 ? stats.period_time_us / (float) stats.period_count / 1000.0f : 0.0f,
               stats.period_max_time_us / 1000.0f, stats.period_time_us / 1000.0f);
    }
  }

  // Main-loop overhead for the period: active wall time minus component time.
  // active = sum of per-iteration loop time excluding yield/sleep.
  if (this->period_active_count_ > 0) {
    uint64_t active = this->period_active_time_us_;
    uint64_t overhead = active > period_component_sum_us ? active - period_component_sum_us : 0;
    // Use double for µs→ms conversion so multi-day uptimes (where total
    // microsecond counters exceed float's ~7-digit mantissa) keep resolution.
    ESP_LOGI(TAG,
             "  main_loop: iters=%" PRIu64 ", active_avg=%.3fms, active_max=%.2fms, active_total=%.1fms, "
             "overhead_total=%.1fms",
             this->period_active_count_,
             static_cast<double>(active) / static_cast<double>(this->period_active_count_) / 1000.0,
             static_cast<double>(this->period_active_max_us_) / 1000.0, static_cast<double>(active) / 1000.0,
             static_cast<double>(overhead) / 1000.0);
    uint64_t before = this->period_before_time_us_;
    uint64_t tail = this->period_tail_time_us_;
    uint64_t accounted = before + tail;
    uint64_t inter = overhead > accounted ? overhead - accounted : 0;
    ESP_LOGI(TAG, "  main_loop_overhead_section: before=%.1fms, tail=%.1fms, inter_component=%.1fms",
             static_cast<double>(before) / 1000.0, static_cast<double>(tail) / 1000.0,
             static_cast<double>(inter) / 1000.0);
  }

  // Log total stats since boot (only for active components - idle ones haven't changed)
  ESP_LOGI(TAG, " Total stats (since boot): %zu active components", count);

  if (count > 0) {
    // Re-sort by total runtime for all-time stats
    std::sort(sorted, sorted + count, compare_total_time);

    for (size_t i = 0; i < count; i++) {
      const auto &stats = sorted[i]->runtime_stats_;
      ESP_LOGI(TAG, "  %s: count=%" PRIu32 ", avg=%.3fms, max=%.2fms, total=%.1fms",
               LOG_STR_ARG(sorted[i]->get_component_log_str()), stats.total_count,
               stats.total_count > 0 ? stats.total_time_us / (float) stats.total_count / 1000.0f : 0.0f,
               stats.total_max_time_us / 1000.0f, stats.total_time_us / 1000.0);
    }
  }

  if (this->total_active_count_ > 0) {
    uint64_t active = this->total_active_time_us_;
    uint64_t overhead = active > total_component_sum_us ? active - total_component_sum_us : 0;
    ESP_LOGI(TAG,
             "  main_loop: iters=%" PRIu64 ", active_avg=%.3fms, active_max=%.2fms, active_total=%.1fms, "
             "overhead_total=%.1fms",
             this->total_active_count_,
             static_cast<double>(active) / static_cast<double>(this->total_active_count_) / 1000.0,
             static_cast<double>(this->total_active_max_us_) / 1000.0, static_cast<double>(active) / 1000.0,
             static_cast<double>(overhead) / 1000.0);
    uint64_t before = this->total_before_time_us_;
    uint64_t tail = this->total_tail_time_us_;
    uint64_t accounted = before + tail;
    uint64_t inter = overhead > accounted ? overhead - accounted : 0;
    ESP_LOGI(TAG, "  main_loop_overhead_section: before=%.1fms, tail=%.1fms, inter_component=%.1fms",
             static_cast<double>(before) / 1000.0, static_cast<double>(tail) / 1000.0,
             static_cast<double>(inter) / 1000.0);
  }

  // Reset period stats
  for (auto *component : components) {
    component->runtime_stats_.reset_period();
  }
  this->period_active_count_ = 0;
  this->period_active_time_us_ = 0;
  this->period_active_max_us_ = 0;
  this->period_before_time_us_ = 0;
  this->period_tail_time_us_ = 0;
}

bool RuntimeStatsCollector::compare_period_time(Component *a, Component *b) {
  return a->runtime_stats_.period_time_us > b->runtime_stats_.period_time_us;
}

bool RuntimeStatsCollector::compare_total_time(Component *a, Component *b) {
  return a->runtime_stats_.total_time_us > b->runtime_stats_.total_time_us;
}

// Slow path for process_pending_stats — gate already checked by the inline
// wrapper in runtime_stats.h. Out-of-line keeps the log_stats_ machinery out
// of Application::loop().
void RuntimeStatsCollector::process_pending_stats_slow_(uint32_t current_time) {
  this->log_stats_();
  this->next_log_time_ = current_time + this->log_interval_;
}

}  // namespace runtime_stats

runtime_stats::RuntimeStatsCollector
    *global_runtime_stats =  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    nullptr;

}  // namespace esphome

#endif  // USE_RUNTIME_STATS
