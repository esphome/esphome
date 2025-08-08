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

  // Check if we have cached the name for this component
  auto name_it = this->component_names_cache_.find(component);
  if (name_it == this->component_names_cache_.end()) {
    // First time seeing this component, cache its name
    const char *source = component->get_component_source();
    this->component_names_cache_[component] = source;
    this->component_stats_[source].record_time(duration_ms);
  } else {
    this->component_stats_[name_it->second].record_time(duration_ms);
  }

  if (this->next_log_time_ == 0) {
    this->next_log_time_ = current_time + this->log_interval_;
    return;
  }
}

void RuntimeStatsCollector::log_stats_() {
  ESP_LOGI(TAG, "Component Runtime Statistics");
  ESP_LOGI(TAG, "Period stats (last %" PRIu32 "ms):", this->log_interval_);

  // First collect stats we want to display
  std::vector<ComponentStatPair> stats_to_display;

  for (const auto &it : this->component_stats_) {
    const ComponentRuntimeStats &stats = it.second;
    if (stats.get_period_count() > 0) {
      ComponentStatPair pair = {it.first, &stats};
      stats_to_display.push_back(pair);
    }
  }

  // Sort by period runtime (descending)
  std::sort(stats_to_display.begin(), stats_to_display.end(), std::greater<ComponentStatPair>());

  // Log top components by period runtime
  for (const auto &it : stats_to_display) {
    const char *source = it.name;
    const ComponentRuntimeStats *stats = it.stats;

    ESP_LOGI(TAG, "  %s: count=%" PRIu32 ", avg=%.2fms, max=%" PRIu32 "ms, total=%" PRIu32 "ms", source,
             stats->get_period_count(), stats->get_period_avg_time_ms(), stats->get_period_max_time_ms(),
             stats->get_period_time_ms());
  }

  // Log total stats since boot
  ESP_LOGI(TAG, "Total stats (since boot):");

  // Re-sort by total runtime for all-time stats
  std::sort(stats_to_display.begin(), stats_to_display.end(),
            [](const ComponentStatPair &a, const ComponentStatPair &b) {
              return a.stats->get_total_time_ms() > b.stats->get_total_time_ms();
            });

  for (const auto &it : stats_to_display) {
    const char *source = it.name;
    const ComponentRuntimeStats *stats = it.stats;

    ESP_LOGI(TAG, "  %s: count=%" PRIu32 ", avg=%.2fms, max=%" PRIu32 "ms, total=%" PRIu32 "ms", source,
             stats->get_total_count(), stats->get_total_avg_time_ms(), stats->get_total_max_time_ms(),
             stats->get_total_time_ms());
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
