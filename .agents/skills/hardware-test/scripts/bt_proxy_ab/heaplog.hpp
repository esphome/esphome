// Heap lines for a proxy A/B: every 30 s internal and total (free, low-water mark, largest
// block), and a HEAPMIN line whenever the internal low-water mark drops by 1 KB or more (checked every 100 ms).
#pragma once
#include <esp_heap_caps.h>
#include <esp_idf_version.h>
#include "esphome/core/log.h"
namespace heaplog {
static size_t last_min = SIZE_MAX;
inline void log() {
  ESP_LOGI("heaplog", "HEAP free=%u min=%u largest=%u total_free=%u total_min=%u total_largest=%u idf=%s",
           (unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned) heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
           (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
           (unsigned) heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
           (unsigned) heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT),
           (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT), esp_get_idf_version());
}
inline void watch() {
  size_t m = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
  if (last_min == SIZE_MAX || last_min >= m + 1024) {
    last_min = m;
    ESP_LOGI("heaplog", "HEAPMIN min=%u free=%u largest=%u total_min=%u total_largest=%u", (unsigned) m,
             (unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
             (unsigned) heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT),
             (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
  }
}
}  // namespace heaplog
