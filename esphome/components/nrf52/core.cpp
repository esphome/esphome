#ifdef USE_NRF52

#include <Arduino.h>
#include "Adafruit_nRFCrypto.h"
#include "nrfx_wdt.h"

namespace esphome {

void yield() { ::yield(); }
uint32_t millis() { return ::millis(); }
void delay(uint32_t ms) { ::delay(ms); }
uint32_t micros() { return ::micros(); }

struct nrf5x_wdt_obj
{
    nrfx_wdt_t wdt;
    nrfx_wdt_channel_id ch;
};
static nrfx_wdt_config_t nrf5x_wdt_cfg = NRFX_WDT_DEFAULT_CONFIG;

static struct nrf5x_wdt_obj nrf5x_wdt = {
    .wdt = NRFX_WDT_INSTANCE(0),
};

void arch_init() {
    //Configure WDT.
    nrfx_wdt_init(&nrf5x_wdt.wdt, &nrf5x_wdt_cfg, nullptr);
    nrfx_wdt_channel_alloc(&nrf5x_wdt.wdt, &nrf5x_wdt.ch);
    nrfx_wdt_enable(&nrf5x_wdt.wdt);

    nRFCrypto.begin();
    // Init random seed
    union seedParts {
        uint32_t seed32;
        uint8_t seed8[4];
    } seed;
    nRFCrypto.Random.generate(seed.seed8, sizeof(seed.seed8));
    randomSeed(seed.seed32);

}
void arch_feed_wdt() {
    nrfx_wdt_feed(&nrf5x_wdt.wdt);
}

void arch_restart() { /* TODO */ }

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

#endif  // USE_RP2040
