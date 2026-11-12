#ifdef USE_ZEPHYR

#include "esphome/core/defines.h"
#include "esphome/core/hal.h"

#include <zephyr/drivers/watchdog.h>
#include <zephyr/sys/reboot.h>

#if CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC != 1000000 && defined(CONFIG_SOC_SERIES_NRF52X)
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

// yield(), delay(), millis(), millis_64(), delayMicroseconds(), micros() except the cycle-counter one,
// arch_get_cpu_cycle_count(), arch_get_cpu_freq_hz() inlined in
// components/zephyr/hal.h.

#ifdef USE_ZEPHYR_DWT_MICROS
// The kernel clock counts in ticks of about 30 us, and the CPU cycle counter stops while the CPU sleeps.
// micros() adds the cycles counted since the last call. While the CPU stays busy the result and the kernel clock
// each trail the true time by less than a tick, so they differ by less than a tick either way. A sleep shorter
// than a tick can go unnoticed, so the result may trail the true time by up to two ticks until the next resync.
// The CPU cannot sleep while interrupts are locked, so there only a lag from an earlier sleep is corrected: a
// pulse timed under InterruptLock never sees a resync in the middle.
// One struct so the code loads a single base address.
static struct {
  uint32_t value;
  uint32_t last_cycles;  // Moves on by whole microseconds only, so the remainder carries over.
} micros_state;          // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static constexpr uint32_t CYCLES_PER_US = 64;  // nRF52 cores always run at 64 MHz.

uint32_t micros() {
  const unsigned int key = irq_lock();
  const uint32_t cycles = DWT->CYCCNT;
  const uint32_t elapsed_us = (cycles - micros_state.last_cycles) / CYCLES_PER_US;
  micros_state.last_cycles += elapsed_us * CYCLES_PER_US;
  uint32_t value = micros_state.value + elapsed_us;

  const uint32_t kernel_us = k_ticks_to_us_floor32(k_uptime_ticks());
  // A tick, plus 1 us because kernel_us is rounded down.
  const int32_t tick_us = k_ticks_to_us_ceil32(1) + 1;
  const int32_t ahead = static_cast<int32_t>(value - kernel_us);
  const int32_t max_lag_us = arch_irq_unlocked(key) ? tick_us : 4 * tick_us;
  if (ahead < -max_lag_us) {
    // The CPU slept, so the cycle counter fell behind.
    value = kernel_us;
    micros_state.last_cycles = cycles;
  } else if (ahead >= tick_us) {
    // The CPU clock can run fast against the RTC; cap the drift at one tick.
    value = kernel_us + tick_us - 1;
  }
  micros_state.value = value;
  irq_unlock(key);
  return value;
}
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
