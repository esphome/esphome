#pragma once
// Samples the status LED pin from an esp_timer every 2 ms, off the loop task, and records edges in a ring.
// n counts every edge; take n as the mark when a window starts.
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace ledprobe {
static constexpr gpio_num_t PIN = GPIO_NUM_25;
static constexpr size_t MAX_EDGES = 400;
inline volatile uint32_t edge_t[MAX_EDGES];
inline volatile uint8_t edge_v[MAX_EDGES];
inline volatile size_t n = 0;  // edges seen so far, the ring keeps the last MAX_EDGES
inline volatile int last = -1;
inline volatile uint32_t samples = 0;
inline volatile uint32_t last_sample_ms = 0;
inline bool started = false;  // pin and timer set up

inline void sample(void *) {
  int v = gpio_get_level(PIN);
  samples++;
  last_sample_ms = (uint32_t) (esp_timer_get_time() / 1000);
  if (v != last) {
    last = v;
    size_t i = n;
    edge_t[i % MAX_EDGES] = (uint32_t) (esp_timer_get_time() / 1000);
    edge_v[i % MAX_EDGES] = v;
    n = i + 1;
  }
}

inline void start() {
  esp_err_t err = gpio_input_enable(PIN);
  if (err != ESP_OK) {
    ESP_LOGE("probe", "LED pin input failed: %s", esp_err_to_name(err));
    return;
  }
  esp_timer_create_args_t a{};
  a.callback = sample;
  a.name = "ledprobe";
  esp_timer_handle_t h;
  if ((err = esp_timer_create(&a, &h)) != ESP_OK || (err = esp_timer_start_periodic(h, 2000)) != ESP_OK) {
    ESP_LOGE("probe", "LED probe timer failed: %s", esp_err_to_name(err));
    return;
  }
  started = true;
}

inline uint32_t now_ms() { return (uint32_t) (esp_timer_get_time() / 1000); }

// Pattern of the edges recorded since mark; call from the loop after the window.
inline void report(const char *label, size_t mark, uint32_t t0, uint32_t t1) {
  uint32_t ons = 0, on_min = UINT32_MAX, on_max = 0, on_sum = 0;
  uint32_t per_min = UINT32_MAX, per_max = 0, per_sum = 0, pers = 0, prev_rise = 0, rise = 0, max_gap = 0;
  uint32_t prev_edge = t0;
  if (!started) {
    ESP_LOGW("probe", "%s: INVALID, the probe never started (see the setup error at boot)", label);
    return;
  }
  uint32_t stale = now_ms() - last_sample_ms;
  if (stale > 100) {
    ESP_LOGW("probe", "%s: INVALID, no pin sample for %u ms, the probe timer stopped", label, (unsigned) stale);
    return;
  }
  size_t end = n;
  if (end - mark > MAX_EDGES) {
    ESP_LOGW("probe", "%s: INVALID, %u edges overwritten, use a shorter window", label,
             (unsigned) (end - mark - MAX_EDGES));
    return;
  }
  for (size_t i = mark; i < end; i++) {
    uint32_t t = edge_t[i % MAX_EDGES];
    if (t < t0 || t > t1)
      continue;
    if (t - prev_edge > max_gap)
      max_gap = t - prev_edge;
    prev_edge = t;
    if (edge_v[i % MAX_EDGES]) {
      if (prev_rise) {
        uint32_t p = t - prev_rise;
        per_sum += p;
        pers++;
        if (p < per_min)
          per_min = p;
        if (p > per_max)
          per_max = p;
      }
      prev_rise = rise = t;
    } else if (rise) {
      uint32_t o = t - rise;
      ons++;
      on_sum += o;
      if (o < on_min)
        on_min = o;
      if (o > on_max)
        on_max = o;
      rise = 0;
    }
  }
  // the timer task kept writing while we read; slots read may have been overwritten
  if (n - mark > MAX_EDGES) {
    ESP_LOGW("probe", "%s: INVALID, %u edges came in while reading, use a shorter window", label, (unsigned) (n - end));
    return;
  }
  if (t1 - prev_edge > max_gap)
    max_gap = t1 - prev_edge;
  ESP_LOGI("probe",
           "%s: window %u ms, blinks %u, on %u/%u/%u ms (min/avg/max), period %u/%u/%u ms, longest gap %u ms, level "
           "now %d, samples %u",
           label, (unsigned) (t1 - t0), (unsigned) ons, ons ? (unsigned) on_min : 0,
           ons ? (unsigned) (on_sum / ons) : 0, (unsigned) on_max, pers ? (unsigned) per_min : 0,
           pers ? (unsigned) (per_sum / pers) : 0, (unsigned) per_max, (unsigned) max_gap, (int) last,
           (unsigned) samples);
}
}  // namespace ledprobe
