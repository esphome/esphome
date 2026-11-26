/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 youkorr
 * Part of ppa-alternative: https://github.com/youkorr/ppa-alternative-
 */

/**
 * @file lv_draw_ortho_ppa.h
 *
 * lv_draw_ortho's turns done by the ESP32-P4's PPA instead of its CPU.
 *
 * lv_draw_ortho draws every widget upright into a tile in internal RAM and
 * puts the tile into the frame buffer turned. Simulated on a P4
 * (test/p4sim), the drawing reads and writes no PSRAM at all; the turn, done
 * by the CPU, is what costs -- five instructions a pixel and every frame-buffer
 * line through the cache. The PPA turns a block by DMA while the CPU goes on:
 * with this, the CPU draws the next tile while the PPA turns the last one.
 *
 * RGB565 and RGB888 only; anything else, a buffer the PPA cannot write, or a
 * full queue, and that turn is the CPU's as before.
 *
 * Two faults of the P4's PPA are kept away from, both found in ESP-IDF's
 * issues and neither handled by LVGL's own PPA unit nor by Espressif's ports:
 *
 *  - DIG-734 (esp-idf #19023, #19096, #18768): at 90 and 270, a block whose
 *    last macro block leaves the DMA fewer bits than its FIFO holds never
 *    finishes. The driver works around it, but up to v5.5.6 -- what ESPHome
 *    builds with -- it sizes that last block before the turn instead of after,
 *    so a sixth of all block sizes hang for ever (fixed on master by 469aa16).
 *    And both drivers bypass only strictly below the FIFO's depth, while a
 *    Tab5 hung on a block leaving exactly that depth (464x70 turned 90).
 *    Such a block is cut here into pieces that are either given the bypass
 *    or leave at least twice the depth -- a bypass where it is not needed does
 *    no harm, Espressif's own esp_lvgl_adapter gives it to every transaction
 *    -- each at least 16 pixels a side: no sliver.
 *  - No wait is unbounded: a transaction that has not finished in
 *    LV_DRAW_ORTHO_PPA_TIMEOUT_MS marks the PPA stuck, and lv_draw_ortho and
 *    the video window go on without it. The driver's own blocking mode waits
 *    with portMAX_DELAY, which is a frozen screen. What it was stuck on is
 *    kept: lv_draw_ortho_ppa_describe_stall().
 */
#ifndef LV_DRAW_ORTHO_PPA_H
#define LV_DRAW_ORTHO_PPA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_draw_ortho.h"
#include "esp_err.h"
#include "driver/ppa.h"

/** Register a PPA client and hand lv_draw_ortho's turns to it. */
esp_err_t lv_draw_ortho_ppa_init(void);

/** Hand the turns back to the CPU and release the PPA. */
void lv_draw_ortho_ppa_deinit(void);

/** One scale-rotate-mirror through the same client and the same guards: cut
 *  for DIG-734 when it is 1:1 and unmirrored, trimmed by up to three source
 *  rows or columns otherwise (the frame's edge, never its middle), and waited
 *  for with a bound. ESP_ERR_TIMEOUT marks the PPA stuck. `op->mode` is
 *  ignored. For lv_draw_ortho_video.c. */
esp_err_t lv_draw_ortho_ppa_srm(const ppa_srm_oper_config_t * op);

/**
 * Turn a big picture in PSRAM -- a camera frame -- through internal RAM: two
 * bands of `bytes` each are taken from internal DMA-capable RAM, and an AXI
 * GDMA copy is installed (esp_async_memcpy). Such a picture turned into a
 * frame buffer then has its rows copied into a band in one run while the PPA
 * turns the band before out of RAM: the PPA reading PSRAM in macro blocks is
 * what made the turn slow (a 1280x720 camera frame turned 270 on a Tab5, the
 * camera streaming: 44.3 ms straight, 18.6 through two bands of 32 rows). A
 * band holds as many whole rows of the picture as fit, a multiple of 8; under
 * 8, or whatever cannot be done that way, is turned the plain way. 0 releases
 * them. ESP_ERR_NO_MEM when internal RAM has no room: the plain turn, then.
 */
esp_err_t lv_draw_ortho_ppa_set_stage(size_t bytes);

/**
 * The same, with no RAM of its own: while lv_draw_ortho turns a picture whole
 * it draws no tile, so it lends the turner its first tile
 * (lv_draw_ortho_set_loan), and the halves of that tile are the bands -- 64 KiB
 * each with the default tiles. Bands of set_stage() win when there are any.
 * Installs the GDMA copy; false stops it. After lv_draw_ortho_ppa_init().
 */
esp_err_t lv_draw_ortho_ppa_use_tiles(bool on);

/** What lv_draw_ortho_ppa_set_stage() and _use_tiles() changed. */
typedef struct {
    uint32_t pictures;    /**< blocks turned through the bands */
    uint32_t bands;       /**< bands they were turned in */
    uint32_t declined;    /**< big blocks in PSRAM turned the plain way, or partly */
    uint32_t rows;        /**< rows in a band, the last time */
    uint32_t band_bytes;  /**< each band's size: set_stage()'s, else the last lent tile's half, else 0 */
    uint64_t us;          /**< spent turning them, but the wait for the last band (the caller's) */
} lv_draw_ortho_ppa_stage_stats_t;

void lv_draw_ortho_ppa_get_stage_stats(lv_draw_ortho_ppa_stage_stats_t * out, bool reset);

/** How long a PPA transaction may take before the PPA is taken for stuck. A
 *  whole 1280x720 turn takes 15 to 45 ms on a P4. */
#ifndef LV_DRAW_ORTHO_PPA_TIMEOUT_MS
#define LV_DRAW_ORTHO_PPA_TIMEOUT_MS 250
#endif

/** True once a PPA transaction has not finished in time: it is not used again
 *  until a reset, since nothing public can unstick it. */
bool lv_draw_ortho_ppa_stuck(void);
void lv_draw_ortho_ppa_mark_stuck(void);

/** A line of lv_draw_ortho_ppa_describe_stall(). */
typedef void (*lv_draw_ortho_ppa_line_cb_t)(const char * line, void * user);

/** After a stall, a few lines on what the PPA was stuck on: which engine, the
 *  transaction that never finished with its whole geometry and the bypass
 *  each driver would program, those queued behind it, and the last that
 *  finished. Nothing when the PPA has not stalled. */
void lv_draw_ortho_ppa_describe_stall(lv_draw_ortho_ppa_line_cb_t line, void * user);

/** How a `w` x `h` input block turned at `rot` is cut before it is sent:
 *  the number of pieces (1 when it is sent whole) with each piece's x, y, w, h
 *  in `pieces`, or 0 when it is left to the CPU. For tests. */
int lv_draw_ortho_ppa_cut(uint32_t w, uint32_t h, lv_display_rotation_t rot, uint32_t bits, uint32_t (*pieces)[4],
                          int max);

/** Whether the PPA driver can hang on this scale-rotate-mirror: `w` x `h`
 *  the INPUT block, `rot` LVGL's rotation, `bits` the output's bits a pixel,
 *  scales as given to the driver: it needs the macro-block bypass and the
 *  driver does not give it. DIG-734 as v5.5.6 and earlier meet it, and a last
 *  macro block within twice the FIFO's depth, which master meets too (a Tab5
 *  hung at exactly the depth). */
bool lv_draw_ortho_ppa_dig734(uint32_t w, uint32_t h, lv_display_rotation_t rot, uint32_t bits,
                              float scale_x, float scale_y);

#ifdef __cplusplus
}
#endif

#endif /* LV_DRAW_ORTHO_PPA_H */
