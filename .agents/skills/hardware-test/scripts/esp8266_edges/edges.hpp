#pragma once
// GPIO12 edge timestamps from an ISR; dump() logs the count and the `<dt><H|L>` intervals, then clears.
#include <Arduino.h>
namespace edges {
static constexpr int N = 256;
static volatile uint32_t ts[N];
static volatile uint8_t lv[N];
static volatile int cnt = 0;
static volatile uint32_t total = 0;
static void IRAM_ATTR isr() {
  uint32_t t = micros();
  total++;
  int c = cnt;
  if (c < N) {
    ts[c] = t;
    lv[c] = digitalRead(12);
    cnt = c + 1;
  }
}
inline void start() {
  pinMode(12, INPUT);
  attachInterrupt(digitalPinToInterrupt(12), isr, CHANGE);
}
inline void dump(const char *tag) {
  int c = cnt;
  ESP_LOGI("edges", "%s level=%d edges=%d total=%u", tag, digitalRead(12), c, (unsigned) total);
  char buf[200];
  int p = 0;
  for (int i = 0; i < c; i++) {
    p += snprintf(buf + p, sizeof(buf) - p, "%u%c ", (unsigned) (i ? ts[i] - ts[i - 1] : 0), lv[i] ? 'H' : 'L');
    if (p > 150 || i == c - 1) {
      ESP_LOGI("edges", "  %s", buf);
      p = 0;
    }
  }
  cnt = 0;
}
}  // namespace edges
