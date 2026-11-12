#ifdef USE_ZEPHYR

#include "esphome/core/defines.h"
#include "esphome/core/hal.h"

#include <zephyr/drivers/watchdog.h>
#include <zephyr/sys/reboot.h>

#if CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC != 1000000 && defined(CONFIG_CPU_CORTEX_M_HAS_DWT)
#define USE_ZEPHYR_DWT_MICROS
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

// yield(), delay(), millis(), millis_64(), delayMicroseconds(), micros() on a 1 MHz system timer,
// arch_get_cpu_cycle_count(), arch_get_cpu_freq_hz() inlined in
// components/zephyr/hal.h.

#ifdef USE_ZEPHYR_DWT_MICROS
// The kernel clock counts in ticks of about 30 us, and the CPU cycle counter stops while the CPU sleeps.
// micros() adds the cycles counted since the last call and keeps the result within one tick of the kernel clock.
// One struct so the code loads a single base address.
static struct {
  uint32_t value;
  uint32_t last_cycles;  // Moves on by whole microseconds only, so the remainder carries over.
} micros_state;          // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static inline uint32_t cycles_per_us() {
#ifdef CONFIG_SOC_SERIES_NRF52X
  return 64;  // nRF52 cores always run at 64 MHz, so the division below becomes a shift.
#else
  return SystemCoreClock / 1000000U;
#endif
}

uint32_t micros() {
  const uint32_t per_us = cycles_per_us();
  const unsigned int key = irq_lock();
  const uint32_t cycles = DWT->CYCCNT;
  const uint32_t elapsed_us = (cycles - micros_state.last_cycles) / per_us;
  micros_state.last_cycles += elapsed_us * per_us;
  uint32_t value = micros_state.value + elapsed_us;

  const uint32_t kernel_us = k_ticks_to_us_floor32(k_uptime_ticks());
  const uint32_t tick_us = k_ticks_to_us_ceil32(1);
  const int32_t ahead = static_cast<int32_t>(value - kernel_us);
  if (ahead < 0) {
    // The CPU slept, so the cycle counter fell behind.
    value = kernel_us;
    micros_state.last_cycles = cycles;
  } else if (static_cast<uint32_t>(ahead) >= tick_us) {
    value = kernel_us + tick_us - 1;
  }
  micros_state.value = value;
  irq_unlock(key);
  return value;
}
#elif CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC != 1000000
uint32_t micros() { return k_ticks_to_us_floor32(k_uptime_ticks()); }
#endif

void arch_init() {
  main_loop_thread() = k_current_get();
#ifdef USE_ZEPHYR_DWT_MICROS
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
