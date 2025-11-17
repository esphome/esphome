#ifdef USE_ZEPHYR

#include <zephyr/kernel.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/random/random.h>
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/defines.h"

namespace esphome {

static int wdt_channel_id = -1;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static const device *const WDT = DEVICE_DT_GET(DT_ALIAS(watchdog0));

void yield() { ::k_yield(); }
uint32_t millis() { return k_ticks_to_ms_floor32(k_uptime_ticks()); }
uint32_t micros() { return k_ticks_to_us_floor32(k_uptime_ticks()); }
void delayMicroseconds(uint32_t us) { ::k_usleep(us); }
void delay(uint32_t ms) { ::k_msleep(ms); }

void arch_init() {
  if (device_is_ready(WDT)) {
    static wdt_timeout_cfg wdt_config{};
    wdt_config.flags = WDT_FLAG_RESET_SOC;
    wdt_config.window.max = 2000;
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
}

void arch_feed_wdt() {
  if (wdt_channel_id >= 0) {
    wdt_feed(WDT, wdt_channel_id);
  }
}

void arch_restart() { sys_reboot(SYS_REBOOT_COLD); }
uint32_t arch_get_cpu_cycle_count() { return k_cycle_get_32(); }
uint32_t arch_get_cpu_freq_hz() { return sys_clock_hw_cycles_per_sec(); }
uint8_t progmem_read_byte(const uint8_t *addr) { return *addr; }

Mutex::Mutex() {
  auto *mutex = new k_mutex();
  this->handle_ = mutex;
  k_mutex_init(mutex);
}
Mutex::~Mutex() { delete static_cast<k_mutex *>(this->handle_); }
void Mutex::lock() { k_mutex_lock(static_cast<k_mutex *>(this->handle_), K_FOREVER); }
bool Mutex::try_lock() { return k_mutex_lock(static_cast<k_mutex *>(this->handle_), K_NO_WAIT) == 0; }
void Mutex::unlock() { k_mutex_unlock(static_cast<k_mutex *>(this->handle_)); }

IRAM_ATTR InterruptLock::InterruptLock() { state_ = irq_lock(); }
IRAM_ATTR InterruptLock::~InterruptLock() { irq_unlock(state_); }

// Zephyr doesn't support lwIP core locking, so this is a no-op
LwIPLock::LwIPLock() {}
LwIPLock::~LwIPLock() {}

uint32_t random_uint32() { return rand(); }  // NOLINT(cert-msc30-c, cert-msc50-cpp)
bool random_bytes(uint8_t *data, size_t len) {
  sys_rand_get(data, len);
  return true;
}

void get_mac_address_raw(uint8_t *mac) {  // NOLINT(readability-non-const-parameter)
  mac[0] = ((NRF_FICR->DEVICEADDR[1] & 0xFFFF) >> 8) | 0xC0;
  mac[1] = NRF_FICR->DEVICEADDR[1] & 0xFFFF;
  mac[2] = NRF_FICR->DEVICEADDR[0] >> 24;
  mac[3] = NRF_FICR->DEVICEADDR[0] >> 16;
  mac[4] = NRF_FICR->DEVICEADDR[0] >> 8;
  mac[5] = NRF_FICR->DEVICEADDR[0];
}

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
