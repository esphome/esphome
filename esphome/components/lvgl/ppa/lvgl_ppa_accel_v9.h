/**
 * @file lvgl_ppa_accel_v9.h
 * LVGL v9 PPA hardware blend handler (Espressif esp-iot-solution port)
 *
 * Registers a custom SW blend handler that routes RGB565 blend/fill
 * operations to ESP32-P4 PPA. Complements the higher-level draw unit
 * in lv_draw_ppa.c by accelerating all SW blend code paths (text,
 * gradients-after-rasterize, rect edges, etc.).
 */

#ifndef LVGL_PPA_ACCEL_V9_H
#define LVGL_PPA_ACCEL_V9_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

void lvgl_port_ppa_v9_init(lv_display_t *display);

#ifdef __cplusplus
}
#endif

#endif /* LVGL_PPA_ACCEL_V9_H */
