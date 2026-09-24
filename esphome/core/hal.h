#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include "gpio.h"
#include "esphome/core/defines.h"
#include "esphome/core/time_64.h"
#include "esphome/core/time_conversion.h"

// Per-platform HAL bits (IRAM_ATTR / PROGMEM macros, in_isr_context(),
// inline yield/delay/micros/millis/millis_64 wrappers, ESP8266 progmem
// helpers) live next to each platform component as components/<platform>/hal.h
// and are dispatched here based on the active USE_* platform define. Each
// header guards its body with the matching #ifdef USE_<platform> and re-enters
// namespace esphome {} so it is safe to be re-included.
#if defined(USE_ESP32)
#include "esphome/components/esp32/hal.h"
#elif defined(USE_ESP8266)
#include "esphome/components/esp8266/hal.h"
#elif defined(USE_LIBRETINY)
#include "esphome/components/libretiny/hal.h"
#elif defined(USE_RP2)
#include "esphome/components/rp2/hal.h"
#elif defined(USE_HOST)
#include "esphome/components/host/hal.h"
#elif defined(USE_ZEPHYR)
#include "esphome/components/zephyr/hal.h"
#else
#error "hal.h: not implemented for this platform"
#endif

namespace esphome {

// Cross-platform declarations. delayMicroseconds(), arch_feed_wdt(),
// arch_get_cpu_cycle_count(), arch_init(), arch_get_cpu_freq_hz() vary
// per platform (some inline, some out-of-line) so they live in
// components/<platform>/hal.h.
void __attribute__((noreturn)) arch_restart();

/// Why the current boot happened, as reported by the platform SDK. Most platforms report only a subset.
///
/// RESET_CAUSE_UNKNOWN (0) means the SDK gave no usable reason. It has no YAML spelling, so callers must treat it
/// as "not configured". An SDK that cannot tell two causes apart reports a neighbouring one instead: an ESP32
/// reset-pin press reads as POWER_ON, some ESP32-S3 esp_restart() calls and unrecorded Beken and LN882H resets read
/// as WATCHDOG, and many ESP8266 boards read a cold power-up as EXTERNAL. POWER_ON is not proof of a cold boot.
/// The values are prefixed because bare names collide with SDK macros (EXTERNAL, RESET_PIN).
enum class ResetCause : uint8_t {
  RESET_CAUSE_UNKNOWN = 0,
  RESET_CAUSE_POWER_ON,    ///< Reported as a cold boot; see above.
  RESET_CAUSE_SOFTWARE,    ///< Restart by the firmware (arch_restart()): OTA, restart button, reboot_timeout.
  RESET_CAUSE_WATCHDOG,    ///< Hardware or software watchdog expired.
  RESET_CAUSE_PANIC,       ///< Exception, assert or unhandled fault.
  RESET_CAUSE_BROWNOUT,    ///< Supply dipped below the brownout threshold.
  RESET_CAUSE_EXTERNAL,    ///< Reset pin asserted.
  RESET_CAUSE_SLEEP_WAKE,  ///< Woke from deep sleep.
};

/// Report why this boot happened. Defined in each components/<platform>/hal.cpp, so a platform that lacks it
/// fails at link time.
ResetCause arch_get_reset_cause();

#ifndef USE_ESP8266
// All non-ESP8266 platforms: PROGMEM is a no-op, so these are direct dereferences.
// ESP8266's out-of-line declarations live in components/esp8266/hal.h.
inline uint8_t progmem_read_byte(const uint8_t *addr) { return *addr; }
inline const char *progmem_read_ptr(const char *const *addr) { return *addr; }
inline uint16_t progmem_read_uint16(const uint16_t *addr) { return *addr; }
// Bulk copy out of PROGMEM. PROGMEM is a no-op everywhere except ESP8266, so a
// plain `std::memcpy` is correct and the fast path here.
inline void progmem_memcpy(void *dst, const void *src, size_t len) { std::memcpy(dst, src, len); }
#endif

}  // namespace esphome
