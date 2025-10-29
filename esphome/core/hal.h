#pragma once
#include <string>
#include <cstdint>
#include "gpio.h"

#if defined(USE_ESP32_FRAMEWORK_ESP_IDF)
#include <esp_attr.h>
#ifndef PROGMEM
#define PROGMEM
#endif

#elif defined(USE_ESP32_FRAMEWORK_ARDUINO)

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

namespace esphome {

void yield();
uint32_t millis();
uint32_t micros();
void delay(uint32_t ms);
void delayMicroseconds(uint32_t us);  // NOLINT(readability-identifier-naming)
void __attribute__((noreturn)) arch_restart();
void arch_init();
void arch_feed_wdt();
uint32_t arch_get_cpu_cycle_count();
uint32_t arch_get_cpu_freq_hz();
uint8_t progmem_read_byte(const uint8_t *addr);

}  // namespace esphome
