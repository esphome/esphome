/**
 * @file lv_draw_ppa_private.h
 * Custom PPA private header for ESP32-P4
 * Based on https://github.com/lvgl/lvgl/pull/9162 (included in LVGL 9.5+)
 * Adapted for C++ compilation (ESPHome build system)
 */

#pragma once
// namespace esphome::lvgl -- ESPHome lint marker; this header is shared with C sources.

#ifndef LV_DRAW_PPA_PRIVATE_FIXED_H
#define LV_DRAW_PPA_PRIVATE_FIXED_H

/*********************
 *      INCLUDES
 *********************/
#include "lvgl.h"
#include "src/lv_conf_internal.h"
#include "src/draw/lv_draw_private.h"
#include "src/draw/lv_draw_buf_private.h"
#include "src/display/lv_display_private.h"
#include "src/misc/lv_area_private.h"

/* The ppa driver depends heavily on the esp-idf headers */
#include "sdkconfig.h"

#ifndef CONFIG_SOC_PPA_SUPPORTED
#error "This SoC does not support PPA"
#endif

#include "driver/ppa.h"
#include "esp_heap_caps.h"
#include "esp_err.h"
#include "hal/color_hal.h"
#include "esp_cache.h"
#include "esp_private/esp_cache_private.h"
#include "esp_log.h"
#include "esp_memory_utils.h"

/*********************
 *      DEFINES
 *********************/

#ifdef CONFIG_CACHE_L2_CACHE_LINE_SIZE
#define PPA_CACHE_LINE_SIZE ((uint32_t) CONFIG_CACHE_L2_CACHE_LINE_SIZE)
#else
#define PPA_CACHE_LINE_SIZE 64U
#endif

/**********************
 *      TYPEDEFS
 **********************/

/**
 * Round a byte size up to LV_DRAW_BUF_ALIGN (cache-line on ESP32-P4).
 * ESP-IDF PPA hardware and esp_cache_msync() both require sizes aligned to
 * the cache line, mirroring what esp_lvgl_port (common/ppa/lcd_ppa.c) does
 * via ALIGN_UP(size, CONFIG_CACHE_L2_CACHE_LINE_SIZE).
 */
static inline uint32_t lv_draw_ppa_cache_align(void) {
  size_t alignment = 0;
  esp_err_t err = esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &alignment);
  if (err != ESP_OK || alignment == 0 || (alignment & (alignment - 1U)) != 0) {
    alignment = 64;
  }
  return (uint32_t) alignment;
}

static inline uint32_t lv_draw_ppa_align_size(uint32_t size) {
  uint32_t alignment = lv_draw_ppa_cache_align();
  return (size + alignment - 1U) & ~(alignment - 1U);
}

static inline bool lv_draw_ppa_buf_cache_aligned(const void *p) {
  return ((uintptr_t) p % lv_draw_ppa_cache_align()) == 0;
}

typedef struct lv_draw_ppa_unit {
  lv_draw_unit_t base_unit;
  lv_draw_task_t *task_act;
  ppa_client_handle_t srm_client;
  ppa_client_handle_t fill_client;
  ppa_client_handle_t blend_client;
  uint8_t *buf;
} lv_draw_ppa_unit_t;

/**********************
 *   STATIC FUNCTIONS
 **********************/

static inline bool ppa_src_cf_supported(lv_color_format_t cf) {
  switch (cf) {
    case LV_COLOR_FORMAT_RGB565:
    case LV_COLOR_FORMAT_RGB888:
    case LV_COLOR_FORMAT_ARGB8888:
    case LV_COLOR_FORMAT_XRGB8888:
      return true;
    default:
      return false;
  }
}

static inline bool ppa_dest_cf_supported(lv_color_format_t cf) {
  switch (cf) {
    case LV_COLOR_FORMAT_RGB565:
    case LV_COLOR_FORMAT_RGB888:
    case LV_COLOR_FORMAT_ARGB8888:
      return true;
    default:
      return false;
  }
}

static inline ppa_fill_color_mode_t lv_color_format_to_ppa_fill(lv_color_format_t lv_fmt) {
  switch (lv_fmt) {
    case LV_COLOR_FORMAT_RGB565:
      return PPA_FILL_COLOR_MODE_RGB565;
    case LV_COLOR_FORMAT_RGB888:
      return PPA_FILL_COLOR_MODE_RGB888;
    case LV_COLOR_FORMAT_ARGB8888:
      return PPA_FILL_COLOR_MODE_ARGB8888;
    default:
      return PPA_FILL_COLOR_MODE_RGB565;
  }
}

static inline ppa_blend_color_mode_t lv_color_format_to_ppa_blend(lv_color_format_t lv_fmt) {
  switch (lv_fmt) {
    case LV_COLOR_FORMAT_RGB565:
      return PPA_BLEND_COLOR_MODE_RGB565;
    case LV_COLOR_FORMAT_RGB888:
      return PPA_BLEND_COLOR_MODE_RGB888;
    case LV_COLOR_FORMAT_ARGB8888:
      return PPA_BLEND_COLOR_MODE_ARGB8888;
    default:
      return PPA_BLEND_COLOR_MODE_RGB565;
  }
}

static inline ppa_srm_color_mode_t lv_color_format_to_ppa_srm(lv_color_format_t lv_fmt) {
  switch (lv_fmt) {
    case LV_COLOR_FORMAT_RGB565:
      return PPA_SRM_COLOR_MODE_RGB565;
    case LV_COLOR_FORMAT_RGB888:
      return PPA_SRM_COLOR_MODE_RGB888;
    case LV_COLOR_FORMAT_XRGB8888:
      return PPA_SRM_COLOR_MODE_ARGB8888;
    default:
      return PPA_SRM_COLOR_MODE_RGB565;
  }
}

#endif /* LV_DRAW_PPA_PRIVATE_FIXED_H */
