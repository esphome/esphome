#pragma once
// GPIO4 any-edge interrupt through ESP32InternalGPIOPin; the signal comes from another board, and GPIO22 (jumpered
// to GPIO4) is a plain input read as the true level. First 40 s a NO_LIGHT_SLEEP lock is held, then sleep is free.
#include <cstdio>
#include <cstring>
#include "driver/gpio.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "esphome/components/esp32/gpio.h"
#include "esphome/core/log.h"

namespace wd {
static const char *const TAG = "wd";
static constexpr gpio_num_t IN = GPIO_NUM_4;
static constexpr gpio_num_t REF = GPIO_NUM_22;
static constexpr int64_t AWAKE_US = 40000000;
struct Ev {
  uint32_t t_us;
  uint8_t in;
  uint8_t ref;
};
// Two capture windows: from 9.5 s (awake) and from 69.5 s (sleep allowed).
static constexpr uint32_t W0_US = 9500000, W1_US = 69500000, N0 = 48, N1 = 256;
static Ev w0[N0], w1[N1];
static volatile uint32_t n0 = 0, n1 = 0;
// per phase: 0 awake (lock held), 1 sleep allowed
static volatile uint32_t calls[2], real[2], spurious[2], mismatch[2];
static volatile uint8_t last_ref = 2;
static int64_t start_us = 0;
static esp_pm_lock_handle_t lock = nullptr;
static volatile bool locked = false;
static volatile uint32_t wakes = 0, wake_ext1 = 0, wake_timer = 0, wake_gpio = 0, wake_other = 0;

static void IRAM_ATTR on_edge(void *) {
  uint32_t t = (uint32_t) (esp_timer_get_time() - start_us);
  uint8_t in = gpio_get_level(IN);
  uint8_t ref = gpio_get_level(REF);
  int ph = locked ? 0 : 1;
  calls[ph] = calls[ph] + 1;
  if (ref != last_ref)
    real[ph] = real[ph] + 1;
  else
    spurious[ph] = spurious[ph] + 1;
  if (in != ref)
    mismatch[ph] = mismatch[ph] + 1;
  last_ref = ref;
  if (t >= W1_US) {
    uint32_t i = n1;
    if (i < N1) {
      w1[i] = {t, in, ref};
      n1 = i + 1;
    }
  } else if (t >= W0_US) {
    uint32_t i = n0;
    if (i < N0) {
      w0[i] = {t, in, ref};
      n0 = i + 1;
    }
  }
}

#ifdef CONFIG_PM_LIGHT_SLEEP_CALLBACKS
static IRAM_ATTR esp_err_t on_exit(int64_t, void *) {
  wakes = wakes + 1;
  return ESP_OK;
}
#endif

inline void start() {
  gpio_config_t rc{};
  rc.pin_bit_mask = 1ULL << REF;
  rc.mode = GPIO_MODE_INPUT;
  gpio_config(&rc);
  // The first phase is the awake control; it only counts when the lock really holds the chip awake
  esp_err_t err = esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "wd_awake", &lock);
  if (err == ESP_OK)
    err = esp_pm_lock_acquire(lock);
  if (err != ESP_OK)
    ESP_LOGE(TAG, "awake lock failed: %s; the awake phase is not a valid control", esp_err_to_name(err));
  locked = err == ESP_OK;
  start_us = esp_timer_get_time();
#ifdef CONFIG_PM_LIGHT_SLEEP_CALLBACKS
  static esp_pm_sleep_cbs_register_config_t cbs{};
  cbs.exit_cb = &on_exit;
  cbs.exit_cb_prior = 5;
  ESP_LOGI(TAG, "exit callback %s", esp_err_to_name(esp_pm_light_sleep_register_cbs(&cbs)));
#endif
  auto *pin = new esphome::esp32::ESP32InternalGPIOPin();  // NOLINT
  pin->set_pin(IN);
  pin->set_flags(esphome::gpio::FLAG_INPUT);
  pin->setup();
  pin->attach_interrupt(on_edge, (void *) nullptr, esphome::gpio::INTERRUPT_ANY_EDGE);
  ESP_LOGI(TAG, "started, awake phase %u s, GPIO4 %d GPIO22 %d", (unsigned) (AWAKE_US / 1000000), gpio_get_level(IN),
           gpio_get_level(REF));
}

static int64_t phase_start[2] = {0, 0};
inline void report() {
  if (start_us == 0)
    return;
  int64_t el = esp_timer_get_time() - start_us;
  if (locked && el >= AWAKE_US) {
    locked = false;
    phase_start[1] = el;
    esp_pm_lock_release(lock);
    ESP_LOGI(TAG, "%.1f s: sleep lock released, sleep allowed from now", el / 1e6);
  }
  // Sample GPIO4 and GPIO22 every 50 ms for 1 s.
  char a[21], b[21];
  for (int i = 0; i < 20; i++) {
    a[i] = gpio_get_level(IN) ? '1' : '0';
    b[i] = gpio_get_level(REF) ? '1' : '0';
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  a[20] = b[20] = 0;
  ESP_LOGI(TAG, "sampled 1 s: GPIO4 %s GPIO22 %s", a, b);
  for (int ph = 0; ph < 2; ph++) {
    double secs = ph == 0 ? (locked ? el : AWAKE_US) / 1e6 : (locked ? 0 : (el - phase_start[1]) / 1e6);
    ESP_LOGI(TAG,
             "%.0f s %s: %.1f s, isr calls %u, real edges %u (expected about %.0f), no change %u, GPIO4!=GPIO22 %u",
             el / 1e6, ph == 0 ? "awake" : "sleep", secs, (unsigned) calls[ph], (unsigned) real[ph], secs * 4,
             (unsigned) spurious[ph], (unsigned) mismatch[ph]);
  }
#ifdef CONFIG_PM_LIGHT_SLEEP_CALLBACKS
  ESP_LOGI(TAG, "light sleep exits %u", (unsigned) wakes);
#endif
  ESP_LOGI(TAG, "last wake cause %d (2 ext0, 3 ext1, 4 timer, 7 gpio)", (int) esp_sleep_get_wakeup_cause());
#ifdef CONFIG_PM_PROFILING
  static char buf[1024];
  FILE *f = fmemopen(buf, sizeof(buf) - 1, "w");
  if (f != nullptr) {
    esp_pm_dump_locks(f);
    long n = ftell(f);
    fclose(f);
    buf[n < 0 ? 0 : n] = 0;
    for (char *line = strtok(buf, "\n"); line != nullptr; line = strtok(nullptr, "\n"))
      if (strstr(line, "SLEEP  ") || strstr(line, "SLEEP\t") || strstr(line, " SLEEP"))
        ESP_LOGI(TAG, "pm %s", line);
  }
#endif
}

inline void dump_window(const char *label, const Ev *w, uint32_t n, uint32_t count) {
  ESP_LOGI(TAG, "%s: %u events captured, first %u:", label, (unsigned) n, (unsigned) count);
  uint32_t prev = n ? w[0].t_us : 0;
  for (uint32_t i = 0; i < n && i < count; i++) {
    ESP_LOGI(TAG, "ev %2u t=%.6f s gap %7u us GPIO4 %u GPIO22 %u", (unsigned) i, w[i].t_us / 1e6,
             (unsigned) (w[i].t_us - prev), w[i].in, w[i].ref);
    prev = w[i].t_us;
  }
}
inline void dump() {
  dump_window("window from 9.5 s (awake)", w0, n0, 12);
  dump_window("window from 69.5 s (sleep allowed)", w1, n1, 40);
}
}  // namespace wd
