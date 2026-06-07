/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 *
 * Ported and adapted from
 *   esp-iot-solution/components/display/tools/esp_lvgl_adapter/benchmark/main/
 *   test_esp_lvgl_adapter_fps.c
 *
 * The original uses esp_lv_adapter_get_fps() on a headless display driven
 * by lv_demo_benchmark(). This port counts frames directly via
 * LV_EVENT_REFR_READY on the live display and samples every 500 ms, then
 * reproduces the same percentile / stability-rating report.
 */

#include "esphome/core/defines.h"
#include "lvgl_fps_benchmark.h"

// namespace esphome::lvgl -- ESPHome lint marker; the benchmark exports C-linkage helpers.

#ifdef USE_LVGL_FPS_BENCHMARK

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

extern "C" uint32_t lvgl_esphome_get_perf_logging_enabled(void);

static const char *TAG_FPS = "lvgl"; /* reuse the lvgl tag - known to pass user log filters */

#define FPS_LOGI(...) \
  do { \
    if (lvgl_esphome_get_perf_logging_enabled()) \
      ESP_LOGI(__VA_ARGS__); \
  } while (0)

static constexpr uint32_t FPS_STARTUP_DELAY_MS = 5000;
static constexpr uint32_t FPS_SAMPLE_PERIOD_MS = 500;
static constexpr uint32_t FPS_MAX_SAMPLES = 200;
static constexpr uint32_t FPS_STARTUP_THRESHOLD = 30;
static constexpr uint32_t FPS_STABLE_COUNT_REQUIRED = 3;
static constexpr float FPS_END_DETECTION_RATIO = 0.3f;
static constexpr uint32_t FPS_CONSECUTIVE_END_REQUIRED = 10;
static constexpr uint32_t FPS_ABSOLUTE_MIN_THRESHOLD = 3;

typedef enum {
  FPS_STATE_STARTUP = 0,
  FPS_STATE_STABLE,
  FPS_STATE_ENDING,
  FPS_STATE_DONE,
} fps_state_t;

typedef struct {
  SemaphoreHandle_t mutex;
  lv_display_t *disp;
  volatile uint32_t frame_counter; /* incremented by LV_EVENT_REFR_READY */
  uint64_t last_sample_us;
  fps_state_t state;
  uint32_t state_counter;
  uint32_t samples[FPS_MAX_SAMPLES];
  uint32_t sample_count;
  uint32_t valid_sample_start;
  uint32_t valid_sample_end;
  uint32_t stable_fps_threshold;
  uint32_t consecutive_low_count;
  uint32_t startup_samples[5];
  uint32_t startup_sample_count;
  bool report_printed;
} fps_ctx_t;

static fps_ctx_t s_ctx;
static volatile uint32_t s_current_fps = 0;

extern "C" uint32_t lvgl_esphome_get_fps(void) { return s_current_fps; }

extern "C" void lvgl_esphome_note_frame(void) { s_ctx.frame_counter++; }

static void fps_refr_ready_cb(lv_event_t *e) {
  (void) e;
  s_ctx.frame_counter++;
}

static int compare_uint32(const void *a, const void *b) {
  uint32_t ua = *(const uint32_t *) a;
  uint32_t ub = *(const uint32_t *) b;
  return (ua > ub) - (ua < ub);
}

static uint32_t percentile(const uint32_t *sorted, uint32_t count, float pct) {
  if (count == 0)
    return 0;
  uint32_t idx = (uint32_t) ((count - 1) * pct / 100.0f);
  return sorted[idx];
}

extern "C" void lvgl_fps_benchmark_print(void) {
  if (!s_ctx.mutex)
    return;
  if (xSemaphoreTake(s_ctx.mutex, pdMS_TO_TICKS(2000)) != pdTRUE)
    return;

  uint32_t total = s_ctx.sample_count;
  uint32_t vs = s_ctx.valid_sample_start;
  uint32_t ve = s_ctx.valid_sample_end ? s_ctx.valid_sample_end : total;
  if (ve <= vs) {
    vs = 0;
    ve = total;
  }
  uint32_t count = ve - vs;

  if (count == 0) {
    ESP_LOGW(TAG_FPS, "No samples yet");
    xSemaphoreGive(s_ctx.mutex);
    return;
  }
  if (!lvgl_esphome_get_perf_logging_enabled()) {
    xSemaphoreGive(s_ctx.mutex);
    return;
  }

  uint32_t work[FPS_MAX_SAMPLES];
  memcpy(work, &s_ctx.samples[vs], count * sizeof(uint32_t));

  uint64_t sum = 0;
  uint32_t mn = UINT32_MAX, mx = 0;
  for (uint32_t i = 0; i < count; i++) {
    sum += work[i];
    if (work[i] < mn)
      mn = work[i];
    if (work[i] > mx)
      mx = work[i];
  }
  if (mn == UINT32_MAX)
    mn = 0;

  qsort(work, count, sizeof(uint32_t), compare_uint32);

  uint32_t avg = (uint32_t) (sum / count);
  uint32_t p10 = percentile(work, count, 10);
  uint32_t p25 = percentile(work, count, 25);
  uint32_t p50 = percentile(work, count, 50);
  uint32_t p75 = percentile(work, count, 75);
  uint32_t p90 = percentile(work, count, 90);
  uint32_t iqr = p75 - p25;
  float iqr_ratio = (p50 > 0) ? ((float) iqr / (float) p50 * 100.0f) : 0.0f;

  const char *stab;
  if (iqr_ratio < 30.0f)
    stab = "Excellent";
  else if (iqr_ratio < 60.0f)
    stab = "Good";
  else if (iqr_ratio < 100.0f)
    stab = "Moderate";
  else if (iqr_ratio < 150.0f)
    stab = "Variable";
  else
    stab = "High Variance";

  FPS_LOGI(TAG_FPS, "========================================");
  FPS_LOGI(TAG_FPS, "Performance Report");
  FPS_LOGI(TAG_FPS, "========================================");
  FPS_LOGI(TAG_FPS, "Samples: %lu (filtered: %lu)", (unsigned long) count, (unsigned long) (total - count));
  FPS_LOGI(TAG_FPS, "Key Metrics:");
  FPS_LOGI(TAG_FPS, "  TYPICAL:    %lu fps  (P50, median)", (unsigned long) p50);
  FPS_LOGI(TAG_FPS, "  GUARANTEED: %lu fps  (P25, 75%% of time)", (unsigned long) p25);
  FPS_LOGI(TAG_FPS, "  PEAK:       %lu fps  (P90, best 10%%)", (unsigned long) p90);
  FPS_LOGI(TAG_FPS, "Statistics:");
  FPS_LOGI(TAG_FPS, "  Average:    %lu fps", (unsigned long) avg);
  FPS_LOGI(TAG_FPS, "  Range:      %lu - %lu fps", (unsigned long) mn, (unsigned long) mx);
  FPS_LOGI(TAG_FPS, "  Stability:  %s (IQR: %.1f%%)", stab, iqr_ratio);
  FPS_LOGI(TAG_FPS, "Distribution: P10=%lu | P25=%lu | P50=%lu | P75=%lu | P90=%lu", (unsigned long) p10,
           (unsigned long) p25, (unsigned long) p50, (unsigned long) p75, (unsigned long) p90);
  if (p25 >= 40)
    FPS_LOGI(TAG_FPS, "  [EXCELLENT] Smooth animations, high-refresh UI");
  else if (p25 >= 25)
    FPS_LOGI(TAG_FPS, "  [GOOD] Standard UI, basic animations");
  else if (p25 >= 15)
    FPS_LOGI(TAG_FPS, "  [ACCEPTABLE] Static/simple UI");
  else
    FPS_LOGI(TAG_FPS, "  [LIMITED] Consider optimization or lower resolution");
  FPS_LOGI(TAG_FPS, "========================================");

  xSemaphoreGive(s_ctx.mutex);
}

static void fps_sampler_task(void *arg) {
  (void) arg;

  /* Wait for the UI to settle before sampling */
  vTaskDelay(pdMS_TO_TICKS(FPS_STARTUP_DELAY_MS));
  s_ctx.frame_counter = 0;
  s_ctx.last_sample_us = esp_timer_get_time();
  FPS_LOGI(TAG_FPS, "Sampler started - collecting up to %d samples (%d ms each)", FPS_MAX_SAMPLES,
           FPS_SAMPLE_PERIOD_MS);

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(FPS_SAMPLE_PERIOD_MS));

    uint64_t now = esp_timer_get_time();
    uint32_t frames = s_ctx.frame_counter;
    s_ctx.frame_counter = 0;
    uint64_t elapsed_us = now - s_ctx.last_sample_us;
    s_ctx.last_sample_us = now;
    if (elapsed_us == 0)
      continue;
    uint32_t fps = (uint32_t) ((uint64_t) frames * 1000000ULL / elapsed_us);
    s_current_fps = fps;

    FPS_LOGI(TAG_FPS, "[FPS] Current: %lu fps", (unsigned long) fps);

    if (xSemaphoreTake(s_ctx.mutex, portMAX_DELAY) != pdTRUE)
      continue;

    switch (s_ctx.state) {
      case FPS_STATE_STARTUP:
        if (fps > 0) {
          if (s_ctx.startup_sample_count < 5) {
            s_ctx.startup_samples[s_ctx.startup_sample_count++] = fps;
            if (s_ctx.startup_sample_count == 5) {
              uint32_t sum = 0;
              for (int i = 0; i < 5; i++)
                sum += s_ctx.startup_samples[i];
              s_ctx.stable_fps_threshold = (uint32_t) (sum / 5 * 0.8f);
              FPS_LOGI(TAG_FPS, "[STARTUP] threshold = %lu fps", (unsigned long) s_ctx.stable_fps_threshold);
            }
          }
          if (fps >= s_ctx.stable_fps_threshold) {
            s_ctx.state_counter++;
            if (s_ctx.state_counter >= FPS_STABLE_COUNT_REQUIRED) {
              s_ctx.state = FPS_STATE_STABLE;
              s_ctx.state_counter = 0;
              s_ctx.valid_sample_start = s_ctx.sample_count;
              FPS_LOGI(TAG_FPS, "[STABLE] data collection started");
            }
          } else {
            s_ctx.state_counter = 0;
          }
        }
        break;

      case FPS_STATE_STABLE:
        if (s_ctx.sample_count < FPS_MAX_SAMPLES) {
          s_ctx.samples[s_ctx.sample_count++] = fps;

          uint32_t end_threshold = FPS_ABSOLUTE_MIN_THRESHOLD;
          uint32_t taken = s_ctx.sample_count - s_ctx.valid_sample_start;
          if (taken >= 20) {
            uint32_t start_idx = (s_ctx.sample_count > 20) ? (s_ctx.sample_count - 20) : s_ctx.valid_sample_start;
            uint32_t mn = UINT32_MAX;
            for (uint32_t i = start_idx; i < s_ctx.sample_count; i++) {
              if (s_ctx.samples[i] > 0 && s_ctx.samples[i] < mn)
                mn = s_ctx.samples[i];
            }
            if (mn != UINT32_MAX) {
              uint32_t dyn = (uint32_t) (mn * FPS_END_DETECTION_RATIO);
              end_threshold = dyn > FPS_ABSOLUTE_MIN_THRESHOLD ? dyn : FPS_ABSOLUTE_MIN_THRESHOLD;
            }
          }

          if (fps <= end_threshold) {
            s_ctx.consecutive_low_count++;
            if (s_ctx.consecutive_low_count >= FPS_CONSECUTIVE_END_REQUIRED) {
              s_ctx.valid_sample_end = s_ctx.sample_count - FPS_CONSECUTIVE_END_REQUIRED;
              s_ctx.state = FPS_STATE_ENDING;
              s_ctx.consecutive_low_count = 0;
              FPS_LOGI(TAG_FPS, "[ENDING] valid samples: %lu",
                       (unsigned long) (s_ctx.valid_sample_end - s_ctx.valid_sample_start));
            }
          } else {
            s_ctx.consecutive_low_count = 0;
          }

          if (s_ctx.sample_count % 20 == 0) {
            FPS_LOGI(TAG_FPS, "[STABLE] collected %lu samples", (unsigned long) s_ctx.sample_count);
          }
        } else {
          /* hit the cap -> finalize */
          s_ctx.valid_sample_end = s_ctx.sample_count;
          s_ctx.state = FPS_STATE_ENDING;
        }
        break;

      case FPS_STATE_ENDING:
        s_ctx.state = FPS_STATE_DONE;
        break;

      case FPS_STATE_DONE:
      default:
        break;
    }

    bool should_print_report = false;
    if (s_ctx.state == FPS_STATE_DONE && !s_ctx.report_printed) {
      s_ctx.report_printed = true;
      should_print_report = true;
    }
    xSemaphoreGive(s_ctx.mutex);

    if (should_print_report) {
      lvgl_fps_benchmark_print();
      FPS_LOGI(TAG_FPS, "Use the 'lvgl_fps_print' service to re-print, "
                        "or reboot to re-run the benchmark.");
    }
  }
}

extern "C" void lvgl_fps_attach_v2(lv_display_t *display) {
  /* Log unconditionally on entry so we can verify this TU is linked
     and the call site reaches us. */
  FPS_LOGI(TAG_FPS, ">>> attach() entered, display=%p", display);

  if (!display) {
    ESP_LOGE(TAG_FPS, "display is NULL, aborting");
    return;
  }
  if (s_ctx.disp) {
    ESP_LOGW(TAG_FPS, "already attached to %p", s_ctx.disp);
    return;
  }

  memset(&s_ctx, 0, sizeof(s_ctx));
  s_ctx.disp = display;
  s_ctx.stable_fps_threshold = FPS_STARTUP_THRESHOLD;
  s_ctx.mutex = xSemaphoreCreateMutex();
  if (!s_ctx.mutex) {
    ESP_LOGE(TAG_FPS, "mutex alloc failed");
    return;
  }
  FPS_LOGI(TAG_FPS, "mutex created, registering REFR_READY cb...");

  lv_display_add_event_cb(display, fps_refr_ready_cb, LV_EVENT_REFR_READY, NULL);

  FPS_LOGI(TAG_FPS, "cb registered, creating sampler task...");
  BaseType_t r = xTaskCreate(fps_sampler_task, "lvgl_fps", 4096, NULL, 3, NULL);
  if (r != pdPASS) {
    ESP_LOGE(TAG_FPS, "task create failed (r=%d)", (int) r);
    return;
  }
  FPS_LOGI(TAG_FPS, "<<< attach() done, warmup %d ms", FPS_STARTUP_DELAY_MS);
}

#endif /* USE_LVGL_FPS_BENCHMARK */
