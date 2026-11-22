#pragma once
#include <Arduino.h>
#include "esphome/core/log.h"
// Times every SCL (GPIO5) low phase in an ISR on an ESP8266; long lows are the target stretching the clock.
namespace sclst {
static volatile uint32_t fall_t = 0, max_low = 0, n_1ms = 0, n_5ms = 0, n_20ms = 0, n_100ms = 0, edges = 0;
static volatile uint32_t all_max = 0;
static void IRAM_ATTR isr() {
  uint32_t t = micros();
  edges++;
  if (GPIP(5) == 0) {
    fall_t = t;
  } else {
    uint32_t d = t - fall_t;
    if (d > max_low)
      max_low = d;
    if (d > all_max)
      all_max = d;
    if (d > 1000)
      n_1ms++;
    if (d > 5000)
      n_5ms++;
    if (d > 20000)
      n_20ms++;
    if (d > 100000)
      n_100ms++;
  }
}
inline void start() { attachInterrupt(digitalPinToInterrupt(5), isr, CHANGE); }
inline void dump() {
  ESP_LOGI("sclst", "edges=%u max_low_us=%u all_max_us=%u lows>1ms=%u >5ms=%u >20ms=%u >100ms=%u", (unsigned) edges,
           (unsigned) max_low, (unsigned) all_max, (unsigned) n_1ms, (unsigned) n_5ms, (unsigned) n_20ms,
           (unsigned) n_100ms);
  max_low = 0;
}
}  // namespace sclst
