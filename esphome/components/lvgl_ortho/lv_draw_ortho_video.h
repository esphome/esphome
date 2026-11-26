/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 youkorr
 * Part of ppa-alternative: https://github.com/youkorr/ppa-alternative-
 */

/**
 * @file lv_draw_ortho_video.h
 *
 * A camera (or any video source) shown on an ESP32-P4 the way Espressif's own
 * camera examples reach the sensor's frame rate: written straight into the
 * display's frame buffer, never through LVGL. One PPA pass per frame turns,
 * mirrors and scales it into a window that lv_draw_ortho keeps LVGL out of,
 * while LVGL draws the rest of the screen in the panel's orientation.
 *
 *   camera DMA -> camera buffer --(one PPA pass)--> frame buffer window
 *
 * against LVGL today: camera buffer -> LVGL's buffer -> turned in the flush.
 *
 * Requirements: lv_draw_ortho, LV_DISPLAY_RENDER_MODE_DIRECT with the panel's
 * own frame buffer as LVGL's buffer, and lv_display_set_matrix_rotation().
 * Call lv_draw_ortho_video_show() from LVGL's own thread (an lv_timer, or
 * the same loop that calls lv_timer_handler), so it never runs while LVGL is
 * rendering into the same frame buffer.
 */

#ifndef LV_DRAW_ORTHO_VIDEO_H
#define LV_DRAW_ORTHO_VIDEO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_draw_ortho.h"
#include "esp_err.h"
#include "driver/ppa.h"

typedef struct {
    lv_display_t * disp;
    ppa_client_handle_t client;
    lv_area_t ui_area;      /* where the video is, as the UI sees it */
    lv_area_t native;       /* the same, on the panel */
    lv_area_t crop;         /* the part of each frame shown, in frame pixels */
    bool cropped;
    bool placed;
} lv_draw_ortho_video_t;

/** Register a PPA client for `disp`. */
esp_err_t lv_draw_ortho_video_init(lv_draw_ortho_video_t * v, lv_display_t * disp);

/** Put the video at `ui_area` (UI coordinates, e.g. an object's coords). LVGL
 *  stops drawing there from the next refresh. */
void lv_draw_ortho_video_place(lv_draw_ortho_video_t * v, const lv_area_t * ui_area);

/**
 * Show only part of each frame: `x`, `y`, `w`, `h` in the frame's own pixels.
 * The PPA reads just that block, in the same pass. For a sensor whose format
 * is a little bigger than the screen (an OV02C10's 1288x728 on a 1280x800
 * Guition) a centred crop keeps the picture at 1:1, where scaling it by 0.99
 * would soften every pixel. Pass w or h 0 to show the whole frame again.
 */
void lv_draw_ortho_video_set_crop(lv_draw_ortho_video_t * v, uint32_t x, uint32_t y, uint32_t w, uint32_t h);

/**
 * Write one frame into the window: `frame` is `w` x `h`, upright as the UI
 * should show it, RGB565 or RGB888, and is scaled to the window's size. The
 * mirror flags are in that upright frame (a camera's usual mirror_x).
 */
esp_err_t lv_draw_ortho_video_show(lv_draw_ortho_video_t * v, const void * frame, uint32_t w, uint32_t h,
                                   lv_color_format_t cf, bool mirror_x, bool mirror_y);

/** Give LVGL the window back and release the PPA client. */
void lv_draw_ortho_video_deinit(lv_draw_ortho_video_t * v);

#ifdef __cplusplus
}
#endif

#endif /* LV_DRAW_ORTHO_VIDEO_H */
