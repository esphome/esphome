#include "setup_heap_stats.h"

#ifdef USE_SETUP_HEAP_STATS

#include <algorithm>
#include "esphome/core/component.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32
#include <esp_heap_caps.h>
#endif
#ifdef USE_ESP8266
#include <Esp.h>
#endif
#ifdef USE_RP2040
#include <Arduino.h>
#endif

namespace esphome {

namespace setup_heap_stats {

static const char *const TAG = "setup_heap_stats";

SetupHeapStatsCollector::SetupHeapStatsCollector(uint32_t initial_baseline) {
  global_setup_heap_stats = this;
  uint32_t before_alloc = get_free_internal_heap();
  this->entries_ = new ComponentHeapEntry[ESPHOME_COMPONENT_COUNT];  // NOLINT
  uint32_t after_alloc = get_free_internal_heap();
  // Use the pre_setup() baseline but subtract our own entries allocation
  // so the collector's overhead is not attributed to the first component
  this->last_heap_snapshot_ = initial_baseline - (before_alloc - after_alloc);
}

uint32_t SetupHeapStatsCollector::get_free_internal_heap() {
#ifdef USE_ESP32
  return heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
#elif defined(USE_ESP8266)
  return ESP.getFreeHeap();  // NOLINT(readability-static-accessed-through-instance)
#elif defined(USE_RP2040)
  return rp2040.getFreeHeap();
#elif defined(USE_LIBRETINY)
  return lt_heap_get_free();
#else
  return 0;
#endif
}

void SetupHeapStatsCollector::record_component_registered(Component *comp) {
  uint32_t current_heap = get_free_internal_heap();
  int32_t delta = static_cast<int32_t>(this->last_heap_snapshot_) - static_cast<int32_t>(current_heap);
  this->last_heap_snapshot_ = current_heap;

  if (this->entry_count_ < ESPHOME_COMPONENT_COUNT) {
    this->entries_[this->entry_count_].component = comp;
    this->entries_[this->entry_count_].construction_delta = delta;
    this->entries_[this->entry_count_].setup_delta = 0;
    this->entry_count_++;
  }

  ESP_LOGI(TAG, "Constructed %s: %" PRId32 " bytes (free: %" PRIu32 ")", LOG_STR_ARG(comp->get_component_log_str()),
           delta, current_heap);
}

void SetupHeapStatsCollector::record_before_setup(Component *comp) {
  this->setup_component_ = comp;
  this->setup_before_heap_ = get_free_internal_heap();
}

void SetupHeapStatsCollector::record_after_setup(Component *comp) {
  if (this->setup_component_ != comp)
    return;

  uint32_t current_heap = get_free_internal_heap();
  int32_t delta = static_cast<int32_t>(this->setup_before_heap_) - static_cast<int32_t>(current_heap);

  // Find the entry for this component and update setup delta
  for (uint16_t i = 0; i < this->entry_count_; i++) {
    if (this->entries_[i].component == comp) {
      this->entries_[i].setup_delta = delta;
      break;
    }
  }

  ESP_LOGI(TAG, "Setup %s: %" PRId32 " bytes (free: %" PRIu32 ")", LOG_STR_ARG(comp->get_component_log_str()), delta,
           current_heap);

  this->setup_component_ = nullptr;
}

void SetupHeapStatsCollector::log_summary() {
  if (this->entries_ == nullptr || this->entry_count_ == 0)
    return;

  // Sort by total (construction + setup) descending
  std::sort(this->entries_, this->entries_ + this->entry_count_,
            [](const ComponentHeapEntry &a, const ComponentHeapEntry &b) {
              return (a.construction_delta + a.setup_delta) > (b.construction_delta + b.setup_delta);
            });

  ESP_LOGI(TAG, "Setup Heap Stats Summary (sorted by total, construction + setup):");
  for (uint16_t i = 0; i < this->entry_count_; i++) {
    const auto &entry = this->entries_[i];
    int32_t total = entry.construction_delta + entry.setup_delta;
    if (total == 0 && entry.construction_delta == 0 && entry.setup_delta == 0)
      continue;
    ESP_LOGI(TAG, "  %s: %" PRId32 " bytes (construction: %" PRId32 ", setup: %" PRId32 ")",
             LOG_STR_ARG(entry.component->get_component_log_str()), total, entry.construction_delta, entry.setup_delta);
  }

  // Free storage
  delete[] this->entries_;  // NOLINT
  this->entries_ = nullptr;
  this->entry_count_ = 0;
}

}  // namespace setup_heap_stats

setup_heap_stats::SetupHeapStatsCollector *global_setup_heap_stats =
    nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome

#endif  // USE_SETUP_HEAP_STATS
