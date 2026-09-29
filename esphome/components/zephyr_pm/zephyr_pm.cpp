#include "zephyr_pm.h"
#ifdef USE_ZEPHYR
#include "esphome/core/log.h"

namespace esphome::zephyr_pm {

static const char *const TAG = "zephyr_pm";

void ZephyrPowerManagement::dump_config() {
  ESP_LOGCONFIG(TAG, "Power Management:");
#if CONFIG_PM
  ESP_LOGCONFIG(TAG, "  Light Sleep: enabled (CONFIG_PM)");
#else
  ESP_LOGCONFIG(TAG, "  Light Sleep: unavailable -- idle-halt only (no CONFIG_PM on this board)");
#endif
#if CONFIG_PM_DEVICE_RUNTIME
  ESP_LOGCONFIG(TAG, "  Device Power Down: requested (usage-based runtime PM) -- only devices "
                     "whose driver supports it are affected");
#elif CONFIG_PM_DEVICE_SYSTEM_MANAGED
  ESP_LOGCONFIG(TAG, "  Device Power Down: requested (system-managed) -- only devices whose "
                     "driver supports it are affected");
#endif
#if CONFIG_PM_STATS
  ESP_LOGCONFIG(TAG, "  PM Stats Enabled");
#endif
#if CONFIG_CPU_FREQ_POLICY_ON_DEMAND
  ESP_LOGCONFIG(TAG, "  CPU Frequency Policy: on-demand");
#elif CONFIG_CPU_FREQ_POLICY_PRESSURE
  ESP_LOGCONFIG(TAG, "  CPU Frequency Policy: pressure");
#endif
#if CONFIG_CPU_FREQ_LOG_LEVEL_DBG
  ESP_LOGCONFIG(TAG, "  CPU Frequency Stats Enabled");
#endif
}

}  // namespace esphome::zephyr_pm
#endif
