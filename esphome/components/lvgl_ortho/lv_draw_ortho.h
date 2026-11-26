/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 youkorr
 * Part of ppa-alternative: https://github.com/youkorr/ppa-alternative-
 */

/**
 * @file lv_draw_ortho.h
 *
 * A draw unit that renders straight into a display's NATIVE orientation when
 * the display is turned by 90, 180 or 270 degrees.
 *
 * LVGL normally renders a turned display upright and turns the finished
 * buffer in the flush callback: every changed pixel is written twice, and the
 * second write is a transpose, which is the slowest thing a memory bus can be
 * asked to do. LVGL 9 already has the other half of the answer:
 * `lv_display_set_matrix_rotation(disp, true)` hands every draw task a matrix
 * and lets a draw unit render through it -- but only the vector GPU units read
 * that matrix, so on a plain software renderer the option draws nothing.
 *
 * This unit reads it. A matrix that only turns by quarter turns is a
 * permutation of pixels, so:
 *  - a solid fill is drawn as the native rectangle it becomes, at the cost of
 *    an unturned fill;
 *  - an opaque image is copied with its turn in ONE pass from its source to
 *    the frame buffer, instead of a copy and then a turn;
 *  - an image whose pixels are already stored in the panel's orientation
 *    (`LV_DRAW_ORTHO_IMAGE_FLAG_NATIVE`) is copied row by row with no turn at
 *    all -- which is what a video source can produce for free;
 *  - everything else (text, borders, shadows, arcs, masks, layers...) is drawn
 *    by LVGL's own software renderer into a small upright tile and put back
 *    turned, so its pixels are exactly the software renderer's.
 */

#ifndef LV_DRAW_ORTHO_H
#define LV_DRAW_ORTHO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

#ifndef LV_USE_DRAW_ORTHO
#define LV_USE_DRAW_ORTHO 1
#endif

#if LV_USE_DRAW_ORTHO

#if !LV_DRAW_TRANSFORM_USE_MATRIX
#error "lv_draw_ortho needs LV_DRAW_TRANSFORM_USE_MATRIX 1 (and so LV_USE_MATRIX and LV_USE_FLOAT)"
#endif

/** Bytes of the upright tile the general path draws into. Every tile costs
 *  each task touching it a call into the software renderer (a label lays its
 *  text out again), so a tile too small is slower than LVGL's own way. On a
 *  PC with the test UI at 720x1280: 32 KiB 2.6 ms, 64 KiB 1.8, 128 KiB 1.45,
 *  256 KiB 1.3, against 1.7 for drawing upright and turning in flush. */
#ifndef LV_DRAW_ORTHO_TILE_BYTES
#define LV_DRAW_ORTHO_TILE_BYTES (128 * 1024)
#endif

/** With a second core: the pixels a tile must hold to be cut in two, one half
 *  for each core, when it would otherwise be drawn by the first core alone
 *  (an area that fits in one tile, or the odd tile of a larger one). Below
 *  it, the second turn and the second pass over the tasks cost more than the
 *  shared drawing saves. Not measured on a board. */
#ifndef LV_DRAW_ORTHO_PAIR_MIN_PX
#define LV_DRAW_ORTHO_PAIR_MIN_PX 8192
#endif

/** An image carrying this flag stores its pixels in the panel's native
 *  orientation: `header.w` x `header.h` is still its size as the UI sees it,
 *  but row r of the data is native row r of what it will become on the glass.
 *  Drawn on a turned display, unscaled and opaque, it is a plain row copy. */
#define LV_DRAW_ORTHO_IMAGE_FLAG_NATIVE LV_IMAGE_FLAGS_USER1

/** Counters, so a board can say where its time goes. */
typedef struct {
    uint32_t fills_direct;     /**< solid fills drawn as native rectangles */
    uint32_t fills_handed;     /**< of those, filled by a filler (the PPA) rather than the CPU */
    uint32_t fills_hidden;     /**< of those, not drawn at all: an opaque picture drawn later covers them */
    uint32_t fills_trimmed;    /**< of those, drawn only round an opaque picture drawn later over the rest */
    uint32_t images_turned;    /**< images copied with their turn in one pass */
    uint32_t images_native;    /**< pre-turned images copied row by row */
    uint32_t tiles;            /**< upright tiles drawn by the software renderer */
    uint32_t tiles_unread;     /**< of which, painted over wholly so never read back */
    uint32_t batches;          /**< runs of tasks drawn through one set of tiles */
    uint32_t tasks_general;    /**< tasks drawn through tiles */
    uint32_t tasks_refused;    /**< tasks whose matrix was not a quarter turn */
    uint32_t window_skips;     /**< writes cut around the video window */
    uint64_t bytes_turned;     /**< bytes moved through a turn */
    uint64_t bytes_straight;   /**< bytes moved without one */
    uint64_t bytes_drawn;      /**< bytes LVGL's renderer wrote into tiles, overdraw included */
    uint64_t bytes_handed;     /**< of bytes_turned, those a turner (the PPA) moved rather than the CPU */
    uint32_t waits;            /**< times the CPU waited for a turner */
    uint8_t tile_internal;     /**< 1 when the tile is in internal RAM rather than PSRAM */
    uint8_t tiles_in_turn;     /**< 2 when one tile is drawn while the other is turned, 4 when a
                                    pair is drawn on two cores while the last pair is turned */
    uint8_t cores;             /**< 2 when a second core paints every other tile */
    uint32_t tiles_other_core; /**< of `tiles`, those the second core painted */
    uint8_t turner_failed;     /**< 1, for good, once a turner stopped answering: the CPU turns
                                    from then on, and the screen must be drawn again once */
    uint64_t bytes_straight_handed; /**< of bytes_straight, those a turner (the PPA) copied */
    uint32_t images_cached;    /**< pictures drawn from a copy turned once (lv_draw_ortho_set_image_cache) */
    uint32_t image_cache_builds; /**< pictures turned into the cache */
    uint32_t image_cache_bytes; /**< what the turned copies hold now */
    uint64_t bytes_turned_images; /**< of bytes_turned, pictures turned in one pass: a camera frame drawn by
                                       LVGL is here, once per frame shown when nothing draws it twice */
    uint64_t bytes_read_back;  /**< of bytes_turned, what was under a tile, turned back into it before
                                    the tile was drawn; the rest of bytes_turned is tiles going out */
    uint32_t images_again;     /**< of images_turned, the same picture (same pixels in memory) as the one
                                    turned just before it: a camera frame drawn a second time */
    uint32_t images_in_layer;  /**< of images_turned, those turned into a layer of LVGL's (a widget drawn
                                    apart, then drawn again as a whole) rather than the screen */
    uint64_t wait_us;          /**< time the CPU spent in `waits`, in microseconds: the part of a frame
                                    that is the turner's and not the drawing's */
    uint32_t images_flash;     /**< big pictures in the flash left to the tiles: the CPU reads the flash
                                    faster than the turner does (lv_draw_ortho_set_flash_cb) */
    uint32_t sync_noted;       /**< LVGL's copies between frame buffers written down (lv_draw_ortho_take_sync) */
    uint64_t sync_bytes_noted; /**< what LVGL would have copied */
    uint32_t sync_copies;      /**< copies actually made, each a rectangle the next frame does not draw */
    uint64_t sync_bytes_copied; /**< what they copied: the rest of sync_bytes_noted was never needed */
    uint64_t sync_bytes_handed; /**< of those, bytes the turner (the PPA) copied */
    uint32_t sync_others;      /**< other copies between those frame buffers, made at once (esp_lvgl_adapter's
                                    own, bringing a third panel buffer up to date) */
    uint64_t sync_others_handed; /**< bytes of those the turner copied */
    uint8_t helper_cores;      /**< one bit per core a task doing the second core's work runs on
                                    (ESP32): work goes to the one on the other core than the
                                    drawing task, which may be either */
} lv_draw_ortho_stats_t;

/** Called with a straight (unturned) copy so a platform can hand it to DMA.
 *  Must copy `h` rows of `row_bytes` and return true, or return false to let
 *  the CPU do it. */
typedef bool (*lv_draw_ortho_copy_cb_t)(void * dest, int32_t dest_stride,
                                        const void * src, int32_t src_stride,
                                        int32_t row_bytes, int32_t h);

/** A picture in memory, for a turner: `w` x `h` pixels, `stride` bytes a row. */
typedef struct {
    void * buf;
    uint32_t stride;
    uint32_t w, h;
} lv_draw_ortho_pic_t;

/**
 * Hand a turn to something other than the CPU -- the ESP32-P4's PPA, see
 * lv_draw_ortho_ppa.h. Turn `block` of `src` by `rot` (the display's rotation
 * when going onto the panel, its opposite when coming back) and put it in
 * `dst` with its top-left corner at (dx, dy). Return a ticket, counting up
 * from 1, once it is QUEUED -- or 0 to decline, and the CPU does it.
 */
typedef uint32_t (*lv_draw_ortho_turn_cb_t)(const lv_draw_ortho_pic_t * src, const lv_area_t * block,
                                            const lv_draw_ortho_pic_t * dst, int32_t dx, int32_t dy,
                                            lv_display_rotation_t rot, lv_color_format_t cf);

/** Return true once the turn with this ticket, and every one queued before
 *  it, has finished and its pixels are in memory -- or false if they never
 *  will (a turner that has stopped answering). On false, lv_draw_ortho turns
 *  on the CPU from then on and reports it in `turner_failed`: what was
 *  queued is lost, and the screen has to be drawn again. */
typedef bool (*lv_draw_ortho_wait_cb_t)(uint32_t ticket);

/**
 * Turn tiles with `turn` instead of the CPU, NULL for the CPU again. With a
 * turner, lv_draw_ortho draws one tile while the last one is being turned, so
 * it takes a second tile (internal RAM only; without room for one it waits
 * instead). Nothing it queued is left running when it hands a layer back.
 */
void lv_draw_ortho_set_turner(lv_draw_ortho_turn_cb_t turn, lv_draw_ortho_wait_cb_t wait);

/**
 * Lend the first tile to a turner while it turns a picture handed to it whole
 * (an image drawn straight into the frame buffer): no tile is drawn then, and
 * the other core is idle. `loan` is called with the tile's memory before the
 * picture's first turn, and with NULL, 0 once its last has finished. Turns
 * queued before may still read the tile when it is lent, and the CPU's lines
 * of it may still be dirty in the cache: a turner writing to it by DMA waits
 * for the first and writes back and drops the second. Only a tile in internal
 * RAM is lent. NULL stops lending.
 */
typedef void (*lv_draw_ortho_loan_cb_t)(void * mem, size_t bytes);
void lv_draw_ortho_set_loan(lv_draw_ortho_loan_cb_t loan);

/**
 * Fill a rectangle of `dst` with one opaque colour, by DMA -- the ESP32-P4's
 * PPA. Asked only for what LVGL would fill solid straight into the frame
 * buffer (no gradient, no rounded corner, fully opaque); returns once it is
 * in memory, or false to let the CPU do it.
 */
typedef bool (*lv_draw_ortho_fill_cb_t)(const lv_draw_ortho_pic_t * dst, const lv_area_t * rect, lv_color_t color,
                                        lv_color_format_t cf);
void lv_draw_ortho_set_filler(lv_draw_ortho_fill_cb_t fill);

/**
 * Paint every other tile on a second core: 2 to start it, 1 to stop it.
 * Only LVGL's software renderer runs there, into a tile of its own; the
 * frame buffer and the turner stay with the core LVGL runs on. Needs an OS
 * (LV_USE_OS: FreeRTOS on an ESP32 with two cores, pthreads elsewhere),
 * because two renderers at once are safe only behind LVGL's own locks, and
 * a tile of internal RAM more (three more with a turner). Returns false and
 * stays on one core when either is missing.
 */
bool lv_draw_ortho_set_cores(uint32_t cores);

/** Create the draw unit. Call after `lv_init()`. */
void lv_draw_ortho_init(void);

void lv_draw_ortho_set_copy_cb(lv_draw_ortho_copy_cb_t cb);

/** Turn the fast paths off, so every task goes through tiles. For tests. */
void lv_draw_ortho_set_fast_paths(bool enable);

/** Draw every task on its own instead of in runs. For tests. */
void lv_draw_ortho_set_batching(bool enable);

/**
 * Keep LVGL out of one rectangle of a display's frame buffer, so a video
 * source -- a camera through the ESP32-P4's PPA, a JPEG decoder -- can write
 * its frames straight into the frame buffer, the way Espressif's own camera
 * examples reach the sensor's frame rate, while LVGL draws everything else.
 *
 * `native_area` is in the PANEL's coordinates (see lv_draw_ortho_native_area).
 * Works only in LV_DISPLAY_RENDER_MODE_DIRECT, where LVGL renders into the
 * frame buffer itself. One window; NULL clears it.
 *
 * The source and LVGL must not both be writing at once, and on a chip with a
 * data cache the source's frames must be made visible to the CPU (and LVGL's
 * written back) before the other side runs -- a cache line shared across the
 * window's edge would otherwise put stale pixels back. That is the caller's,
 * and the README says how on an ESP32-P4.
 */
void lv_draw_ortho_set_window(lv_display_t * disp, const lv_area_t * native_area);

/** Where an area of the UI (an object's coordinates) lands on the panel. */
void lv_draw_ortho_native_area(lv_display_t * disp, const lv_area_t * area, lv_area_t * native_area);

/**
 * Keep up to `bytes` of pictures turned once, 0 (the default) to keep none
 * and free them.
 *
 * Every time a picture is drawn on a turned display it is turned again, and a
 * turn is the slowest thing the PPA does: a wallpaper under a clock is turned
 * whole once a second. With a cache, the second time an opaque, unscaled
 * picture of at least LV_DRAW_ORTHO_ALONE_BYTES is drawn it is turned once
 * into a buffer of its own (PSRAM, from lv_malloc) and every draw after that
 * is a straight copy -- by the PPA too, at angle 0, when there is one. The
 * least recently drawn go first when the budget is spent; a picture bigger
 * than the budget is never kept.
 *
 * Only pictures that cannot change are kept: on an ESP32 those in the flash
 * (a picture compiled in, an ESPHome `image:`), never a draw buffer marked
 * LV_IMAGE_FLAGS_MODIFIABLE (a canvas). See the two calls below to widen that.
 */
void lv_draw_ortho_set_image_cache(size_t bytes);

/** Forget the turned copy of the picture whose pixels are at `data` -- after
 *  changing them in place -- or of every picture with NULL. */
void lv_draw_ortho_image_cache_drop(const void * data);

/** Whether a picture's pixels never change, so its turned copy may be kept.
 *  Never asked about one marked LV_IMAGE_FLAGS_MODIFIABLE. */
typedef bool (*lv_draw_ortho_constant_cb_t)(const lv_image_dsc_t * img);

/** Decide which pictures may be kept instead of "in the flash" (NULL for that
 *  again). Forgets everything kept so far. */
void lv_draw_ortho_set_image_constant_cb(lv_draw_ortho_constant_cb_t cb);

/**
 * Take over LVGL's copies between `disp`'s frame buffers: DIRECT mode with
 * two or three of them, where LVGL brings the buffer it draws into up to date
 * at the start of every refresh -- by the CPU, and before it knows whether
 * anything will be drawn at all. Each copy is written down instead, and when
 * a buffer is about to be drawn into, only what it is behind by and the frame
 * does not draw is copied, from the buffer shown last, by the turner (the
 * PPA) when there is one. LVGL itself is not changed: it makes those copies
 * through the buffers' buf_copy_cb, which this replaces.
 *
 * Call after the display's buffers are set (lv_display_set_buffers, and
 * lv_display_set_3rd_draw_buffer if any). Buffers set again later are taken
 * again at the next frame. Returns false, and takes nothing, for any other
 * mode, a single buffer, or (LVGL 9.6) a display with a sync_cb of its own.
 * One display at a time.
 */
bool lv_draw_ortho_take_sync(lv_display_t * disp);

/** Give LVGL its copies back (NULL: whichever display has them). */
void lv_draw_ortho_release_sync(lv_display_t * disp);

/**
 * Decide which pictures have their pixels in the flash instead of "on an
 * ESP32, those at a flash address" (NULL for that again).
 *
 * A turner is never handed such a picture: the PPA reads the flash through
 * the same cache as the CPU and gets less out of it -- measured, 224 ms
 * against 171 for a 1024x600 picture on a Waveshare 7B, 338 against 239 for
 * 1280x720 on a Tab5. The CPU draws it into the tiles instead, on both cores
 * when there are two, and reads it once into the picture cache when there is
 * one.
 */
void lv_draw_ortho_set_flash_cb(lv_draw_ortho_constant_cb_t cb);

const lv_draw_ortho_stats_t * lv_draw_ortho_get_stats(void);
void lv_draw_ortho_reset_stats(void);

#endif /* LV_USE_DRAW_ORTHO */

#ifdef __cplusplus
}
#endif

#endif /* LV_DRAW_ORTHO_H */
