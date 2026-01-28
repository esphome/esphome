#include "runtime_stats.h"

#ifdef USE_RUNTIME_STATS

#include "esphome/core/component.h"
#include <algorithm>

namespace esphome {

namespace runtime_stats {

RuntimeStatsCollector::RuntimeStatsCollector() : log_interval_(60000), next_log_time_(0) {
  global_runtime_stats = this;
}

void RuntimeStatsCollector::record_component_time(Component *component, uint32_t duration_ms, uint32_t current_time) {
  if (component == nullptr)
    return;

  // Record stats using component pointer as key
  this->component_stats_[component].record_time(duration_ms);

  if (this->next_log_time_ == 0) {
    this->next_log_time_ = current_time + this->log_interval_;
    return;
  }
}

void RuntimeStatsCollector::log_stats_() {
  // First pass: count active components
  size_t count = 0;
  for (const auto &it : this->component_stats_) {
    if (it.second.get_period_count() > 0) {
      count++;
    }
  }

  ESP_LOGI(TAG,
           "Component Runtime Statistics\n"
           " Period stats (last %" PRIu32 "ms): %zu active components",
           this->log_interval_, count);

  if (count == 0) {
    return;
  }

  // Stack buffer sized to actual active count (up to 256 components), heap fallback for larger
  SmallBufferWithHeapFallback<256, Component *> buffer(count);
  Component **sorted = buffer.get();

  // Second pass: fill buffer with active components
  size_t idx = 0;
  for (const auto &it : this->component_stats_) {
    if (it.second.get_period_count() > 0) {
      sorted[idx++] = it.first;
    }
  }

  // Sort by period runtime (descending)
  std::sort(sorted, sorted + count, [this](Component *a, Component *b) {
    return this->component_stats_[a].get_period_time_ms() > this->component_stats_[b].get_period_time_ms();
  });

  // Log top components by period runtime
  for (size_t i = 0; i < count; i++) {
    const auto &stats = this->component_stats_[sorted[i]];
    ESP_LOGI(TAG, "  %s: count=%" PRIu32 ", avg=%.2fms, max=%" PRIu32 "ms, total=%" PRIu32 "ms",
             LOG_STR_ARG(sorted[i]->get_component_log_str()), stats.get_period_count(), stats.get_period_avg_time_ms(),
             stats.get_period_max_time_ms(), stats.get_period_time_ms());
  }

  // Log total stats since boot (only for active components - idle ones haven't changed)
  ESP_LOGI(TAG, " Total stats (since boot): %zu active components", count);

  // Re-sort by total runtime for all-time stats
  std::sort(sorted, sorted + count, [this](Component *a, Component *b) {
    return this->component_stats_[a].get_total_time_ms() > this->component_stats_[b].get_total_time_ms();
  });

  for (size_t i = 0; i < count; i++) {
    const auto &stats = this->component_stats_[sorted[i]];
    ESP_LOGI(TAG, "  %s: count=%" PRIu32 ", avg=%.2fms, max=%" PRIu32 "ms, total=%" PRIu32 "ms",
             LOG_STR_ARG(sorted[i]->get_component_log_str()), stats.get_total_count(), stats.get_total_avg_time_ms(),
             stats.get_total_max_time_ms(), stats.get_total_time_ms());
  }
}

void RuntimeStatsCollector::process_pending_stats(uint32_t current_time) {
  if (this->next_log_time_ == 0)
    return;

  if (current_time >= this->next_log_time_) {
    this->log_stats_();
    this->reset_stats_();
    this->next_log_time_ = current_time + this->log_interval_;
  }
}

}  // namespace runtime_stats

runtime_stats::RuntimeStatsCollector *global_runtime_stats =
    nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome

#endif  // USE_RUNTIME_STATS
