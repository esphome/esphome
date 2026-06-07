/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 *
 * Ported from esp-iot-solution
 *   components/display/tools/esp_lvgl_adapter/src/display/bridge/v9/lvgl_ppa_accel_v9.c
 * to the ESPHome LVGL 9.5 build (adapted for C++ wrapper / no esp_lvgl_port).
 */

#include "sdkconfig.h"

#ifdef CONFIG_SOC_PPA_SUPPORTED

#include "lvgl_ppa_accel_v9.h"
#include "lv_draw_ppa_private.h"
#include "lvgl.h"
#include "src/draw/sw/blend/lv_draw_sw_blend.h"
#include "src/draw/sw/blend/lv_draw_sw_blend_private.h"
#include "src/draw/sw/blend/lv_draw_sw_blend_to_rgb565.h"
#include "src/draw/sw/blend/lv_draw_sw_blend_to_rgb888.h"
#include "src/draw/lv_draw.h"
#include "src/draw/lv_draw_buf.h"
#include "src/draw/lv_draw_private.h"
#include "src/display/lv_display_private.h"
#include "src/misc/lv_area_private.h"

#include "driver/ppa.h"
#include "esp_cache.h"
#include "esp_private/esp_cache_private.h"
#include "esp_memory_utils.h"
#include "esp_log.h"

uint32_t lvgl_esphome_get_perf_logging_enabled(void);

#define LVGL_PORT_PPA_ALIGNMENT       (64)
#define LVGL_PORT_PPA_ALIGN_UP(s, a)  (((s) + ((a) - 1)) & ~((a) - 1))
#define LVGL_PORT_PPA_MIN_AREA_PX     (100)

static const char *TAG_V9 = "lvgl";  /* reuse the lvgl tag used by the component */

/* Instrumentation: how many ops landed on hardware vs the SW fallback,
   reported once every PPA_STATS_INTERVAL_MS to confirm acceleration is
   live. Set PPA_STATS_INTERVAL_MS to 0 to silence. */
#define PPA_STATS_INTERVAL_MS (2000)

static uint32_t s_ppa_hw_fills   = 0;
static uint32_t s_ppa_hw_blends  = 0;
static uint32_t s_ppa_sw_fallback = 0;
static uint32_t s_ppa_stats_last_log_ms = 0;

#include "esp_timer.h"
static inline uint32_t ppa_now_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void ppa_stats_maybe_log(void) {
#if PPA_STATS_INTERVAL_MS > 0
    uint32_t now = ppa_now_ms();
    if (!lvgl_esphome_get_perf_logging_enabled()) {
        s_ppa_hw_fills = 0;
        s_ppa_hw_blends = 0;
        s_ppa_sw_fallback = 0;
        s_ppa_stats_last_log_ms = now;
        return;
    }
    if (s_ppa_stats_last_log_ms == 0) s_ppa_stats_last_log_ms = now;
    if (now - s_ppa_stats_last_log_ms >= PPA_STATS_INTERVAL_MS) {
        uint32_t total = s_ppa_hw_fills + s_ppa_hw_blends + s_ppa_sw_fallback;
        if (total > 0) {
            uint32_t hw = s_ppa_hw_fills + s_ppa_hw_blends;
            ESP_LOGD(TAG_V9, "PPA ops/2s: HW=%lu (fill=%lu, blend=%lu)  SW=%lu  hw_ratio=%lu%%",
                     (unsigned long)hw, (unsigned long)s_ppa_hw_fills,
                     (unsigned long)s_ppa_hw_blends, (unsigned long)s_ppa_sw_fallback,
                     (unsigned long)(hw * 100 / total));
        }
        s_ppa_hw_fills = 0;
        s_ppa_hw_blends = 0;
        s_ppa_sw_fallback = 0;
        s_ppa_stats_last_log_ms = now;
    }
#endif
}

static ppa_client_handle_t s_blend_handle = NULL;
static ppa_client_handle_t s_fill_handle = NULL;
static size_t s_cache_align = 0;
static bool s_handler_registered = false;

static void lv_draw_ppa_v9_handler(lv_draw_task_t *t, const lv_draw_sw_blend_dsc_t *dsc);

static lv_draw_sw_custom_blend_handler_t s_custom_handler_rgb565 = {
    .dest_cf = LV_COLOR_FORMAT_RGB565,
    .handler = lv_draw_ppa_v9_handler,
};

static lv_draw_sw_custom_blend_handler_t s_custom_handler_rgb888 = {
    .dest_cf = LV_COLOR_FORMAT_RGB888,
    .handler = lv_draw_ppa_v9_handler,
};

static size_t ppa_align(void)
{
    if (s_cache_align == 0) {
        esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &s_cache_align);
        if (s_cache_align == 0 || (s_cache_align & (s_cache_align - 1)) != 0) {
            s_cache_align = LVGL_PORT_PPA_ALIGNMENT;
        }
    }
    return s_cache_align;
}

static void ppa_cache_sync_region(const lv_area_t *area, const lv_area_t *buf_area,
                                  void *buf, uint32_t px_size, int flag)
{
    if (!area || !buf_area || !buf || !esp_ptr_external_ram(buf)) {
        return;
    }

    size_t align = ppa_align();
    int32_t width = lv_area_get_width(area);
    int32_t height = lv_area_get_height(area);
    int32_t buf_w = lv_area_get_width(buf_area);
    int32_t buf_h = lv_area_get_height(buf_area);

    if (width <= 0 || height <= 0 || buf_w <= 0 || buf_h <= 0 || px_size == 0) {
        return;
    }

    int32_t off_x = area->x1 - buf_area->x1;
    int32_t off_y = area->y1 - buf_area->y1;

    if (off_x < 0 || off_y < 0 || (off_x + width) > buf_w || (off_y + height) > buf_h) {
        return;
    }

    uint8_t *start = (uint8_t *)buf + ((size_t)off_y * buf_w + off_x) * px_size;
    size_t bytes = ((size_t)(height - 1) * (size_t)buf_w + (size_t)width) * px_size;
    uintptr_t addr = (uintptr_t)start;
    uintptr_t aligned_addr = addr & ~(align - 1);
    size_t total = LVGL_PORT_PPA_ALIGN_UP(bytes + (addr - aligned_addr), align);

    /* LVGL PR #10107: tolerate unaligned CPU-to-memory flushes. ESP-IDF does
     * not allow ESP_CACHE_MSYNC_FLAG_UNALIGNED with memory-to-cache
     * invalidation, so M2C uses the manually aligned region above. */
    int msync_flags = flag | ESP_CACHE_MSYNC_FLAG_TYPE_DATA;
    if ((flag & ESP_CACHE_MSYNC_FLAG_DIR_M2C) == 0) {
        msync_flags |= ESP_CACHE_MSYNC_FLAG_UNALIGNED;
    }
    esp_cache_msync((void *)aligned_addr, total, msync_flags);
}

static void ppa_cache_invalidate(const lv_area_t *area, const lv_area_t *buf_area, void *buf, uint32_t px_size)
{
    ppa_cache_sync_region(area, buf_area, buf, px_size, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
}

static void ppa_blend(void *bg_buf, lv_color_format_t color_format, uint32_t px_size,
                      const lv_area_t *bg_area, const void *fg_buf, const lv_area_t *fg_area,
                      uint16_t fg_stride_px, const lv_area_t *block_area, lv_opa_t opa)
{
    uint16_t bg_w = lv_area_get_width(bg_area);
    uint16_t bg_h = lv_area_get_height(bg_area);
    uint16_t bg_off_x = block_area->x1 - bg_area->x1;
    uint16_t bg_off_y = block_area->y1 - bg_area->y1;

    uint16_t block_w = lv_area_get_width(block_area);
    uint16_t block_h = lv_area_get_height(block_area);
    uint16_t fg_w = fg_stride_px;
    uint16_t fg_h = lv_area_get_height(fg_area);
    uint16_t fg_off_x = block_area->x1 - fg_area->x1;
    uint16_t fg_off_y = block_area->y1 - fg_area->y1;

    if ((uint32_t)fg_off_x + block_w > fg_w) {
        fg_w = fg_off_x + block_w;
    }
    if ((uint32_t)fg_off_y + block_h > fg_h) {
        fg_h = fg_off_y + block_h;
    }
    size_t align = ppa_align();
    ppa_blend_color_mode_t ppa_cf = lv_color_format_to_ppa_blend(color_format);

    ppa_blend_oper_config_t cfg = {
        .in_bg = {
            .buffer = bg_buf,
            .pic_w = bg_w,
            .pic_h = bg_h,
            .block_w = block_w,
            .block_h = block_h,
            .block_offset_x = bg_off_x,
            .block_offset_y = bg_off_y,
            .blend_cm = ppa_cf,
        },
        .in_fg = {
            .buffer = fg_buf,
            .pic_w = fg_w,
            .pic_h = fg_h,
            .block_w = block_w,
            .block_h = block_h,
            .block_offset_x = fg_off_x,
            .block_offset_y = fg_off_y,
            .blend_cm = ppa_cf,
        },
        .out = {
            .buffer = bg_buf,
            .buffer_size = LVGL_PORT_PPA_ALIGN_UP((size_t)px_size * bg_w * bg_h, align),
            .pic_w = bg_w,
            .pic_h = bg_h,
            .block_offset_x = bg_off_x,
            .block_offset_y = bg_off_y,
            .blend_cm = ppa_cf,
        },
        .bg_rgb_swap = 0,
        .bg_byte_swap = 0,
        .bg_alpha_update_mode = PPA_ALPHA_FIX_VALUE,
        .bg_alpha_fix_val = (uint8_t)(255 - opa),
        .fg_rgb_swap = 0,
        .fg_byte_swap = 0,
        .fg_alpha_update_mode = PPA_ALPHA_FIX_VALUE,
        .fg_alpha_fix_val = opa,
        .mode = PPA_TRANS_MODE_BLOCKING,
    };

    esp_err_t err = ppa_do_blend(s_blend_handle, &cfg);
    if (err != ESP_OK) {
        ESP_LOGW(TAG_V9, "ppa_do_blend failed: %d", err);
    }
}

static void ppa_fill(void *bg_buf, lv_color_format_t color_format, uint32_t px_size,
                     const lv_area_t *bg_area, const lv_area_t *block_area, lv_color_t color)
{
    uint16_t bg_w = lv_area_get_width(bg_area);
    uint16_t bg_h = lv_area_get_height(bg_area);
    size_t align = ppa_align();
    ppa_fill_color_mode_t ppa_cf = lv_color_format_to_ppa_fill(color_format);

    lv_color32_t c32 = lv_color_to_32(color, LV_OPA_COVER);
    uint32_t argb = ((uint32_t)c32.alpha << 24) | ((uint32_t)c32.red << 16) |
                    ((uint32_t)c32.green << 8) | ((uint32_t)c32.blue);

    ppa_fill_oper_config_t cfg = {
        .out = {
            .buffer = bg_buf,
            .buffer_size = LVGL_PORT_PPA_ALIGN_UP((size_t)px_size * bg_w * bg_h, align),
            .pic_w = bg_w,
            .pic_h = bg_h,
            .block_offset_x = (uint32_t)(block_area->x1 - bg_area->x1),
            .block_offset_y = (uint32_t)(block_area->y1 - bg_area->y1),
            .fill_cm = ppa_cf,
        },
        .fill_block_w = (uint32_t)lv_area_get_width(block_area),
        .fill_block_h = (uint32_t)lv_area_get_height(block_area),
        .mode = PPA_TRANS_MODE_BLOCKING,
    };
    cfg.fill_argb_color.val = argb;

    esp_err_t err = ppa_do_fill(s_fill_handle, &cfg);
    if (err != ESP_OK) {
        ESP_LOGW(TAG_V9, "ppa_do_fill failed: %d", err);
    }
}

static void lv_draw_ppa_v9_sw_fallback(lv_draw_task_t *t, const lv_draw_sw_blend_dsc_t *dsc)
{
    lv_layer_t *layer = t->target_layer;
    if (!layer || !layer->draw_buf) {
        return;
    }

    lv_area_t blend_area;
    if (!lv_area_intersect(&blend_area, dsc->blend_area, &t->clip_area)) {
        return;
    }

    uint32_t layer_stride = layer->draw_buf->header.stride;

    if (dsc->src_buf == NULL) {
        lv_draw_sw_blend_fill_dsc_t fill_dsc;
        lv_memzero(&fill_dsc, sizeof(fill_dsc));
        fill_dsc.dest_w = lv_area_get_width(&blend_area);
        fill_dsc.dest_h = lv_area_get_height(&blend_area);
        fill_dsc.dest_stride = layer_stride;
        fill_dsc.opa = dsc->opa;
        fill_dsc.color = dsc->color;
        if (dsc->mask_buf == NULL || dsc->mask_res == LV_DRAW_SW_MASK_RES_FULL_COVER) {
            fill_dsc.mask_buf = NULL;
        } else {
            fill_dsc.mask_buf = dsc->mask_buf;
            fill_dsc.mask_stride = dsc->mask_stride ? dsc->mask_stride : (uint32_t)lv_area_get_width(dsc->mask_area);
            fill_dsc.mask_buf += fill_dsc.mask_stride * (blend_area.y1 - dsc->mask_area->y1) +
                                 (blend_area.x1 - dsc->mask_area->x1);
        }

        fill_dsc.relative_area = blend_area;
        lv_area_move(&fill_dsc.relative_area, -layer->buf_area.x1, -layer->buf_area.y1);
        fill_dsc.dest_buf = lv_draw_layer_go_to_xy(layer,
                                                   blend_area.x1 - layer->buf_area.x1,
                                                   blend_area.y1 - layer->buf_area.y1);
        if (layer->color_format == LV_COLOR_FORMAT_RGB888) {
            lv_draw_sw_blend_color_to_rgb888(&fill_dsc, lv_color_format_get_size(layer->color_format));
        } else {
            lv_draw_sw_blend_color_to_rgb565(&fill_dsc);
        }
        return;
    }

    lv_draw_sw_blend_image_dsc_t image_dsc;
    lv_memzero(&image_dsc, sizeof(image_dsc));
    image_dsc.dest_w = lv_area_get_width(&blend_area);
    image_dsc.dest_h = lv_area_get_height(&blend_area);
    image_dsc.dest_stride = layer_stride;
    image_dsc.opa = dsc->opa;
    image_dsc.blend_mode = dsc->blend_mode;
    const lv_area_t *src_area = dsc->src_area ? dsc->src_area : dsc->blend_area;
    uint32_t src_px_size = lv_color_format_get_size(dsc->src_color_format);
    image_dsc.src_stride = dsc->src_stride ? dsc->src_stride : (lv_area_get_width(src_area) * src_px_size);
    image_dsc.src_color_format = dsc->src_color_format;

    const uint8_t *src_buf = (const uint8_t *)dsc->src_buf;
    src_buf += (size_t)(blend_area.y1 - src_area->y1) * image_dsc.src_stride;
    src_buf += (size_t)(blend_area.x1 - src_area->x1) * src_px_size;
    image_dsc.src_buf = src_buf;

    if (dsc->mask_buf == NULL || dsc->mask_res == LV_DRAW_SW_MASK_RES_FULL_COVER) {
        image_dsc.mask_buf = NULL;
    } else {
        image_dsc.mask_buf = dsc->mask_buf;
        image_dsc.mask_stride = dsc->mask_stride ? dsc->mask_stride : (uint32_t)lv_area_get_width(dsc->mask_area);
        image_dsc.mask_buf += image_dsc.mask_stride * (blend_area.y1 - dsc->mask_area->y1) +
                              (blend_area.x1 - dsc->mask_area->x1);
    }

    image_dsc.relative_area = blend_area;
    lv_area_move(&image_dsc.relative_area, -layer->buf_area.x1, -layer->buf_area.y1);
    if (src_area) {
        image_dsc.src_area = *src_area;
        lv_area_move(&image_dsc.src_area, -layer->buf_area.x1, -layer->buf_area.y1);
    } else {
        lv_memset(&image_dsc.src_area, 0, sizeof(image_dsc.src_area));
    }
    image_dsc.dest_buf = lv_draw_layer_go_to_xy(layer,
                                                blend_area.x1 - layer->buf_area.x1,
                                                blend_area.y1 - layer->buf_area.y1);

    if (layer->color_format == LV_COLOR_FORMAT_RGB888) {
        lv_draw_sw_blend_image_to_rgb888(&image_dsc, lv_color_format_get_size(layer->color_format));
    } else {
        lv_draw_sw_blend_image_to_rgb565(&image_dsc);
    }
}

/* Wrap the SW fallback so we don't have to instrument every call site. */
static inline void lv_draw_ppa_v9_sw_fallback_tracked(lv_draw_task_t *t,
                                                       const lv_draw_sw_blend_dsc_t *dsc) {
    s_ppa_sw_fallback++;
    lv_draw_ppa_v9_sw_fallback(t, dsc);
}
#define lv_draw_ppa_v9_sw_fallback(t, dsc) lv_draw_ppa_v9_sw_fallback_tracked((t), (dsc))

static void lv_draw_ppa_v9_handler(lv_draw_task_t *t, const lv_draw_sw_blend_dsc_t *dsc)
{
    ppa_stats_maybe_log();
    lv_layer_t *layer = t->target_layer;
    if (!layer || !layer->draw_buf ||
            (layer->color_format != LV_COLOR_FORMAT_RGB565 && layer->color_format != LV_COLOR_FORMAT_RGB888)) {
        lv_draw_ppa_v9_sw_fallback(t, dsc);
        return;
    }

    uint32_t dst_px_size = lv_color_format_get_size(layer->color_format);
    if (dst_px_size == 0) {
        lv_draw_ppa_v9_sw_fallback(t, dsc);
        return;
    }

    lv_area_t block_area;
    if (!_lv_area_intersect(&block_area, dsc->blend_area, &t->clip_area)) {
        return;
    }

    if (dsc->mask_buf && dsc->mask_res != LV_DRAW_SW_MASK_RES_FULL_COVER &&
            dsc->mask_res != LV_DRAW_SW_MASK_RES_UNKNOWN) {
        lv_draw_ppa_v9_sw_fallback(t, dsc);
        return;
    }

    if (lv_area_get_size(&block_area) <= LVGL_PORT_PPA_MIN_AREA_PX) {
        lv_draw_ppa_v9_sw_fallback(t, dsc);
        return;
    }

    void *bg_buf = layer->draw_buf->data;
    size_t align = ppa_align();
    if (!bg_buf || ((uintptr_t)bg_buf % align) != 0) {
        lv_draw_ppa_v9_sw_fallback(t, dsc);
        return;
    }

    if (block_area.x1 < layer->buf_area.x1 || block_area.y1 < layer->buf_area.y1 ||
            block_area.x2 > layer->buf_area.x2 || block_area.y2 > layer->buf_area.y2) {
        lv_draw_ppa_v9_sw_fallback(t, dsc);
        return;
    }

    ppa_cache_sync_region(&block_area, &layer->buf_area, bg_buf, dst_px_size, ESP_CACHE_MSYNC_FLAG_DIR_C2M);

    if (dsc->src_buf) {
        if (dsc->src_color_format != layer->color_format) {
            lv_draw_ppa_v9_sw_fallback(t, dsc);
            return;
        }

        const lv_area_t *src_area = dsc->src_area ? dsc->src_area : dsc->blend_area;
        uint32_t src_px_size = lv_color_format_get_size(dsc->src_color_format);
        size_t src_stride = dsc->src_stride ? dsc->src_stride : (lv_area_get_width(src_area) * src_px_size);

        if (src_px_size == 0 || (src_stride % src_px_size) != 0) {
            lv_draw_ppa_v9_sw_fallback(t, dsc);
            return;
        }

        int32_t src_off_x = block_area.x1 - src_area->x1;
        int32_t src_off_y = block_area.y1 - src_area->y1;
        if (src_off_x < 0 || src_off_y < 0) {
            lv_draw_ppa_v9_sw_fallback(t, dsc);
            return;
        }

        const uint8_t *src_ptr8 = (const uint8_t *)dsc->src_buf +
                                  (size_t)src_off_y * src_stride +
                                  (size_t)src_off_x * src_px_size;
        uintptr_t src_addr = (uintptr_t)src_ptr8;
        uintptr_t src_aligned = src_addr & ~(align - 1);
        size_t src_total = LVGL_PORT_PPA_ALIGN_UP(
                               ((size_t)(lv_area_get_height(&block_area) - 1) * src_stride +
                                (size_t)lv_area_get_width(&block_area) * src_px_size) +
                               (src_addr - src_aligned), align);
        if (esp_ptr_external_ram((void *)dsc->src_buf)) {
            esp_cache_msync((void *)src_aligned, src_total,
                            ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
        }

        uint16_t src_stride_px = src_stride / src_px_size;
        s_ppa_hw_blends++;
        ppa_blend(bg_buf, layer->color_format, dst_px_size, &layer->buf_area, dsc->src_buf,
                  src_area, src_stride_px, &block_area, dsc->opa);

        ppa_cache_invalidate(&block_area, &layer->buf_area, bg_buf, dst_px_size);
        return;
    }

    if (dsc->opa >= LV_OPA_MAX) {
        s_ppa_hw_fills++;
        ppa_fill(bg_buf, layer->color_format, dst_px_size, &layer->buf_area, &block_area, dsc->color);
        ppa_cache_invalidate(&block_area, &layer->buf_area, bg_buf, dst_px_size);
        return;
    }

    lv_draw_ppa_v9_sw_fallback(t, dsc);
}

void lvgl_port_ppa_v9_init(lv_display_t *display)
{
    if (!display ||
            (lv_display_get_color_format(display) != LV_COLOR_FORMAT_RGB565 &&
             lv_display_get_color_format(display) != LV_COLOR_FORMAT_RGB888)) {
        ESP_LOGD(TAG_V9, "skip: display format unsupported by PPA v9 blend handler");
        return;
    }

    if (s_blend_handle == NULL && s_fill_handle == NULL) {
        ppa_client_config_t blend_cfg = {
            .oper_type = PPA_OPERATION_BLEND,
            .max_pending_trans_num = 1,
            .data_burst_length = PPA_DATA_BURST_LENGTH_128,
        };
        ppa_client_config_t fill_cfg = {
            .oper_type = PPA_OPERATION_FILL,
            .max_pending_trans_num = 1,
            .data_burst_length = PPA_DATA_BURST_LENGTH_128,
        };
        esp_err_t err = ppa_register_client(&blend_cfg, &s_blend_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG_V9, "ppa blend client register failed: %d", err);
            s_blend_handle = NULL;
            return;
        }
        err = ppa_register_client(&fill_cfg, &s_fill_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG_V9, "ppa fill client register failed: %d", err);
            ppa_unregister_client(s_blend_handle);
            s_blend_handle = NULL;
            s_fill_handle = NULL;
            return;
        }
    }

    if (!s_handler_registered) {
        lv_draw_sw_register_blend_handler(&s_custom_handler_rgb565);
        lv_draw_sw_register_blend_handler(&s_custom_handler_rgb888);
        s_handler_registered = true;
        ESP_LOGD(TAG, "PPA v9 blend handler registered for RGB565/RGB888");
    }
}

#else /* !CONFIG_SOC_PPA_SUPPORTED */

void lvgl_port_ppa_v9_init(lv_display_t *display)
{
    (void)display;
}

#endif /* CONFIG_SOC_PPA_SUPPORTED */
