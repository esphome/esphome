#ifdef USE_NRF52

#include <Arduino.h>
#include "Adafruit_nRFCrypto.h"

namespace esphome {

void yield() { ::yield(); }
uint32_t millis() { return ::millis(); }
void delay(uint32_t ms) { ::delay(ms); }
uint32_t micros() { return ::micros(); }

void arch_init() {
	 nRFCrypto.begin();
    // Init random seed
    union seedParts {
        uint32_t seed32;
        uint8_t seed8[4];
    } seed;
    nRFCrypto.Random.generate(seed.seed8, sizeof(seed.seed8));
    randomSeed(seed.seed32);

}
void arch_feed_wdt() { /* TODO */ }

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
