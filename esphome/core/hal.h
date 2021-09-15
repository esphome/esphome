#pragma once
#include <string>
#include <cstdint>
#include "gpio.h"

#if defined(USE_ESP32_FRAMEWORK_ESP_IDF)
#include <esp_attr.h>
#elif defined(USE_ESP32_FRAMEWORK_ARDUINO)
#include <esp_attr.h>
#elif defined(USE_ESP8266)
#include <c_types.h>
#else
#define IRAM_ATTR
#endif

namespace esphome {

void yield();
uint32_t millis();
uint32_t micros();
void delay(uint32_t ms);
void delayMicroseconds(uint32_t us);
void __attribute__((noreturn)) arch_restart();
void arch_feed_wdt();

}  // namespace esphome
