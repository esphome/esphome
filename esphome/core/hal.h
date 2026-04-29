#pragma once
#include <string>
#include <cstdint>
#include "gpio.h"
#include "esphome/core/defines.h"
#include "esphome/core/time_64.h"
#include "esphome/core/time_conversion.h"

// Per-platform HAL bits (IRAM_ATTR / PROGMEM macros, in_isr_context(),
// inline yield/delay/micros/millis/millis_64 wrappers, ESP8266 progmem
// helpers) live under esphome/core/hal/ and are dispatched here based on
// the active USE_* platform define. Each header guards its body with the
// matching #ifdef USE_<platform> and re-enters namespace esphome {} so it
// is safe to be re-included.
#if defined(USE_ESP32)
#include "esphome/core/hal/hal_esp32.h"
#elif defined(USE_ESP8266)
#include "esphome/core/hal/hal_esp8266.h"
#elif defined(USE_LIBRETINY)
#include "esphome/core/hal/hal_libretiny.h"
#elif defined(USE_RP2040)
#include "esphome/core/hal/hal_rp2040.h"
#elif defined(USE_HOST)
#include "esphome/core/hal/hal_host.h"
#elif defined(USE_ZEPHYR)
#include "esphome/core/hal/hal_zephyr.h"
#else
#error "hal.h: not implemented for this platform"
#endif

namespace esphome {

// Cross-platform declarations. delayMicroseconds(), arch_feed_wdt(),
// arch_get_cpu_cycle_count() vary per platform (some inline, some
// out-of-line) so they live in hal/hal_<platform>.h.
void __attribute__((noreturn)) arch_restart();
void arch_init();
uint32_t arch_get_cpu_freq_hz();

#ifndef USE_ESP8266
// All non-ESP8266 platforms: PROGMEM is a no-op, so these are direct dereferences.
// ESP8266's out-of-line declarations live in hal/hal_esp8266.h.
inline uint8_t progmem_read_byte(const uint8_t *addr) { return *addr; }
inline const char *progmem_read_ptr(const char *const *addr) { return *addr; }
inline uint16_t progmem_read_uint16(const uint16_t *addr) { return *addr; }
#endif

}  // namespace esphome
