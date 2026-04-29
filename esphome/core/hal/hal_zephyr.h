#pragma once

#ifdef USE_ZEPHYR

#include <cstdint>

#define IRAM_ATTR
#define PROGMEM

namespace esphome {

/// Returns true when executing inside an interrupt handler.
/// Zephyr/nRF52: not currently consulted — wake path is platform-specific.
__attribute__((always_inline)) inline bool in_isr_context() { return false; }

void yield();
void delay(uint32_t ms);
uint32_t micros();
uint32_t millis();
uint64_t millis_64();

}  // namespace esphome

#endif  // USE_ZEPHYR
