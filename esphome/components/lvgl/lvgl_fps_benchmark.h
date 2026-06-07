/**
 * @file lvgl_fps_benchmark.h
 * Ported FPS sampler from esp-iot-solution esp_lvgl_adapter benchmark
 * (test_esp_lvgl_adapter_fps.c). Counts frames via LV_EVENT_REFR_READY,
 * samples FPS every 500 ms, prints a P10/P25/P50/P75/P90 + IQR report.
 */

#ifndef LVGL_FPS_BENCHMARK_H
#define LVGL_FPS_BENCHMARK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "lvgl.h"

/* Attach sampler to a display. Starts after ~5s warmup, auto-stops on
   200 samples or sustained low-FPS plateau, prints the report once. */
void lvgl_fps_attach_v2(lv_display_t *display);
uint32_t lvgl_esphome_get_fps(void);
void lvgl_esphome_note_frame(void);

/* Force-print the current report (e.g. from a HA button). Safe to call
   any time after attach; no-op if no samples yet. */
void lvgl_fps_benchmark_print(void);

#ifdef __cplusplus
}
#endif

#endif /* LVGL_FPS_BENCHMARK_H */
