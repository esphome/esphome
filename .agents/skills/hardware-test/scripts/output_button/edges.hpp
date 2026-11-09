#pragma once
// Timestamps every edge on two output pads (GPIO5, GPIO4) with an interrupt; on ESP8266 a pad reads back its own
// output level, so no wiring is needed.
#include <Arduino.h>
#include "esphome/core/log.h"

namespace edges {
static constexpr int N = 64;
static volatile uint32_t t5[N], t4[N];
static volatile uint8_t l5[N], l4[N];
static volatile int n5, n4;
static uint32_t t0;
static void IRAM_ATTR isr5() {
  int i = n5;
  if (i < N) {
    t5[i] = micros();
    l5[i] = digitalRead(5);
    n5 = i + 1;
  }
}
static void IRAM_ATTR isr4() {
  int i = n4;
  if (i < N) {
    t4[i] = micros();
    l4[i] = digitalRead(4);
    n4 = i + 1;
  }
}
inline void begin() {
  attachInterrupt(digitalPinToInterrupt(5), isr5, CHANGE);
  attachInterrupt(digitalPinToInterrupt(4), isr4, CHANGE);
}
inline void reset() {
  n5 = n4 = 0;
  t0 = micros();
}
inline void report(const char *label) {
  char buf[200];
  int o = snprintf(buf, sizeof(buf), "%s GPIO5:", label);
  for (int i = 0; i < n5 && o < 180; i++)
    o += snprintf(buf + o, sizeof(buf) - o, " %c@%ums", l5[i] ? '+' : '-', (unsigned) ((t5[i] - t0) / 1000));
  ESP_LOGI("edges", "%s", buf);
  o = snprintf(buf, sizeof(buf), "%s GPIO4:", label);
  for (int i = 0; i < n4 && o < 180; i++)
    o += snprintf(buf + o, sizeof(buf) - o, " %c@%ums", l4[i] ? '+' : '-', (unsigned) ((t4[i] - t0) / 1000));
  ESP_LOGI("edges", "%s", buf);
}
}  // namespace edges
