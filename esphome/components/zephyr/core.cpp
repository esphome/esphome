#ifdef USE_ZEPHYR

#include <zephyr/kernel.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/sys/reboot.h>

namespace esphome {

static int wdt_channel_id = -EINVAL;
const device *wdt = nullptr;

void yield() { ::k_yield(); }
uint32_t millis() { return k_ticks_to_ms_floor32(k_uptime_ticks()); }
uint32_t micros() { return k_ticks_to_us_floor32(k_uptime_ticks()); }
void delayMicroseconds(uint32_t us) { ::k_usleep(us); }
void delay(uint32_t ms) { ::k_msleep(ms); }

void arch_init() {
  wdt = DEVICE_DT_GET(DT_ALIAS(watchdog0));

  if (device_is_ready(wdt)) {
    static wdt_timeout_cfg wdt_config{};
    wdt_config.flags = WDT_FLAG_RESET_SOC;
    wdt_config.window.max = 2000;
    wdt_channel_id = wdt_install_timeout(wdt, &wdt_config);
    if (wdt_channel_id >= 0) {
      wdt_setup(wdt, WDT_OPT_PAUSE_HALTED_BY_DBG | WDT_OPT_PAUSE_IN_SLEEP);
    }
  }
}

void arch_feed_wdt() {
  if (wdt_channel_id >= 0) {
    wdt_feed(wdt, wdt_channel_id);
  }
}

void arch_restart() { sys_reboot(SYS_REBOOT_COLD); }
uint32_t arch_get_cpu_cycle_count() { return k_cycle_get_32(); }
uint32_t arch_get_cpu_freq_hz() { return sys_clock_hw_cycles_per_sec(); }
uint8_t progmem_read_byte(const uint8_t *addr) { return *addr; }

}  // namespace esphome

void setup();
void loop();

int main() {
  setup();
  while (true) {
    loop();
    esphome::yield();
  }
  return 0;
}

#endif
