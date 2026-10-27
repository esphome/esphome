#pragma once
// LEDC on an RC_FAST clock keeps toggling GPIO10 in light sleep; an ESP32InternalGPIOPin interrupt on the same pad
// timestamps every edge.
#include <cstdio>
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_pm.h"
#include "esp_timer.h"
#include "esphome/components/esp32/gpio.h"
#include "esphome/core/log.h"

namespace wakeprobe {
static const char *const TAG = "wakeprobe";
static constexpr gpio_num_t PIN = GPIO_NUM_10;
static volatile uint32_t edges = 0;
static volatile int64_t last_us = 0;
static volatile int64_t gap_min = INT64_MAX, gap_max = 0;
static volatile uint32_t late_edges = 0;  // gap more than 20 ms off the half period
static int64_t start_us = 0;
static uint32_t half_period_us = 0;
static uint32_t freq = 0;
static esp_err_t err_timer = ESP_OK, err_channel = ESP_OK;

static void IRAM_ATTR on_edge(void *) {
  int64_t now = esp_timer_get_time();
  if (last_us != 0) {
    int64_t gap = now - last_us;
    if (gap < gap_min)
      gap_min = gap;
    if (gap > gap_max)
      gap_max = gap;
    int64_t off = gap - (int64_t) half_period_us;
    if (off > 20000 || off < -20000)
      late_edges = late_edges + 1;
  }
  last_us = now;
  edges = edges + 1;
}

inline void start() {
  auto *pin = new esphome::esp32::ESP32InternalGPIOPin();  // NOLINT
  pin->set_pin(PIN);
  pin->set_flags(esphome::gpio::FLAG_INPUT);
  pin->setup();
  pin->attach_interrupt(on_edge, (void *) nullptr, esphome::gpio::INTERRUPT_ANY_EDGE);
  ledc_timer_config_t t{};
  t.speed_mode = LEDC_LOW_SPEED_MODE;
  t.duty_resolution = LEDC_TIMER_14_BIT;
  t.timer_num = LEDC_TIMER_0;
  t.freq_hz = 2;
  t.clk_cfg = LEDC_USE_RC_FAST_CLK;
  esp_err_t e1 = err_timer = ledc_timer_config(&t);
  ledc_channel_config_t c{};
  c.gpio_num = PIN;
  c.speed_mode = LEDC_LOW_SPEED_MODE;
  c.channel = LEDC_CHANNEL_0;
  c.timer_sel = LEDC_TIMER_0;
  c.duty = 1 << 13;
  c.sleep_mode = LEDC_SLEEP_MODE_KEEP_ALIVE;
  esp_err_t e2 = err_channel = ledc_channel_config(&c);
  // ledc_channel_config makes the pad output only; turn its input back on so the interrupt sees the wave.
  gpio_input_enable(PIN);
  freq = ledc_get_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0);
  half_period_us = freq ? 500000 / freq : 0;
  start_us = esp_timer_get_time();
  ESP_LOGI(TAG, "LEDC timer=%s channel=%s freq=%u Hz half period %u us", esp_err_to_name(e1), esp_err_to_name(e2),
           (unsigned) freq, (unsigned) half_period_us);
}

inline void report() {
  if (start_us == 0)
    return;
  float secs = (esp_timer_get_time() - start_us) / 1e6f;
  char lv[21];
  for (int i = 0; i < 20; i++) {
    lv[i] = gpio_get_level(PIN) ? '1' : '0';
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  lv[20] = 0;
  ESP_LOGI(TAG, "LEDC timer=%s channel=%s freq=%u, level over 1 s: %s", esp_err_to_name(err_timer),
           esp_err_to_name(err_channel), (unsigned) freq, lv);
  ESP_LOGI(TAG, "%.0f s: edges=%u expected=%.0f gap min=%lld max=%lld us, off by >20 ms: %u", secs, (unsigned) edges,
           secs * 2 * freq, (long long) gap_min, (long long) gap_max, (unsigned) late_edges);
#ifdef CONFIG_PM_PROFILING
  static char buf[1024];
  FILE *f = fmemopen(buf, sizeof(buf) - 1, "w");
  if (f != nullptr) {
    esp_pm_dump_locks(f);
    long n = ftell(f);
    fclose(f);
    buf[n < 0 ? 0 : n] = 0;
    for (char *line = strtok(buf, "\n"); line != nullptr; line = strtok(nullptr, "\n")) {
      if (strstr(line, "SLEEP") || strstr(line, "CPU_MAX") || strstr(line, "APB_MAX") || strstr(line, "APB_MIN"))
        ESP_LOGI(TAG, "pm %s", line);
    }
  }
#endif
}
}  // namespace wakeprobe
