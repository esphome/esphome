#pragma once
#include <string>
#include <cstdint>
#include "gpio.h"

#if defined(USE_ESP32_FRAMEWORK_ESP_IDF)
#include <esp_attr.h>
#define PROGMEM
#elif defined(USE_ESP32_FRAMEWORK_ARDUINO)
#include <esp_attr.h>
#define PROGMEM
#elif defined(USE_ESP8266)
#include <c_types.h>
#define PROGMEM ICACHE_RODATA_ATTR
#else
#define IRAM_ATTR
#define PROGMEM
#endif

namespace esphome {

void yield();
uint32_t millis();
uint32_t micros();
void delay(uint32_t ms);
void delayMicroseconds(uint32_t us);
void __attribute__((noreturn)) arch_restart();
void arch_feed_wdt();
uint32_t arch_get_cpu_cycle_count();
uint32_t arch_get_cpu_freq_hz();
uint8_t progmem_read_8(const uint8_t *addr);
uint16_t progmem_read_16(const uint16_t *addr);
uint32_t progmem_read_32(const uint32_t *addr);

}  // namespace esphome
