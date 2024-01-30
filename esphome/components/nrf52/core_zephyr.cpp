#ifdef USE_NRF52
#ifdef USE_ZEPHYR

#include <stdlib.h>
#include <zephyr/kernel.h>

namespace esphome {
void yield() { ::k_yield(); }
uint32_t millis() { return k_ticks_to_ms_floor32(k_uptime_ticks()); }
void delay(uint32_t ms) { ::k_msleep(ms); }
uint32_t micros() { return k_ticks_to_us_floor32(k_uptime_ticks()); }

void arch_init() {
    // TODO
}
void arch_feed_wdt() {
    // TODO
}

void arch_restart() {
    // TODO
}

void nrf52GetMacAddr(uint8_t *mac)
{
    const uint8_t *src = (const uint8_t *)NRF_FICR->DEVICEADDR;
    mac[5] = src[0];
    mac[4] = src[1];
    mac[3] = src[2];
    mac[2] = src[3];
    mac[1] = src[4];
    mac[0] = src[5] | 0xc0; // MSB high two bits get set elsewhere in the bluetooth stack
}

}  // namespace esphome

void setup();
void loop();

int main(){
    setup();
    while(1) {
        loop();
        esphome::yield();
    }
    return 0;
}

#endif
#endif  // USE_RP2040
