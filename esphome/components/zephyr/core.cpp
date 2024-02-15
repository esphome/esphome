#ifdef USE_ZEPHYR

#include <zephyr/kernel.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/sys/reboot.h>

namespace esphome {

static int wdt_channel_id = -EINVAL;
const device * wdt = nullptr;

void yield() { ::k_yield(); }
uint32_t millis() { return k_ticks_to_ms_floor32(k_uptime_ticks()); }
void delay(uint32_t ms) { ::k_msleep(ms); }
uint32_t micros() { return k_ticks_to_us_floor32(k_uptime_ticks()); }

void arch_init() {
  wdt = DEVICE_DT_GET(DT_ALIAS(watchdog0));

  if (device_is_ready(wdt)) {
    static wdt_timeout_cfg wdt_config{};
    wdt_config.flags = WDT_FLAG_RESET_SOC;
    wdt_config.window.max = 2000;
    wdt_channel_id = wdt_install_timeout(wdt, &wdt_config);
    if (wdt_channel_id >= 0) {
      wdt_setup(wdt, WDT_OPT_PAUSE_HALTED_BY_DBG);
    }
  }
}

void arch_feed_wdt() {
  if (wdt_channel_id >= 0) {
    wdt_feed(wdt, wdt_channel_id);
  }
}

void arch_restart() { sys_reboot(SYS_REBOOT_COLD); }

}  // namespace esphome

void setup();
void loop();

int main() {
  setup();
  while (1) {
    loop();
    esphome::yield();
  }
  return 0;
}

#endif
