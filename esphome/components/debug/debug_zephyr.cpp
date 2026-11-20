#include "debug_component.h"
#ifdef USE_ZEPHYR
#include <cinttypes>
#include <climits>
#include "esphome/core/log.h"
#include <esphome/components/zephyr/reset_reason.h>
#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>

namespace esphome::debug {

ESPHOME_LOG_TAG(TAG, "debug");

const char *DebugComponent::get_reset_reason_(std::span<char, RESET_REASON_BUFFER_SIZE> buffer) {
  const char *buf = zephyr::get_reset_reason(buffer);
  ESP_LOGD(TAG, "Reset Reason: %s", buf);
  return buf;
}

const char *DebugComponent::get_wakeup_cause_(std::span<char, WAKEUP_CAUSE_BUFFER_SIZE> buffer) {
  // Zephyr doesn't have detailed wakeup cause like ESP32
  return "";
}

uint32_t DebugComponent::get_free_heap_() { return INT_MAX; }

static void fa_cb(const struct flash_area *fa, void *user_data) {
#if CONFIG_FLASH_MAP_LABELS
  const char *fa_label = flash_area_label(fa);

  if (fa_label == nullptr) {
    fa_label = "-";
  }
  ESP_LOGCONFIG(TAG, "%2d   0x%0*" PRIxPTR "   %-26s  %-24.24s  0x%-10x 0x%-12x", (int) fa->fa_id,
                sizeof(uintptr_t) * 2, (uintptr_t) fa->fa_dev, fa->fa_dev->name, fa_label, (uint32_t) fa->fa_off,
                fa->fa_size);
#else
  ESP_LOGCONFIG(TAG, "%2d   0x%0*" PRIxPTR "   %-26s  0x%-10x 0x%-12x", (int) fa->fa_id, sizeof(uintptr_t) * 2,
                (uintptr_t) fa->fa_dev, fa->fa_dev->name, (uint32_t) fa->fa_off, fa->fa_size);
#endif
}

void DebugComponent::log_partition_info_() {
#if CONFIG_FLASH_MAP_LABELS
  ESP_LOGCONFIG(TAG, "ID | Device     | Device Name               "
                     "| Label                   | Offset     | Size");
  ESP_LOGCONFIG(TAG, "--------------------------------------------"
                     "-----------------------------------------------");
#else
  ESP_LOGCONFIG(TAG, "ID | Device     | Device Name               "
                     "| Offset     | Size");
  ESP_LOGCONFIG(TAG, "-----------------------------------------"
                     "------------------------------");
#endif
  flash_area_foreach(fa_cb, nullptr);
}

// The Python side enables the statistics only for a verbose logger
#ifdef CONFIG_SCHED_THREAD_USAGE_ANALYSIS
static void log_thread_usage(const struct k_thread *thread, void *user_data) {
  const auto cpu_cycles = *static_cast<const uint64_t *>(user_data);
  auto *tid = const_cast<k_tid_t>(thread);  // NOLINT(cppcoreguidelines-pro-type-const-cast)
  k_thread_runtime_stats_t stats;
  if (k_thread_runtime_stats_get(tid, &stats) != 0)
    return;
  char state[32];
  const char *name = k_thread_name_get(tid);
  ESP_LOGV(TAG, "  %-16s prio %3d %-10s %3u%% peak %" PRIu32 " avg %" PRIu32, name != nullptr ? name : "?",
           thread->base.prio, k_thread_state_str(tid, state, sizeof(state)),
           static_cast<unsigned>(stats.execution_cycles * 100 / cpu_cycles),
           static_cast<uint32_t>(k_cyc_to_us_floor64(stats.peak_cycles)),
           static_cast<uint32_t>(k_cyc_to_us_floor64(stats.average_cycles)));
}
#endif

void DebugComponent::update_platform_() {
#ifdef CONFIG_SCHED_THREAD_USAGE_ANALYSIS
  k_thread_runtime_stats_t cpu;
  if (k_thread_runtime_stats_all_get(&cpu) != 0 || cpu.execution_cycles == 0)
    return;
  ESP_LOGV(TAG, "Thread CPU usage since boot (peak and average us per time slice):");
  // The unlocked walk lets the log output inside the callback block
  k_thread_foreach_unlocked(log_thread_usage, &cpu.execution_cycles);
#endif
}

}  // namespace esphome::debug
#endif
