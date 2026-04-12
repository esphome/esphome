#pragma once
#include <string>
#include <cstdint>
#include "gpio.h"

#if defined(USE_ESP32)
#include <esp_attr.h>
#ifndef PROGMEM
#define PROGMEM
#endif

#elif defined(USE_ESP8266)

#include <c_types.h>
#ifndef PROGMEM
#define PROGMEM ICACHE_RODATA_ATTR
#endif

#elif defined(USE_RP2040)

#define IRAM_ATTR __attribute__((noinline, long_call, section(".time_critical")))
#define PROGMEM

#else

#define IRAM_ATTR
#define PROGMEM

#endif

// On ESP32 with 1 kHz FreeRTOS tick rate, millis() is just xTaskGetTickCount() —
// a single volatile read of a DRAM global. Inlining it here eliminates the
// function call entirely at every call site. No IRAM needed (no flash code
// executed), no 64-bit math, no HAL call.
#if defined(USE_ESP32) && CONFIG_FREERTOS_HZ == 1000
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace esphome {

void yield();
#if defined(USE_ESP32) && CONFIG_FREERTOS_HZ == 1000
inline uint32_t millis() { return xTaskGetTickCount(); }
#else
uint32_t millis();
#endif
uint64_t millis_64();
uint32_t micros();
void delay(uint32_t ms);
void delayMicroseconds(uint32_t us);  // NOLINT(readability-identifier-naming)
void __attribute__((noreturn)) arch_restart();
void arch_init();
void arch_feed_wdt();
uint32_t arch_get_cpu_cycle_count();
uint32_t arch_get_cpu_freq_hz();

#ifdef USE_ESP8266
// ESP8266: pgm_read_* does real flash reads on Harvard architecture
uint8_t progmem_read_byte(const uint8_t *addr);
const char *progmem_read_ptr(const char *const *addr);
uint16_t progmem_read_uint16(const uint16_t *addr);
#else
// All other platforms: PROGMEM is a no-op, so these are direct dereferences
inline uint8_t progmem_read_byte(const uint8_t *addr) { return *addr; }
inline const char *progmem_read_ptr(const char *const *addr) { return *addr; }
inline uint16_t progmem_read_uint16(const uint16_t *addr) { return *addr; }
#endif

}  // namespace esphome
