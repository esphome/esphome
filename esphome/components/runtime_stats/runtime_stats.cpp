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

  if (count == 0) {
    return;
  }

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

  // Log total stats since boot (only for active components - idle ones haven't changed)
  ESP_LOGI(TAG, " Total stats (since boot): %zu active components", count);

  // Re-sort by total runtime for all-time stats
  std::sort(sorted, sorted + count, compare_total_time);

  for (size_t i = 0; i < count; i++) {
    const auto &stats = sorted[i]->runtime_stats_;
    ESP_LOGI(TAG, "  %s: count=%" PRIu32 ", avg=%.3fms, max=%.2fms, total=%.1fms",
             LOG_STR_ARG(sorted[i]->get_component_log_str()), stats.total_count,
             stats.total_count > 0 ? stats.total_time_us / (float) stats.total_count / 1000.0f : 0.0f,
             stats.total_max_time_us / 1000.0f, stats.total_time_us / 1000.0);
  }

  // Reset period stats
  for (auto *component : components) {
    component->runtime_stats_.reset_period();
  }
}

bool RuntimeStatsCollector::compare_period_time(Component *a, Component *b) {
  return a->runtime_stats_.period_time_us > b->runtime_stats_.period_time_us;
}

bool RuntimeStatsCollector::compare_total_time(Component *a, Component *b) {
  return a->runtime_stats_.total_time_us > b->runtime_stats_.total_time_us;
}

void RuntimeStatsCollector::process_pending_stats(uint32_t current_time) {
  if ((int32_t) (current_time - this->next_log_time_) >= 0) {
    this->log_stats_();
    this->next_log_time_ = current_time + this->log_interval_;
  }
}

}  // namespace runtime_stats

runtime_stats::RuntimeStatsCollector
    *global_runtime_stats =  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    nullptr;

}  // namespace esphome

#endif  // USE_RUNTIME_STATS
