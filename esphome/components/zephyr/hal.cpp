#ifdef USE_ZEPHYR

#include "esphome/core/defines.h"
#include "esphome/core/hal.h"

#include <zephyr/drivers/watchdog.h>
#include <zephyr/sys/reboot.h>

#ifdef CONFIG_CPU_CORTEX_M_HAS_DWT
#include <cmsis_core.h>
#endif

// Empty zephyr namespace block to satisfy ci-custom's lint_namespace check.
// HAL functions live in namespace esphome (root) — they are not part of the
// zephyr component's API.
namespace esphome::zephyr {}  // namespace esphome::zephyr

namespace esphome {

#ifdef CONFIG_WATCHDOG
static int wdt_channel_id = -1;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static const device *const WDT = DEVICE_DT_GET(DT_ALIAS(watchdog0));
#endif

// yield(), delay(), millis(), millis_64(), delayMicroseconds(),
// arch_get_cpu_cycle_count(), arch_get_cpu_freq_hz() inlined in
// components/zephyr/hal.h.

#ifdef CONFIG_CPU_CORTEX_M_HAS_DWT
// The kernel clock counts in ticks of about 30 us, and the CPU cycle counter stops while the CPU sleeps.
// micros() adds the cycles counted since the last call and keeps the result within one tick of the kernel clock.
static uint32_t micros_value;        // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static uint32_t micros_last_cycles;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static uint32_t micros_cycles_left;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

uint32_t micros() {
  const unsigned int key = irq_lock();
  const uint32_t cycles = DWT->CYCCNT;
  const uint32_t cycles_per_us = SystemCoreClock / 1000000U;
  const uint32_t elapsed = cycles - micros_last_cycles + micros_cycles_left;
  micros_last_cycles = cycles;
  micros_cycles_left = elapsed % cycles_per_us;
  uint32_t value = micros_value + elapsed / cycles_per_us;

  const uint32_t kernel_us = k_ticks_to_us_floor32(k_uptime_ticks());
  const uint32_t tick_us = k_ticks_to_us_ceil32(1);
  const int32_t ahead = static_cast<int32_t>(value - kernel_us);
  if (ahead < 0) {
    // The CPU slept, so the cycle counter fell behind.
    value = kernel_us;
    micros_cycles_left = 0;
  } else if (static_cast<uint32_t>(ahead) >= tick_us) {
    value = kernel_us + tick_us - 1;
  }
  micros_value = value;
  irq_unlock(key);
  return value;
}
#else
uint32_t micros() { return k_ticks_to_us_floor32(k_uptime_ticks()); }
#endif

void arch_init() {
  main_loop_thread() = k_current_get();
#ifdef CONFIG_CPU_CORTEX_M_HAS_DWT
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
#endif
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

}  // namespace esphome

#endif  // USE_ZEPHYR
