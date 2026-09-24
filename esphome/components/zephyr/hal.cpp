#ifdef USE_ZEPHYR

#include "esphome/core/defines.h"
#include "esphome/core/hal.h"

#include <zephyr/drivers/watchdog.h>
#include <zephyr/sys/reboot.h>

// Empty zephyr namespace block to satisfy ci-custom's lint_namespace check.
// HAL functions live in namespace esphome (root) — they are not part of the
// zephyr component's API.
namespace esphome::zephyr {}  // namespace esphome::zephyr

namespace esphome {

#ifdef CONFIG_WATCHDOG
static int wdt_channel_id = -1;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static const device *const WDT = DEVICE_DT_GET(DT_ALIAS(watchdog0));
#endif

// yield(), delay(), micros(), millis(), millis_64(), delayMicroseconds(),
// arch_get_cpu_cycle_count(), arch_get_cpu_freq_hz() inlined in
// components/zephyr/hal.h.

void arch_init() {
#ifdef CONFIG_WATCHDOG
  if (device_is_ready(WDT)) {
    static wdt_timeout_cfg wdt_config{};
    wdt_config.flags = WDT_FLAG_RESET_SOC;
#ifndef USE_BOOTLOADER_MCUBOOT
    // Adafruit bootloader doesn't feed the WDT while
    // erasing flash during a firmware update, so a shorter timeout can break the update.
    wdt_config.window.max = 30000;
#elif defined(USE_ZIGBEE)
    // zboss thread uses a lot of CPU cycles during startup
    wdt_config.window.max = 10000;
#else
    wdt_config.window.max = 2000;
#endif
    wdt_channel_id = wdt_install_timeout(WDT, &wdt_config);
    if (wdt_channel_id >= 0) {
      uint8_t options = 0;
#ifdef USE_DEBUG
      options |= WDT_OPT_PAUSE_HALTED_BY_DBG;
#endif
#ifdef USE_DEEP_SLEEP
      options |= WDT_OPT_PAUSE_IN_SLEEP;
#endif
      wdt_setup(WDT, options);
    }
  }
#endif
  // feed watchdog early. Otherwise OTA may rollback.
  arch_feed_wdt();
}

void arch_feed_wdt() {
#ifdef CONFIG_WATCHDOG
  if (wdt_channel_id >= 0) {
    wdt_feed(WDT, wdt_channel_id);
  }
#endif
}

void arch_restart() { sys_reboot(SYS_REBOOT_COLD); }

// Always UNKNOWN for now: nRF52's RESETREAS (via hwinfo, see zephyr/reset_reason.cpp) has no power-on bit, so a cold
// boot and a brownout both read as no flags, and it accumulates until cleared. Unverified on hardware.
ResetCause arch_get_reset_cause() { return ResetCause::RESET_CAUSE_UNKNOWN; }

}  // namespace esphome

#endif  // USE_ZEPHYR
