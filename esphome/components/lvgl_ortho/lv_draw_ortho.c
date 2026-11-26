/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 youkorr
 * Part of ppa-alternative: https://github.com/youkorr/ppa-alternative-
 */

/**
 * @file lv_draw_ortho.c
 *
 * See lv_draw_ortho.h for what this is for. The comments here are about how.
 */

#include "lv_draw_ortho.h"

#if LV_USE_DRAW_ORTHO

#include "lvgl_private.h"
#include <math.h>
#include <string.h>
#if defined(ESP_PLATFORM)
#include "esp_memory_utils.h"
#endif

/* LVGL 10 renamed the draw task types after their descriptors. */
#if LVGL_VERSION_MAJOR < 10
#define LV_DRAW_DSC_TYPE_FILL           LV_DRAW_TASK_TYPE_FILL
#define LV_DRAW_DSC_TYPE_BORDER         LV_DRAW_TASK_TYPE_BORDER
#define LV_DRAW_DSC_TYPE_BOX_SHADOW     LV_DRAW_TASK_TYPE_BOX_SHADOW
#define LV_DRAW_DSC_TYPE_LETTER         LV_DRAW_TASK_TYPE_LETTER
#define LV_DRAW_DSC_TYPE_LABEL          LV_DRAW_TASK_TYPE_LABEL
#define LV_DRAW_DSC_TYPE_IMAGE          LV_DRAW_TASK_TYPE_IMAGE
#define LV_DRAW_DSC_TYPE_ARC            LV_DRAW_TASK_TYPE_ARC
#define LV_DRAW_DSC_TYPE_LINE           LV_DRAW_TASK_TYPE_LINE
#define LV_DRAW_DSC_TYPE_BLUR           LV_DRAW_TASK_TYPE_BLUR
#define LV_DRAW_DSC_TYPE_TRIANGLE       LV_DRAW_TASK_TYPE_TRIANGLE
#define LV_DRAW_DSC_TYPE_LAYER          LV_DRAW_TASK_TYPE_LAYER
#define LV_DRAW_DSC_TYPE_MASK_RECTANGLE LV_DRAW_TASK_TYPE_MASK_RECTANGLE
#define LV_DRAW_DSC_TYPE_VECTOR         LV_DRAW_TASK_TYPE_VECTOR
#endif

/* Any id no other unit uses. LVGL's own units count up from 1. */
#define DRAW_UNIT_ID_ORTHO 79

/**
 * A quarter-turn matrix reduced to integers, pixel for pixel:
 *     native_x = a * x + b * y + tx
 *     native_y = c * x + d * y + ty
 * The matrix LVGL hands over maps pixel EDGES (a vector renderer's view), so
 * a pixel's centre x + 0.5 lands on native_x + 0.5. Where a row of the matrix
 * sums to -1 that moves the integer result down by one; `ortho_from_matrix`
 * folds that in so nothing downstream has to know.
 */
typedef struct {
    int32_t a, b, c, d;
    int32_t tx, ty;
} ortho_t;

typedef struct _job_t job_t;

typedef struct {
    lv_draw_unit_t base;
    lv_draw_task_t * task_act;
    job_t * jobs;            /* LV_DRAW_ORTHO_MAX_BATCH */
    /* Aligned to a cache line. The second with a turner (one is drawn while
     * the other is being turned) or a second core (each draws one); the third
     * and fourth with both, so a pair is drawn while the last pair is being
     * turned. They are cut out of the blocks in tile_alloc, of
     * LV_DRAW_ORTHO_TILE_BYTES each, the first ones first: a block per tile
     * while internal RAM gives them, then each block shared (share_tiles). */
    uint8_t * tile[4];
    void * tile_alloc[4];
    uint32_t ticket[4];      /* the last turn reading each tile, 0 if none */
    int32_t next;
    int32_t pair;
    /* What a tile holds: LV_DRAW_ORTHO_TILE_BYTES, or a half or a quarter of
     * it when there are fewer blocks than tiles. */
    size_t tile_bytes;
    /* One text draw at a time, whichever core it is on (paint_tile). */
    lv_mutex_t text_mutex;
    bool text_mutex_ready;
} ortho_unit_t;

static ortho_unit_t * g_unit;
static lv_draw_ortho_stats_t g_stats;
static lv_draw_ortho_copy_cb_t g_copy_cb;
static bool g_fast = true;
static bool g_batch = true;
static bool g_tile_internal;

/* A turner (the PPA) and the last ticket it was handed that may still be
 * running, 0 when none is. */
static lv_draw_ortho_turn_cb_t g_turn;
static lv_draw_ortho_wait_cb_t g_wait;
static lv_draw_ortho_fill_cb_t g_fill;
static uint32_t g_last;
/* Who borrows the first tile while a picture is turned whole
 * (lv_draw_ortho_set_loan). */
static lv_draw_ortho_loan_cb_t g_loan;
static const void * g_last_picture;   /* the pixels of the last picture turned, for images_again */
static bool g_turner_failed;

/* A turner writes by DMA, and its output has to start on a cache line: the
 * ESP32-P4's PPA wants its L2's, which is at most 128 bytes. */
#define TILE_ALIGN 128

/* The video window: a native rectangle of one display's frame buffer that
 * LVGL leaves to a source writing there itself. */
static lv_display_t * g_win_disp;
static lv_area_t g_win;

/* ------------------------------------------------------------------------ */
/* The matrix                                                               */
/* ------------------------------------------------------------------------ */

static bool near_int(float v, int32_t * out)
{
    float r = roundf(v);
    if(fabsf(v - r) > 1e-3f) return false;
    *out = (int32_t)r;
    return true;
}

static bool ortho_from_matrix(const lv_matrix_t * m, ortho_t * o)
{
    int32_t tx, ty;
    if(!near_int(m->m[0][0], &o->a) || !near_int(m->m[0][1], &o->b) ||
       !near_int(m->m[1][0], &o->c) || !near_int(m->m[1][1], &o->d) ||
       !near_int(m->m[0][2], &tx) || !near_int(m->m[1][2], &ty)) return false;

    /* No perspective. */
    if(fabsf(m->m[2][0]) > 1e-3f || fabsf(m->m[2][1]) > 1e-3f || fabsf(m->m[2][2] - 1.0f) > 1e-3f) return false;

    /* A signed permutation: exactly one +-1 per row and per column. */
    if(LV_ABS(o->a) + LV_ABS(o->b) != 1 || LV_ABS(o->c) + LV_ABS(o->d) != 1) return false;
    if(LV_ABS(o->a) + LV_ABS(o->c) != 1 || LV_ABS(o->b) + LV_ABS(o->d) != 1) return false;

    o->tx = tx + ((o->a + o->b) == 1 ? 0 : -1);
    o->ty = ty + ((o->c + o->d) == 1 ? 0 : -1);
    return true;
}

static inline bool ortho_is_identity(const ortho_t * o)
{
    return o->a == 1 && o->b == 0 && o->c == 0 && o->d == 1 && o->tx == 0 && o->ty == 0;
}

static inline void ortho_point(const ortho_t * o, int32_t x, int32_t y, int32_t * nx, int32_t * ny)
{
    *nx = o->a * x + o->b * y + o->tx;
    *ny = o->c * x + o->d * y + o->ty;
}

/* The inverse of a signed permutation is its transpose. */
static inline void ortho_unpoint(const ortho_t * o, int32_t nx, int32_t ny, int32_t * x, int32_t * y)
{
    int32_t u = nx - o->tx, v = ny - o->ty;
    *x = o->a * u + o->c * v;
    *y = o->b * u + o->d * v;
}

static void ortho_area(const ortho_t * o, const lv_area_t * in, lv_area_t * out)
{
    int32_t x1, y1, x2, y2;
    ortho_point(o, in->x1, in->y1, &x1, &y1);
    ortho_point(o, in->x2, in->y2, &x2, &y2);
    out->x1 = LV_MIN(x1, x2);
    out->x2 = LV_MAX(x1, x2);
    out->y1 = LV_MIN(y1, y2);
    out->y2 = LV_MAX(y1, y2);
}

static void ortho_unarea(const ortho_t * o, const lv_area_t * in, lv_area_t * out)
{
    int32_t x1, y1, x2, y2;
    ortho_unpoint(o, in->x1, in->y1, &x1, &y1);
    ortho_unpoint(o, in->x2, in->y2, &x2, &y2);
    out->x1 = LV_MIN(x1, x2);
    out->x2 = LV_MAX(x1, x2);
    out->y1 = LV_MIN(y1, y2);
    out->y2 = LV_MAX(y1, y2);
}

/* ------------------------------------------------------------------------ */
/* The video window                                                         */
/* ------------------------------------------------------------------------ */

/* Whether writes into this layer must keep out of the window: only the
 * display's own layer, rendered straight into its frame buffer. A child
 * layer is a separate buffer, and in partial mode the band is copied by the
 * flush callback, which is where a window would have to be honoured. */
static bool has_window(const lv_layer_t * layer)
{
    if(g_win_disp == NULL || layer->parent != NULL) return false;
    if(lv_refr_get_disp_refreshing() != g_win_disp) return false;
    return lv_display_get_render_mode(g_win_disp) == LV_DISPLAY_RENDER_MODE_DIRECT;
}

/* The parts of a native rectangle outside the window: up to four. */
static int32_t outside_window(const lv_layer_t * layer, const lv_area_t * nat, lv_area_t out[4])
{
    lv_area_t w;
    if(!has_window(layer) || !lv_area_intersect(&w, nat, &g_win)) {
        out[0] = *nat;
        return 1;
    }
    int32_t n = 0;
    if(nat->y1 < w.y1) lv_area_set(&out[n++], nat->x1, nat->y1, nat->x2, w.y1 - 1);
    if(nat->y2 > w.y2) lv_area_set(&out[n++], nat->x1, w.y2 + 1, nat->x2, nat->y2);
    if(nat->x1 < w.x1) lv_area_set(&out[n++], nat->x1, w.y1, w.x1 - 1, w.y2);
    if(nat->x2 > w.x2) lv_area_set(&out[n++], w.x2 + 1, w.y1, nat->x2, w.y2);
    g_stats.window_skips++;
    return n;
}

/* ------------------------------------------------------------------------ */
/* Moving pixels                                                            */
/* ------------------------------------------------------------------------ */

static void copy_straight(uint8_t * dest, int32_t dest_stride, const uint8_t * src, int32_t src_stride,
                          int32_t row_bytes, int32_t h)
{
    g_stats.bytes_straight += (uint64_t)row_bytes * (uint64_t)h;
    if(g_copy_cb && g_copy_cb(dest, dest_stride, src, src_stride, row_bytes, h)) return;
    for(int32_t r = 0; r < h; r++) {
        lv_memcpy(dest, src, row_bytes);
        dest += dest_stride;
        src += src_stride;
    }
}

/* Square blocks a transpose works in. A native row walks a column of the
 * upright side, touching one cache line per pixel; done a block at a time,
 * the BLOCK lines a block touches are still cached when the next native row
 * of the same block wants the pixel beside the one it just read. */
#ifndef LV_DRAW_ORTHO_BLOCK
#define LV_DRAW_ORTHO_BLOCK 32
#endif

/**
 * Move the pixels of a NATIVE rectangle between the native buffer and an
 * upright one, in either direction. `up` addresses upright pixel (x, y) as
 * up + (y - up_area->y1) * up_stride + (x - up_area->x1) * px.
 */
static void move_turned_rows(const ortho_t * o, int32_t px,
                             uint8_t * nat, int32_t nat_stride, const lv_area_t * nat_buf_area,
                             uint8_t * up, int32_t up_stride, const lv_area_t * up_area,
                             const lv_area_t * nat_rect, bool to_native, int32_t block)
{
    /* One step right in native space is (a, b) in upright space. */
    const intptr_t up_step = (intptr_t)o->a * px + (intptr_t)o->b * up_stride;

    /* Two native rows next to each other are two upright columns next to each
     * other, so in 16 bits one 32-bit read of the upright side carries a pixel
     * of each, and two such reads a column apart fill a 32-bit write of each
     * native row: a quarter of the loads and stores of a pixel at a time. A
     * P4 core is in order and has no wider load, so this is where its turn
     * spends its time. */
    const int32_t col_step = o->c * px + o->d * up_stride;  /* one native row down, upright */
    const bool quad = px == 2 && (col_step == 2 || col_step == -2) &&
                      (up_step == up_stride || up_step == -up_stride);

    for(int32_t by = nat_rect->y1; by <= nat_rect->y2; by += block) {
        const int32_t ey = LV_MIN(by + block - 1, nat_rect->y2);
        for(int32_t bx = nat_rect->x1; bx <= nat_rect->x2; bx += block) {
            const int32_t w = LV_MIN(bx + block - 1, nat_rect->x2) - bx + 1;
            int32_t ny = by;
            if(quad) {
                for(; ny + 1 <= ey; ny += 2) {
                    int32_t x, y;
                    ortho_unpoint(o, bx, ny, &x, &y);
                    uint8_t * u = up + (intptr_t)(y - up_area->y1) * up_stride + (intptr_t)(x - up_area->x1) * px;
                    uint16_t * n0 = (uint16_t *)(nat + (intptr_t)(ny - nat_buf_area->y1) * nat_stride +
                                                 (intptr_t)(bx - nat_buf_area->x1) * px);
                    uint16_t * n1 = (uint16_t *)((uint8_t *)n0 + nat_stride);
                    /* The pair in upright memory: row ny at the lower address
                     * when the next native row is one pixel right upright.
                     * Halves of a 32-bit word in little-endian order, which
                     * the P4 and every host this is tested on are. */
                    uint8_t * pair = col_step > 0 ? u : u - 2;
                    int32_t i = 0;
                    if(((uintptr_t)pair & 3) == 0 && ((uintptr_t)n0 & 3) == 0 && (nat_stride & 3) == 0 &&
                       (up_stride & 3) == 0) {
                        const intptr_t two = 2 * up_step;
                        for(; i + 1 < w; i += 2, pair += two) {
                            uint32_t * a = (uint32_t *)pair;
                            uint32_t * b = (uint32_t *)(pair + up_step);
                            uint32_t * w0 = (uint32_t *)(n0 + i);
                            uint32_t * w1 = (uint32_t *)(n1 + i);
                            if(to_native) {
                                const uint32_t va = *a, vb = *b;
                                if(col_step > 0) {
                                    *w0 = (va & 0xffffu) | (vb << 16);
                                    *w1 = (va >> 16) | (vb & 0xffff0000u);
                                }
                                else {
                                    *w0 = (va >> 16) | (vb & 0xffff0000u);
                                    *w1 = (va & 0xffffu) | (vb << 16);
                                }
                            }
                            else {
                                const uint32_t v0 = *w0, v1 = *w1;
                                if(col_step > 0) {
                                    *a = (v0 & 0xffffu) | (v1 << 16);
                                    *b = (v0 >> 16) | (v1 & 0xffff0000u);
                                }
                                else {
                                    *a = (v1 & 0xffffu) | (v0 << 16);
                                    *b = (v1 >> 16) | (v0 & 0xffff0000u);
                                }
                            }
                        }
                    }
                    /* What is left, and any pair that is not word-aligned. */
                    uint8_t * u0 = u + (intptr_t)i * up_step;
                    for(; i < w; i++, u0 += up_step) {
                        uint16_t * p0 = (uint16_t *)u0;
                        uint16_t * p1 = (uint16_t *)(u0 + col_step);
                        if(to_native) {
                            n0[i] = *p0;
                            n1[i] = *p1;
                        }
                        else {
                            *p0 = n0[i];
                            *p1 = n1[i];
                        }
                    }
                }
            }
            for(; ny <= ey; ny++) {
                int32_t x, y;
                ortho_unpoint(o, bx, ny, &x, &y);
                uint8_t * u = up + (intptr_t)(y - up_area->y1) * up_stride + (intptr_t)(x - up_area->x1) * px;
                uint8_t * n = nat + (intptr_t)(ny - nat_buf_area->y1) * nat_stride +
                              (intptr_t)(bx - nat_buf_area->x1) * px;
                if(px == 2) {
                    uint16_t * n16 = (uint16_t *)n;
                    if(to_native) for(int32_t i = 0; i < w; i++, u += up_step) n16[i] = *(const uint16_t *)u;
                    else for(int32_t i = 0; i < w; i++, u += up_step) *(uint16_t *)u = n16[i];
                }
                else if(px == 4) {
                    uint32_t * n32 = (uint32_t *)n;
                    if(to_native) for(int32_t i = 0; i < w; i++, u += up_step) n32[i] = *(const uint32_t *)u;
                    else for(int32_t i = 0; i < w; i++, u += up_step) *(uint32_t *)u = n32[i];
                }
                else {
                    for(int32_t i = 0; i < w; i++, u += up_step, n += px) {
                        if(to_native) lv_memcpy(n, u, px);
                        else lv_memcpy(u, n, px);
                    }
                }
            }
        }
    }
}

/* move_turned_rows, counted. */
static void move_turned(const ortho_t * o, int32_t px,
                        uint8_t * nat, int32_t nat_stride, const lv_area_t * nat_buf_area,
                        uint8_t * up, int32_t up_stride, const lv_area_t * up_area,
                        const lv_area_t * nat_rect, bool to_native)
{
    g_stats.bytes_turned += (uint64_t)lv_area_get_width(nat_rect) *
                            (uint64_t)lv_area_get_height(nat_rect) * (uint64_t)px;
    move_turned_rows(o, px, nat, nat_stride, nat_buf_area, up, up_stride, up_area, nat_rect, to_native,
                     LV_DRAW_ORTHO_BLOCK);
}

/* ------------------------------------------------------------------------ */
/* Turning with a turner                                                    */
/* ------------------------------------------------------------------------ */

/* Wait for everything handed to the turner. Whatever the CPU is about to read
 * or write in a buffer a queued turn may still be reading or writing, it does
 * after this -- and it is done at the end of every run of tasks, so nothing is
 * left running when LVGL takes its layer back. */
/* A turner that stopped answering: the CPU from now on. What it had queued
 * is lost, so the screen has to be drawn again -- lvgl_ortho does, seeing
 * turner_failed; nothing can be invalidated from inside a refresh. */
static void turner_failed(void)
{
    LV_LOG_ERROR("ortho: the turner stopped answering; the CPU turns from now on");
    g_turn = NULL;
    g_wait = NULL;
    g_last = 0;
    g_turner_failed = true;
}

/* A clock, for how long the CPU waits for the turner. */
#if defined(LV_DRAW_ORTHO_SIM_CORES)
/* The simulated P4 has no clock to ask: its waits count as nothing. */
static int64_t ortho_now_us(void)
{
    return 0;
}
#elif defined(ESP_PLATFORM)
#include "esp_timer.h"
static int64_t ortho_now_us(void)
{
    return esp_timer_get_time();
}
#else
#include <time.h>
static int64_t ortho_now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}
#endif

static bool wait_for(uint32_t t)
{
    if(t == 0 || g_wait == NULL) return true;
    g_stats.waits++;
    const int64_t t0 = ortho_now_us();
    const bool ok = g_wait(t);
    g_stats.wait_us += (uint64_t)(ortho_now_us() - t0);
    if(ok) return true;
    turner_failed();
    return false;
}

static void fence(void)
{
    wait_for(g_last);
    g_last = 0;
}

/* The display rotation a quarter-turn matrix is; false for a mirror. */
static bool ortho_rotation(const ortho_t * o, lv_display_rotation_t * rot)
{
    if(o->a == 1 && o->d == 1 && o->b == 0 && o->c == 0) *rot = LV_DISPLAY_ROTATION_0;
    else if(o->a == -1 && o->d == -1 && o->b == 0 && o->c == 0) *rot = LV_DISPLAY_ROTATION_180;
    else if(o->a == 0 && o->d == 0 && o->b == 1 && o->c == -1) *rot = LV_DISPLAY_ROTATION_90;
    else if(o->a == 0 && o->d == 0 && o->b == -1 && o->c == 1) *rot = LV_DISPLAY_ROTATION_270;
    else return false;
    return true;
}

static lv_display_rotation_t opposite(lv_display_rotation_t r)
{
    return r == LV_DISPLAY_ROTATION_90 ? LV_DISPLAY_ROTATION_270
           : r == LV_DISPLAY_ROTATION_270 ? LV_DISPLAY_ROTATION_90 : r;
}

/* move_turned's job handed to the turner. Returns its ticket, or 0 when there
 * is no turner or it declined -- nothing was queued then. */
static uint32_t hand_to_turner(const ortho_t * o, lv_color_format_t cf, int32_t px,
                               uint8_t * nat, int32_t nat_stride, const lv_area_t * nat_buf_area,
                               uint8_t * up, int32_t up_stride, const lv_area_t * up_area,
                               const lv_area_t * nat_rect, bool to_native)
{
    lv_display_rotation_t rot;
    if(g_turn && ortho_rotation(o, &rot)) {
        lv_area_t up_rect, blk;
        ortho_unarea(o, nat_rect, &up_rect);
        const lv_draw_ortho_pic_t np = {nat, (uint32_t)nat_stride, (uint32_t)(nat_stride / px),
                                        (uint32_t)lv_area_get_height(nat_buf_area)
                                       };
        const lv_draw_ortho_pic_t upic = {up, (uint32_t)up_stride, (uint32_t)(up_stride / px),
                                          (uint32_t)lv_area_get_height(up_area)
                                         };
        uint32_t t;
        if(to_native) {
            blk = up_rect;
            lv_area_move(&blk, -up_area->x1, -up_area->y1);
            t = g_turn(&upic, &blk, &np, nat_rect->x1 - nat_buf_area->x1, nat_rect->y1 - nat_buf_area->y1, rot, cf);
        }
        else {
            blk = *nat_rect;
            lv_area_move(&blk, -nat_buf_area->x1, -nat_buf_area->y1);
            t = g_turn(&np, &blk, &upic, up_rect.x1 - up_area->x1, up_rect.y1 - up_area->y1, opposite(rot), cf);
        }
        if(t) {
            const uint64_t bytes = (uint64_t)lv_area_get_size(nat_rect) * (uint64_t)px;
            g_stats.bytes_turned += bytes;
            g_stats.bytes_handed += bytes;
            g_last = t;
            return t;
        }
    }
    return 0;
}

/* move_turned's job, handed to the turner if there is one and it takes it;
 * else done by the CPU, after everything queued has finished. Returns the
 * turner's ticket, 0 when the CPU did it. */
static uint32_t turn_or_cpu(const ortho_t * o, lv_color_format_t cf, int32_t px,
                            uint8_t * nat, int32_t nat_stride, const lv_area_t * nat_buf_area,
                            uint8_t * up, int32_t up_stride, const lv_area_t * up_area,
                            const lv_area_t * nat_rect, bool to_native)
{
    const uint32_t t = hand_to_turner(o, cf, px, nat, nat_stride, nat_buf_area, up, up_stride, up_area, nat_rect,
                                      to_native);
    if(t) return t;
    fence();
    move_turned(o, px, nat, nat_stride, nat_buf_area, up, up_stride, up_area, nat_rect, to_native);
    return 0;
}

static bool wait_ticket(uint32_t * t)
{
    const bool ok = wait_for(*t);
    *t = 0;
    return ok;
}

/* ------------------------------------------------------------------------ */
/* The software renderer, run on a copy of the task                         */
/* ------------------------------------------------------------------------ */

#if LVGL_VERSION_MAJOR >= 10
/* LVGL 10 draws a descriptor onto a layer within a clip area: no task. */
static void sw_execute_on(const lv_draw_task_t * t, lv_layer_t * layer, const lv_area_t * area,
                          const lv_area_t * clip)
{
    void * d = t->draw_dsc;
    switch(t->type) {
        case LV_DRAW_DSC_TYPE_FILL:
            lv_draw_sw_fill(layer, clip, d, area);
            break;
        case LV_DRAW_DSC_TYPE_BORDER:
            lv_draw_sw_border(layer, clip, d, area);
            break;
        case LV_DRAW_DSC_TYPE_BOX_SHADOW:
            lv_draw_sw_box_shadow(layer, clip, d, area);
            break;
        case LV_DRAW_DSC_TYPE_LETTER:
            lv_draw_sw_letter(layer, clip, d, area);
            break;
        case LV_DRAW_DSC_TYPE_LABEL:
            lv_draw_sw_label(layer, clip, d, area);
            break;
        case LV_DRAW_DSC_TYPE_IMAGE:
            lv_draw_sw_image(layer, clip, d, area);
            break;
        case LV_DRAW_DSC_TYPE_ARC:
            lv_draw_sw_arc(layer, clip, d, area);
            break;
        case LV_DRAW_DSC_TYPE_LINE:
            lv_draw_line_iterate(layer, clip, d, lv_draw_sw_line, NULL);
            break;
        case LV_DRAW_DSC_TYPE_BLUR:
            lv_draw_sw_blur(layer, clip, d, area);
            break;
        case LV_DRAW_DSC_TYPE_TRIANGLE:
            lv_draw_sw_triangle(layer, clip, d);
            break;
        case LV_DRAW_DSC_TYPE_LAYER:
            lv_draw_sw_layer(layer, clip, d, area);
            break;
        case LV_DRAW_DSC_TYPE_MASK_RECTANGLE:
            lv_draw_sw_mask_rect(layer, clip, d);
            break;
#if LV_USE_VECTOR_GRAPHIC && LV_USE_THORVG
        case LV_DRAW_DSC_TYPE_VECTOR:
            lv_draw_sw_vector(layer, clip, d);
            break;
#endif
        default:
            break;
    }
}
#else
/* LVGL 9 draws a task: a copy of it, pointed at the layer and the clip. */
static void sw_execute(lv_draw_task_t * t)
{
    switch(t->type) {
        case LV_DRAW_TASK_TYPE_FILL:
            lv_draw_sw_fill(t, t->draw_dsc, &t->area);
            break;
        case LV_DRAW_TASK_TYPE_BORDER:
            lv_draw_sw_border(t, t->draw_dsc, &t->area);
            break;
        case LV_DRAW_TASK_TYPE_BOX_SHADOW:
            lv_draw_sw_box_shadow(t, t->draw_dsc, &t->area);
            break;
        case LV_DRAW_TASK_TYPE_LETTER:
            lv_draw_sw_letter(t, t->draw_dsc, &t->area);
            break;
        case LV_DRAW_TASK_TYPE_LABEL:
            lv_draw_sw_label(t, t->draw_dsc, &t->area);
            break;
        case LV_DRAW_TASK_TYPE_IMAGE:
            lv_draw_sw_image(t, t->draw_dsc, &t->area);
            break;
        case LV_DRAW_TASK_TYPE_ARC:
            lv_draw_sw_arc(t, t->draw_dsc, &t->area);
            break;
        case LV_DRAW_TASK_TYPE_LINE:
            lv_draw_line_iterate(t, t->draw_dsc, lv_draw_sw_line);
            break;
        case LV_DRAW_TASK_TYPE_BLUR:
            lv_draw_sw_blur(t, t->draw_dsc, &t->area);
            break;
        case LV_DRAW_TASK_TYPE_TRIANGLE:
            lv_draw_sw_triangle(t, t->draw_dsc);
            break;
        case LV_DRAW_TASK_TYPE_LAYER:
            lv_draw_sw_layer(t, t->draw_dsc, &t->area);
            break;
        case LV_DRAW_TASK_TYPE_MASK_RECTANGLE:
            lv_draw_sw_mask_rect(t, t->draw_dsc);
            break;
#if LV_USE_VECTOR_GRAPHIC && LV_USE_THORVG
        case LV_DRAW_TASK_TYPE_VECTOR:
            lv_draw_sw_vector(t, t->draw_dsc);
            break;
#endif
        default:
            break;
    }
}

static void sw_execute_on(const lv_draw_task_t * t, lv_layer_t * layer, const lv_area_t * area,
                          const lv_area_t * clip)
{
    lv_draw_task_t copy = *t;
    copy.next = NULL;
    copy.target_layer = layer;
    copy.area = *area;
    copy.clip_area = *clip;
    lv_matrix_identity(&copy.matrix);
    sw_execute(&copy);
}
#endif

static void tile_layer(lv_layer_t * tl, lv_draw_buf_t * tb, const lv_area_t * area,
                       const lv_layer_t * target)
{
    lv_memzero(tl, sizeof(*tl));
    tl->draw_buf = tb;
    tl->buf_area = *area;
    tl->phy_clip_area = *area;
    tl->_clip_area = *area;
    tl->color_format = target->color_format;
    tl->opa = LV_OPA_COVER;
    tl->recolor = target->recolor;
    lv_matrix_identity(&tl->matrix);
}

/* ------------------------------------------------------------------------ */
/* The general path: draw upright into a tile, put it back turned           */
/* ------------------------------------------------------------------------ */

/* Up to this many consecutive tasks are drawn through one set of tiles, so a
 * screen's worth of widgets costs one turn rather than one per widget. */
#ifndef LV_DRAW_ORTHO_MAX_BATCH
#define LV_DRAW_ORTHO_MAX_BATCH 64
#endif

struct _job_t {
    lv_draw_task_t * t;
    lv_area_t todo;   /* upright: what the task may change */
    lv_area_t hole[2]; /* upright: inside `todo` but certainly not changed, or x2 < x1: a rounded
                          rectangle's middle is a cross, the rectangle less its four corners */
};

/* Whether a fill paints every pixel of `tile` opaquely, so what was under the
 * tile never shows and need not be read back. */
static bool covers(const job_t * j, const lv_area_t * tile)
{
    const lv_draw_task_t * t = j->t;
    if(t->type != LV_DRAW_DSC_TYPE_FILL || t->opa < LV_OPA_MAX) return false;
    const lv_draw_fill_dsc_t * d = t->draw_dsc;
    if(d->opa < LV_OPA_MAX) return false;
    if(d->grad.dir != LV_GRAD_DIR_NONE) {
        if(d->grad.dir != LV_GRAD_DIR_HOR && d->grad.dir != LV_GRAD_DIR_VER) return false;
        for(uint32_t i = 0; i < d->grad.stops_count; i++) if(d->grad.stops[i].opa < LV_OPA_MAX) return false;
    }
    if(!lv_area_is_in(tile, &t->clip_area, 0)) return false;
    if(!lv_area_is_in(tile, &t->area, 0)) return false;
    if(d->radius == 0) return true;
    /* Clear of the four corner squares is enough. */
    const int32_t r = LV_MIN(d->radius, LV_MIN(lv_area_get_width(&t->area), lv_area_get_height(&t->area)) / 2);
    lv_area_t a = t->area;
    a.x1 += r;
    a.x2 -= r;
    if(lv_area_is_in(tile, &a, 0)) return true;
    a = t->area;
    a.y1 += r;
    a.y2 -= r;
    return lv_area_is_in(tile, &a, 0);
}

/* The part of a task's area it is certain not to change. A border never
 * touches the middle of its rectangle; a shadow under an opaque background
 * is painted over by that background, which is the next task. Leaving these
 * out keeps a large panel from turning all its pixels for its outline, and
 * keeps a tile inside a card from being read back for a shadow it cannot see.
 * The middle of a rounded rectangle is a cross: the rectangle less its four
 * corner squares, two rectangles. */
static void cross(job_t * j, const lv_area_t * a, int32_t inset, int32_t corner, const lv_area_t * clip)
{
    for(int k = 0; k < 2; k++) {
        lv_area_t h = *a;
        lv_area_increase(&h, -(k ? corner : inset), -(k ? inset : corner));
        if(lv_area_get_width(&h) <= 0 || lv_area_get_height(&h) <= 0 || !lv_area_intersect(&h, &h, clip))
            lv_area_set(&h, 0, 0, -1, -1);
        j->hole[k] = h;
    }
}

static void find_hole(job_t * j)
{
    const lv_draw_task_t * t = j->t;
    lv_area_set(&j->hole[0], 0, 0, -1, -1);
    j->hole[1] = j->hole[0];
    if(t->type == LV_DRAW_DSC_TYPE_BORDER) {
        const lv_draw_border_dsc_t * d = t->draw_dsc;
        cross(j, &t->area, d->width, LV_MAX(d->width, d->radius), &t->area);
    }
    else if(t->type == LV_DRAW_DSC_TYPE_BOX_SHADOW) {
        const lv_draw_box_shadow_dsc_t * d = t->draw_dsc;
        /* The background is drawn a pixel inside the object when it has a
         * border (LVGL, lv_obj_draw), so it is the background's own area that
         * is covered, not the shadow's. */
        if(d->bg_cover && t->next && t->next->type == LV_DRAW_DSC_TYPE_FILL && t->next->opa >= LV_OPA_MAX) {
            const lv_draw_fill_dsc_t * f = t->next->draw_dsc;
            if(f->opa >= LV_OPA_MAX && f->grad.dir == LV_GRAD_DIR_NONE) {
                const lv_area_t * fa = &t->next->area;
                const int32_t r = LV_MIN(f->radius, LV_MIN(lv_area_get_width(fa), lv_area_get_height(fa)) / 2);
                cross(j, fa, 0, r, &t->next->clip_area);
            }
        }
    }
}

static bool job_touches(const job_t * j, const lv_area_t * tile)
{
    lv_area_t a;
    if(!lv_area_intersect(&a, &j->todo, tile)) return false;
    /* Wholly inside the hole: nothing to do here. */
    for(int k = 0; k < 2; k++)
        if(j->hole[k].x2 >= j->hole[k].x1 && lv_area_is_in(&a, &j->hole[k], 0)) return false;
    return true;
}

/* One tile on its way: prepared and read back by the first core, painted by
 * either, put back turned by the first. Painting touches nothing but the tile
 * and LVGL's own drawing state, so the other core can do it. */
typedef struct {
    const job_t * jobs;
    int32_t n, first;
    lv_area_t tile;      /* upright: the part the jobs reach */
    lv_area_t nat;
    lv_draw_buf_t tb;
    lv_layer_t * target;
    int32_t px;
    uint64_t drawn;      /* bytes the renderer wrote, for the stats */
} tile_work_t;

static bool prep_tile(const job_t * jobs, int32_t n, const ortho_t * o, const lv_area_t * whole_tile,
                      uint8_t * mem, size_t mem_size, tile_work_t * w)
{
    /* Only the part of the tile the jobs reach: a run of a scroll bar down
     * the right edge and another along the bottom spans the whole screen, and
     * turning the empty middle of it is all cost and no picture. */
    int32_t first = -1;
    lv_area_t reach = {0, 0, -1, -1};
    for(int32_t i = 0; i < n; i++) {
        lv_area_t a;
        if(!job_touches(&jobs[i], whole_tile)) continue;
        lv_area_intersect(&a, &jobs[i].todo, whole_tile);
        if(first < 0) {
            first = i;
            reach = a;
        }
        else lv_area_join(&reach, &reach, &a);
    }
    if(first < 0) return false;
    const lv_area_t * tile = &reach;

    lv_layer_t * target = jobs[first].t->target_layer;
    lv_draw_buf_t * nb = target->draw_buf;
    const lv_color_format_t cf = nb->header.cf;
    const int32_t px = lv_color_format_get_size(cf);
    const int32_t wd = lv_area_get_width(tile);
    const int32_t h = lv_area_get_height(tile);
    const uint32_t stride = lv_draw_buf_width_to_stride(wd, cf);

    if(lv_draw_buf_init(&w->tb, wd, h, cf, stride, mem, mem_size) != LV_RESULT_OK) {
        LV_LOG_WARN("ortho: tile %dx%d does not fit", (int)wd, (int)h);
        return false;
    }
    w->jobs = jobs;
    w->n = n;
    w->first = first;
    w->tile = reach;
    w->target = target;
    w->px = px;
    w->drawn = 0;
    ortho_area(o, tile, &w->nat);

    /* The tile starts as what is already under it, so blending is right --
     * unless the first thing drawn paints all of it anyway. */
    if(!covers(&jobs[first], tile)) {
        const uint64_t before = g_stats.bytes_turned;
        uint32_t t = turn_or_cpu(o, cf, px, nb->data, nb->header.stride, &target->buf_area,
                                 w->tb.data, stride, tile, &w->nat, false);
        if(!wait_ticket(&t))   /* it is drawn on next */
            move_turned(o, px, nb->data, nb->header.stride, &target->buf_area, w->tb.data, stride, tile, &w->nat,
                        false);
        g_stats.bytes_read_back += g_stats.bytes_turned - before;
    }
    else g_stats.tiles_unread++;
    return true;
}

/* No globals of this file in here but the text mutex: it may run on the
 * other core.
 *
 * A text draw is whole under that mutex, from the font being asked for a
 * glyph to its bitmap being drawn. A font engine need not be safe on two
 * cores at once, and ESPHome's is not: esphome::font::Font keeps the last
 * glyph it looked up in two plain members, so a lookup on one core can hand
 * the other the wrong glyph, whose bitmap is then written into a buffer sized
 * for another one. On a Tab5 that was a crash a few minutes into a screen with
 * a large clock under moving lights. LVGL's own locks do not reach a font.
 * Only the text waits: fills, borders, shadows and pictures of both tiles are
 * still drawn at once. (The first guard here drew a whole pair of tiles one
 * after the other when both held text. Through Espressif's adapter, the
 * commit carrying it measured a dashboard 12% slower on a Waveshare 7B and 18%
 * on a Tab5, and the guard was taken for the cause there, not measured apart;
 * the mutex round each text draw is the fix made there.) */
static void paint_tile(tile_work_t * w)
{
    lv_layer_t tl;
    tile_layer(&tl, &w->tb, &w->tile, w->target);
    for(int32_t i = w->first; i < w->n; i++) {
        lv_area_t clip, drawn;
        const job_t * j = &w->jobs[i];
        if(!job_touches(j, &w->tile)) continue;
        if(lv_area_intersect(&clip, &j->t->clip_area, &w->tile)) {
            const bool text = (j->t->type == LV_DRAW_DSC_TYPE_LABEL || j->t->type == LV_DRAW_DSC_TYPE_LETTER) &&
                              g_unit && g_unit->text_mutex_ready;
            if(text) lv_mutex_lock(&g_unit->text_mutex);
            sw_execute_on(j->t, &tl, &j->t->area, &clip);
            if(text) lv_mutex_unlock(&g_unit->text_mutex);
            if(lv_area_intersect(&drawn, &j->todo, &clip))
                w->drawn += (uint64_t)lv_area_get_size(&drawn) * (uint64_t)w->px;
        }
    }
}

static void finish_tile(const ortho_t * o, tile_work_t * w, uint32_t * ticket)
{
    lv_draw_buf_t * nb = w->target->draw_buf;
    const lv_color_format_t cf = nb->header.cf;
    g_stats.bytes_drawn += w->drawn;
    lv_area_t parts[4];
    const int32_t np = outside_window(w->target, &w->nat, parts);
    for(int32_t i = 0; i < np; i++) {
        const uint32_t t = turn_or_cpu(o, cf, w->px, nb->data, nb->header.stride, &w->target->buf_area,
                                       w->tb.data, w->tb.header.stride, &w->tile, &parts[i], true);
        if(t) *ticket = t;
    }
    g_stats.tiles++;
}

static void draw_tile(const job_t * jobs, int32_t n, const ortho_t * o, const lv_area_t * whole_tile,
                      uint8_t * mem, size_t mem_size, uint32_t * ticket)
{
    tile_work_t w;
    if(!prep_tile(jobs, n, o, whole_tile, mem, mem_size, &w)) return;
    paint_tile(&w);
    finish_tile(o, &w, ticket);
}

/* ------------------------------------------------------------------------ */
/* The other core                                                           */
/* ------------------------------------------------------------------------ */

/* A second core paints every other tile: LVGL's software renderer, into a
 * tile of its own, and nothing else. Reading the frame buffer back, turning
 * and handing work to the PPA stay on the first core, so the frame buffer
 * and the turner have one user as before.
 *
 * Two of LVGL's renderers at once is what LVGL itself does with
 * LV_DRAW_SW_DRAW_UNIT_CNT > 1, and it is safe for the same reason: its
 * shared caches (the circle cache of rounded corners, the image decoders and
 * their cache) are behind LVGL's own mutexes -- which are real only with an
 * OS. With LV_OS_NONE they compile to nothing, so then there is no second
 * core. youkorr/lvgl_9.5 runs LV_OS_FREERTOS on an ESP32. A font is not
 * behind those mutexes, and ESPHome's is not safe on two cores: each text
 * draw is under a mutex of this unit's (paint_tile), and without that mutex
 * there is no second core either. */
#if defined(LV_DRAW_ORTHO_SIM_CORES)
#define ORTHO_HELPER 1   /* the P4 simulation: run where it is joined, counted apart */
#elif LV_USE_OS == LV_OS_FREERTOS && defined(ESP_PLATFORM)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#if CONFIG_FREERTOS_NUMBER_OF_CORES > 1
#define ORTHO_HELPER 2
#endif
#elif LV_USE_OS == LV_OS_PTHREAD
#include <pthread.h>
#define ORTHO_HELPER 3
#endif
#ifndef ORTHO_HELPER
#define ORTHO_HELPER 0
#endif

/* What the other core is handed: a tile to paint. */
typedef void (*helper_fn_t)(void * arg);

#if ORTHO_HELPER == 2
typedef struct {
    TaskHandle_t task;
    SemaphoreHandle_t go, done;
    bool failed;   /* could not be started: this core is not asked again */
} helper_task_t;
#endif

typedef struct {
    bool on;
    helper_fn_t volatile fn;
    void * volatile arg;
    volatile bool quit;
#if ORTHO_HELPER == 2
    /* One task per core, pinned there. Work goes to the one on the core the
     * drawing task is NOT on: LVGL's task has no core of its own, and an
     * application may refresh from a task pinned anywhere, so a single task
     * pinned once could be on the very core that hands it the work -- and the
     * two halves were then done one after the other. The second is started
     * the first time it is needed. */
    helper_task_t on_core[2];
    int busy;             /* the core of the task doing `fn` */
    UBaseType_t prio;     /* LVGL's task's, when the first was started */
#elif ORTHO_HELPER == 3
    pthread_t thread;
    pthread_mutex_t m;
    pthread_cond_t c;
    int state;   /* 0 idle, 1 work handed over, 2 painted */
#endif
} helper_t;

static helper_t g_helper;

#if ORTHO_HELPER == 2
static void helper_main(void * arg)
{
    helper_task_t * h = arg;
    for(;;) {
        xSemaphoreTake(h->go, portMAX_DELAY);
        if(g_helper.quit) break;
        g_helper.fn(g_helper.arg);
        xSemaphoreGive(h->done);
    }
    xSemaphoreGive(h->done);
    vTaskDelete(NULL);
}

/* The task pinned to `core`, started now if it is not running yet. */
static bool helper_on(int core)
{
    helper_task_t * h = &g_helper.on_core[core];
    if(h->task) return true;
    if(h->failed) return false;
    h->go = xSemaphoreCreateBinary();
    h->done = xSemaphoreCreateBinary();
    if(h->go && h->done) {
        const uint32_t stack = LV_MAX(LV_DRAW_THREAD_STACK_SIZE, 12 * 1024);
        if(xTaskCreatePinnedToCore(helper_main, core ? "lv_ortho1" : "lv_ortho0", stack, h, g_helper.prio,
                                   &h->task, core) == pdPASS) return true;
    }
    if(h->go) vSemaphoreDelete(h->go);
    if(h->done) vSemaphoreDelete(h->done);
    h->go = h->done = NULL;
    h->task = NULL;
    h->failed = true;
    LV_LOG_WARN("ortho: the task for core %d could not be started; work handed from the other core is done "
                "there", core);
    return false;
}
#elif ORTHO_HELPER == 3
static void * helper_main(void * arg)
{
    LV_UNUSED(arg);
    pthread_mutex_lock(&g_helper.m);
    for(;;) {
        while(g_helper.state != 1 && !g_helper.quit) pthread_cond_wait(&g_helper.c, &g_helper.m);
        if(g_helper.quit) break;
        pthread_mutex_unlock(&g_helper.m);
        g_helper.fn(g_helper.arg);
        pthread_mutex_lock(&g_helper.m);
        g_helper.state = 2;
        pthread_cond_broadcast(&g_helper.c);
    }
    pthread_mutex_unlock(&g_helper.m);
    return NULL;
}
#endif

static bool helper_start_thread(void)
{
    if(g_unit == NULL || !g_unit->text_mutex_ready) return false;
#if ORTHO_HELPER == 1
    return true;
#elif ORTHO_HELPER == 2
    /* At LVGL's priority, since it is LVGL's work; the first on the core
     * LVGL is not on now. */
    g_helper.prio = uxTaskPriorityGet(NULL);
    for(int k = 0; k < 2; k++) g_helper.on_core[k].failed = false;
    return helper_on((int)xPortGetCoreID() ^ 1);
#elif ORTHO_HELPER == 3
    pthread_mutex_init(&g_helper.m, NULL);
    pthread_cond_init(&g_helper.c, NULL);
    g_helper.state = 0;
    if(pthread_create(&g_helper.thread, NULL, helper_main, NULL) == 0) return true;
    pthread_cond_destroy(&g_helper.c);
    pthread_mutex_destroy(&g_helper.m);
    return false;
#else
    return false;
#endif
}

static void helper_stop_thread(void)
{
#if ORTHO_HELPER == 2
    g_helper.quit = true;   /* the semaphore orders it before the task reads it */
    for(int k = 0; k < 2; k++) {
        helper_task_t * h = &g_helper.on_core[k];
        if(h->task) {
            xSemaphoreGive(h->go);
            xSemaphoreTake(h->done, portMAX_DELAY);
            vSemaphoreDelete(h->go);
            vSemaphoreDelete(h->done);
        }
        h->go = h->done = NULL;
        h->task = NULL;
    }
#elif ORTHO_HELPER == 3
    pthread_mutex_lock(&g_helper.m);
    g_helper.quit = true;
    pthread_cond_broadcast(&g_helper.c);
    pthread_mutex_unlock(&g_helper.m);
    pthread_join(g_helper.thread, NULL);
    pthread_cond_destroy(&g_helper.c);
    pthread_mutex_destroy(&g_helper.m);
#endif
    g_helper.quit = false;
}

#if ORTHO_HELPER == 1
/* The simulation runs one core: the other core's painting runs at the join,
 * between markers, and p4cache.c counts it apart. Markers: 2042 the other
 * core is handed its tile, 2044..2043 its painting, 2041 the join. */
#define SIM_MARK(n) __asm__ volatile("addi x0, x0, %0" :: "i"(n) : "memory")
#endif

/* Hand `fn` to the other core. False when there is no task there to take
 * it: this core does it then, after its own. */
static bool helper_run(helper_fn_t fn, void * arg)
{
#if ORTHO_HELPER == 2
    /* Read where this task runs now. A task with no core of its own may move
     * before the join: that costs the overlap, never the work, since the
     * join waits on the task the work was given to. */
    const int core = (int)xPortGetCoreID() ^ 1;
    if(!helper_on(core)) return false;
    g_helper.busy = core;
#endif
    g_helper.fn = fn;
    g_helper.arg = arg;
#if ORTHO_HELPER == 1
    SIM_MARK(2042);
#elif ORTHO_HELPER == 2
    xSemaphoreGive(g_helper.on_core[g_helper.busy].go);
#elif ORTHO_HELPER == 3
    pthread_mutex_lock(&g_helper.m);
    g_helper.state = 1;
    pthread_cond_broadcast(&g_helper.c);
    pthread_mutex_unlock(&g_helper.m);
#endif
    return true;
}

static void helper_join(void)
{
#if ORTHO_HELPER == 1
    SIM_MARK(2044);
    g_helper.fn(g_helper.arg);
    SIM_MARK(2043);
    SIM_MARK(2041);
#elif ORTHO_HELPER == 2
    xSemaphoreTake(g_helper.on_core[g_helper.busy].done, portMAX_DELAY);
#elif ORTHO_HELPER == 3
    pthread_mutex_lock(&g_helper.m);
    while(g_helper.state != 2) pthread_cond_wait(&g_helper.c, &g_helper.m);
    g_helper.state = 0;
    pthread_mutex_unlock(&g_helper.m);
#endif
    g_helper.fn = NULL;
    g_helper.arg = NULL;
}

static void paint_tile_job(void * arg)
{
    paint_tile((tile_work_t *)arg);
}

static bool helper_paint(tile_work_t * w)
{
    return helper_run(paint_tile_job, w);
}

/* Two tiles at once: the second painted by the other core while this one
 * reads back and paints the first. Its read-back goes first so the other
 * core starts as early as it can; the first's read-back then runs while the
 * other core paints. Both are turned out after the join, in order. With four
 * tiles the pairs alternate, so a pair is painted while the PPA turns out
 * the last one. */
static void draw_pair(ortho_unit_t * u, const job_t * jobs, int32_t n, const ortho_t * o, const lv_area_t rect[2])
{
    int32_t base = 0;
    if(u->tile[2] && u->tile[3]) {
        base = 2 * u->pair;
        u->pair ^= 1;
    }
    tile_work_t w[2];
    bool ok[2], handed = false;
    wait_ticket(&u->ticket[base + 1]);
    ok[1] = prep_tile(jobs, n, o, &rect[1], u->tile[base + 1], u->tile_bytes, &w[1]);
    if(ok[1]) handed = helper_paint(&w[1]);
    wait_ticket(&u->ticket[base]);
    ok[0] = prep_tile(jobs, n, o, &rect[0], u->tile[base], u->tile_bytes, &w[0]);
    if(ok[0]) paint_tile(&w[0]);
    if(handed) {
        helper_join();
        g_stats.tiles_other_core++;
    }
    else if(ok[1]) paint_tile(&w[1]);
    if(ok[0]) finish_tile(o, &w[0], &u->ticket[base]);
    if(ok[1]) finish_tile(o, &w[1], &u->ticket[base + 1]);
}

/* Whether LVGL's software renderer draws any of the jobs differently where
 * a tile cuts it: a picture or a layer it resamples -- scaled, rotated or
 * skewed -- comes out a level different on some pixels near the cut. Through
 * esp_lvgl_adapter's transformed-object suite, a scaled picture cut in two
 * for the second core put 602 pixels one level off LVGL's own. */
static bool cut_shows(const job_t * jobs, int32_t n)
{
    for(int32_t i = 0; i < n; i++) {
        const lv_draw_task_t * t = jobs[i].t;
        if(t->type != LV_DRAW_DSC_TYPE_IMAGE && t->type != LV_DRAW_DSC_TYPE_LAYER) continue;
        const lv_draw_image_dsc_t * d = t->draw_dsc;
        if(d->rotation != 0 || d->scale_x != LV_SCALE_NONE || d->scale_y != LV_SCALE_NONE || d->skew_x != 0 ||
           d->skew_y != 0) return true;
    }
    return false;
}

static void draw_jobs(ortho_unit_t * u, const job_t * jobs, int32_t n, const ortho_t * o)
{
    const lv_color_format_t cf = jobs[0].t->target_layer->draw_buf->header.cf;
    lv_area_t box = jobs[0].todo;
    for(int32_t i = 1; i < n; i++) lv_area_join(&box, &box, &jobs[i].todo);
    g_stats.tasks_general += n;

    /* A blur reads its neighbours, so it is never cut into tiles. */
    if(n == 1 && jobs[0].t->type == LV_DRAW_DSC_TYPE_BLUR) {
        const uint32_t stride = lv_draw_buf_width_to_stride(lv_area_get_width(&box), cf);
        size_t size = (size_t)stride * lv_area_get_height(&box) + LV_DRAW_BUF_ALIGN;
        void * raw = lv_malloc(size);
        if(raw == NULL) {
            LV_LOG_WARN("ortho: no memory for a blur");
            return;
        }
        uint32_t t = 0;
        draw_tile(jobs, 1, o, &box, lv_draw_buf_align(raw, cf), size - LV_DRAW_BUF_ALIGN, &t);
        fence();   /* a turner may still be reading it */
        lv_free(raw);
        return;
    }

    /* As wide as the area allows, then as many rows as fit. */
    const int32_t w = lv_area_get_width(&box);
    int32_t tw = w;
    while(tw > 1 && lv_draw_buf_width_to_stride(tw, cf) > u->tile_bytes) tw = (tw + 1) / 2;
    const uint32_t tstride = lv_draw_buf_width_to_stride(tw, cf);
    int32_t th = (int32_t)(u->tile_bytes / tstride);
    if(th < 1) th = 1;

    const bool two = g_helper.on && u->tile[1];
    /* The second core paints the second tile of a pair, so an area that fits
     * in one tile -- a card of a dashboard, a bar, a slider -- was all the
     * first core's, and so was the odd tile at the end of a larger one. Such
     * an area is cut into an even number of tiles of equal height instead:
     * each core draws half of it. Not below LV_DRAW_ORTHO_PAIR_MIN_PX, where
     * a second turn and a second pass over the tasks cost more than half the
     * drawing saves. (Through Espressif's adapter on a Waveshare 7B and a
     * Tab5, a dashboard of three cards drew 5 areas a frame, nearly all of one
     * tile: this engine at 49 and 35 frames a second against 61 and 49 for the
     * stock adapter, whose two software units share every area.) Not where a
     * cut shows (cut_shows): those keep the tiles they always had. */
    if(two && !cut_shows(jobs, n)) {
        const int32_t h = lv_area_get_height(&box);
        const int32_t cols = (w + tw - 1) / tw;
        const int32_t rows = (h + th - 1) / th;
        if(((int64_t)rows * cols) % 2 == 1 && h >= 2 &&
           (int64_t)lv_area_get_size(&box) / ((int64_t)rows * cols) >= LV_DRAW_ORTHO_PAIR_MIN_PX)
            th = (h + rows) / (rows + 1);   /* h over rows + 1 tiles, rounded up */
    }
    lv_area_t held[2];
    int32_t nheld = 0;
    for(int32_t y = box.y1; y <= box.y2; y += th) {
        for(int32_t x = box.x1; x <= box.x2; x += tw) {
            lv_area_t tile = {x, y, LV_MIN(x + tw - 1, box.x2), LV_MIN(y + th - 1, box.y2)};
            if(two) {
                held[nheld++] = tile;
                if(nheld == 2) {
                    draw_pair(u, jobs, n, o, held);
                    nheld = 0;
                }
                continue;
            }
            /* With two tiles, this one is drawn while the turner still has
             * the other: it waits only for the turns reading THIS one. */
            const int32_t k = u->next;
            if(u->tile[1]) u->next ^= 1;
            wait_ticket(&u->ticket[k]);
            draw_tile(jobs, n, o, &tile, u->tile[k], u->tile_bytes, &u->ticket[k]);
        }
    }
    if(nheld) {   /* an odd one out, drawn here */
        wait_ticket(&u->ticket[0]);
        draw_tile(jobs, n, o, &held[0], u->tile[0], u->tile_bytes, &u->ticket[0]);
    }
    fence();
    for(int i = 0; i < 4; i++) u->ticket[i] = 0;
}

/* ------------------------------------------------------------------------ */
/* Fast paths                                                               */
/* ------------------------------------------------------------------------ */

static bool plain_image(const lv_draw_task_t * t, const lv_image_dsc_t ** src_out);

/* Whether a task later in the same layer is an opaque picture that will cover
 * all of `todo`: then nothing drawn there before it can show, and a screen's
 * background under a full-screen picture -- a wallpaper, a camera frame drawn
 * by LVGL -- is a whole frame buffer written for nothing. Only tasks already
 * in the list are looked at; one not added yet, and the fill is drawn. */
static bool hidden_later(const lv_draw_task_t * t, const lv_area_t * todo)
{
    for(const lv_draw_task_t * c = t->next; c; c = c->next) {
        const lv_image_dsc_t * img;
        if(c->target_layer != t->target_layer || !plain_image(c, &img)) continue;
        if(lv_color_format_has_alpha(img->header.cf)) continue;
        lv_area_t covered;
        if(!lv_area_intersect(&covered, &c->area, &c->clip_area)) continue;
        if(lv_area_is_in(todo, &covered, 0)) return true;
    }
    return false;
}

/* The largest part of `todo` an opaque picture drawn later in the same layer
 * covers, in `out`. LVGL asks for a few pixels round what changed, so a
 * picture smaller than the screen -- a camera canvas in a page -- has its
 * background asked for in a rectangle a little wider than itself, which
 * hidden_later() cannot call hidden: before this the whole rectangle was
 * filled under the picture every frame (an 800x600 canvas: about 950 KB the
 * PPA wrote for a ring five pixels wide, then turned the picture over). */
static bool covered_later(const lv_draw_task_t * t, const lv_area_t * todo, lv_area_t * out)
{
    int64_t best = 0;
    for(const lv_draw_task_t * c = t->next; c; c = c->next) {
        const lv_image_dsc_t * img;
        if(c->target_layer != t->target_layer || !plain_image(c, &img)) continue;
        if(lv_color_format_has_alpha(img->header.cf)) continue;
        lv_area_t covered, part;
        if(!lv_area_intersect(&covered, &c->area, &c->clip_area)) continue;
        if(!lv_area_intersect(&part, &covered, todo)) continue;
        const int64_t size = (int64_t)lv_area_get_width(&part) * lv_area_get_height(&part);
        if(size > best) {
            best = size;
            *out = part;
        }
    }
    return best > 0;
}

/* `a` less `b`, which lies inside it: at most four rectangles, the bands
 * above and below whole, the sides between them. */
static int32_t area_minus(const lv_area_t * a, const lv_area_t * b, lv_area_t out[4])
{
    int32_t n = 0;
    if(b->y1 > a->y1) lv_area_set(&out[n++], a->x1, a->y1, a->x2, b->y1 - 1);
    if(b->y2 < a->y2) lv_area_set(&out[n++], a->x1, b->y2 + 1, a->x2, a->y2);
    if(b->x1 > a->x1) lv_area_set(&out[n++], a->x1, b->y1, b->x1 - 1, b->y2);
    if(b->x2 < a->x2) lv_area_set(&out[n++], b->x2 + 1, b->y1, a->x2, b->y2);
    return n;
}

/* A solid fill turned is the fill of the turned rectangle: a corner radius is
 * the same on every side, so nothing about it changes. */
static bool try_fill(lv_draw_task_t * t, const ortho_t * o, const lv_area_t * todo)
{
    if(t->type != LV_DRAW_DSC_TYPE_FILL) return false;
    lv_draw_fill_dsc_t * dsc = t->draw_dsc;
    if(dsc->grad.dir != LV_GRAD_DIR_NONE) return false;
    if(hidden_later(t, todo)) {
        g_stats.fills_direct++;
        g_stats.fills_hidden++;
        return true;
    }

    /* Only what the picture over it leaves: anything drawn under an opaque
     * picture is drawn over again, as in hidden_later(). */
    lv_area_t keep[4], covered;
    int32_t nk = 1;
    keep[0] = *todo;
    if(covered_later(t, todo, &covered)) {
        nk = area_minus(todo, &covered, keep);
        g_stats.fills_trimmed++;
    }
    lv_area_t area;
    ortho_area(o, &t->area, &area);
    /* A plain rectangle, opaque, straight into the frame buffer: the filler's
     * (the PPA's) when there is one -- a screen's background is 1.8 MB the CPU
     * would otherwise write through its cache into PSRAM. */
    lv_layer_t * target = t->target_layer;
    lv_draw_buf_t * nb = target->draw_buf;
    const lv_color_format_t cf = nb->header.cf;
    const bool plain = g_fill && dsc->radius == 0 && dsc->opa >= LV_OPA_MAX && t->opa >= LV_OPA_MAX &&
                       target->recolor.alpha == 0 &&
                       (cf == LV_COLOR_FORMAT_RGB565 || cf == LV_COLOR_FORMAT_RGB888);
    const int32_t px = lv_color_format_get_size(cf);
    const lv_draw_ortho_pic_t dst = {nb->data, nb->header.stride, nb->header.stride / (uint32_t)px,
                                     (uint32_t)lv_area_get_height(&target->buf_area)
                                    };
    for(int32_t k = 0; k < nk; k++) {
        lv_area_t clip, parts[4];
        ortho_area(o, &keep[k], &clip);
        const int32_t np = outside_window(t->target_layer, &clip, parts);
        for(int32_t i = 0; i < np; i++) {
            lv_area_t clipped;
            if(plain && lv_area_intersect(&clipped, &parts[i], &area)) {
                lv_area_t rect = clipped;
                lv_area_move(&rect, -target->buf_area.x1, -target->buf_area.y1);
                if(g_fill(&dst, &rect, dsc->color, cf)) {
                    g_stats.fills_handed++;
                    continue;
                }
            }
            sw_execute_on(t, target, &area, &parts[i]);
        }
    }
    g_stats.fills_direct++;
    return true;
}

static bool plain_image(const lv_draw_task_t * t, const lv_image_dsc_t ** src_out)
{
    if(t->type != LV_DRAW_DSC_TYPE_IMAGE) return false;
    const lv_draw_image_dsc_t * d = t->draw_dsc;
    if(lv_image_src_get_type(d->src) != LV_IMAGE_SRC_VARIABLE) return false;
    if(d->rotation != 0 || d->scale_x != LV_SCALE_NONE || d->scale_y != LV_SCALE_NONE) return false;
    if(d->skew_x != 0 || d->skew_y != 0 || d->tile || d->clip_radius != 0) return false;
    if(d->recolor_opa > LV_OPA_MIN || d->opa < LV_OPA_MAX || t->opa < LV_OPA_MAX) return false;
    if(d->colorkey || d->bitmap_mask_src || d->blend_mode != LV_BLEND_MODE_NORMAL) return false;

    const lv_image_dsc_t * img = d->src;
    const lv_color_format_t layer_cf = t->target_layer->draw_buf->header.cf;
    if(img->header.cf != layer_cf) return false;
    if(layer_cf != LV_COLOR_FORMAT_RGB565 && layer_cf != LV_COLOR_FORMAT_RGB888 &&
       layer_cf != LV_COLOR_FORMAT_XRGB8888) return false;
    if(lv_area_get_width(&t->area) != (int32_t)img->header.w ||
       lv_area_get_height(&t->area) != (int32_t)img->header.h) return false;
    if(img->data == NULL) return false;
    *src_out = img;
    return true;
}

static uint8_t * tile_aligned(void * p);

/* A picture whose pixels are in the flash is read through the same cache
 * whoever reads it, and the turner gets less out of it than the CPU. Measured
 * on the same 1024x600 RGB565 picture drawn whole with nothing kept, on a
 * Waveshare 7B at 180 (flash in DIO at 40 MHz): the PPA handed the flash
 * itself took 224 ms a picture; the CPU read it into RAM and drew it in 171,
 * the 7 MB/s the flash gives. On a Tab5 at 270, 1280x720: 338 ms against 239.
 * So the turner is never handed the flash: such a picture is drawn like
 * anything else, by the CPU into tiles of internal RAM -- on both cores when
 * there are two -- and the turner turns the tiles while the next are drawn.
 * lv_draw_ortho_set_flash_cb() says which pictures those are, for a platform
 * that is not an ESP32. */
static lv_draw_ortho_constant_cb_t g_flash_cb;

static bool in_flash(const lv_image_dsc_t * img)
{
    if(g_flash_cb) return g_flash_cb(img);
#if defined(ESP_PLATFORM)
    return esp_ptr_in_drom(img->data);
#else
    return false;
#endif
}

/* A straight copy of a native rectangle from `src` (a buffer holding the
 * native area `src_area`) into the frame buffer, by the turner at angle 0 when
 * there is one and `by_turner` -- the PPA is a DMA copy then, where the CPU
 * would read and write every line through its cache into PSRAM. Queued like a
 * turn; the CPU does it after everything queued when there is no turner or it
 * declines. */
static void straight_or_cpu(lv_color_format_t cf, int32_t px, uint8_t * dst, int32_t dst_stride,
                            const lv_area_t * dst_buf_area, const uint8_t * src, int32_t src_stride,
                            const lv_area_t * src_area, const lv_area_t * rect, bool by_turner)
{
    if(g_turn && by_turner) {
        const lv_draw_ortho_pic_t s = {(void *)src, (uint32_t)src_stride, (uint32_t)(src_stride / px),
                                       (uint32_t)lv_area_get_height(src_area)
                                      };
        const lv_draw_ortho_pic_t d = {dst, (uint32_t)dst_stride, (uint32_t)(dst_stride / px),
                                       (uint32_t)lv_area_get_height(dst_buf_area)
                                      };
        lv_area_t blk = *rect;
        lv_area_move(&blk, -src_area->x1, -src_area->y1);
        const uint32_t t = g_turn(&s, &blk, &d, rect->x1 - dst_buf_area->x1, rect->y1 - dst_buf_area->y1,
                                  LV_DISPLAY_ROTATION_0, cf);
        if(t) {
            const uint64_t bytes = (uint64_t)lv_area_get_size(rect) * (uint64_t)px;
            g_stats.bytes_straight += bytes;
            g_stats.bytes_straight_handed += bytes;
            g_last = t;
            return;
        }
    }
    fence();
    const uint8_t * sp = src + (intptr_t)(rect->y1 - src_area->y1) * src_stride + (intptr_t)(rect->x1 - src_area->x1) * px;
    uint8_t * dp = dst + (intptr_t)(rect->y1 - dst_buf_area->y1) * dst_stride + (intptr_t)(rect->x1 - dst_buf_area->x1) * px;
    copy_straight(dp, dst_stride, sp, src_stride, lv_area_get_width(rect) * px, lv_area_get_height(rect));
}

/* ------------------------------------------------------------------------ */
/* Pictures turned once                                                     */
/* ------------------------------------------------------------------------ */

/* A picture that does not change, drawn on a turned display, is turned again
 * every time it is drawn: a wallpaper behind a clock that ticks, a background
 * under a page that scrolls. Turned, a pixel is read in one order and written
 * in another, which is the slowest thing the PPA does -- a Tab5 measured it at
 * about 60 ns a pixel, against about 7 for a straight DMA copy on a Waveshare
 * 7B. So the second time a picture is drawn, it is turned once into a buffer
 * of its own, and every draw after that is a straight copy out of it.
 *
 * Only pictures that cannot change under it: by default those in the flash
 * (an ESPHome `image:`, any picture compiled in), never a draw buffer LVGL
 * marks modifiable (a canvas). lv_draw_ortho_set_image_constant_cb() widens
 * that for an application that knows better, and lv_draw_ortho_image_cache_drop()
 * forgets a picture whose pixels it changed in place. */
#ifndef LV_DRAW_ORTHO_IMAGE_CACHE_SLOTS
#define LV_DRAW_ORTHO_IMAGE_CACHE_SLOTS 16
#endif

typedef struct {
    const void * data;            /* the key, with the four below */
    uint32_t w, h, stride;
    lv_color_format_t cf;
    lv_display_rotation_t rot;
    void * alloc;                 /* NULL: seen once, not turned yet */
    uint8_t * nat;                /* the native pixels, `alloc` aligned to a cache line */
    size_t bytes;
    uint32_t used;                /* for least recently used */
    bool no_room;                 /* turned once already and would not fit: never again */
} cached_t;

static cached_t g_cache[LV_DRAW_ORTHO_IMAGE_CACHE_SLOTS];
static size_t g_cache_budget;     /* 0: off */
static size_t g_cache_bytes;
static uint32_t g_cache_clock;
static lv_draw_ortho_constant_cb_t g_constant_cb;

static void cache_forget(cached_t * c)
{
    if(c->alloc) {
        fence();   /* a copy out of it may still be queued */
        lv_free(c->alloc);
        g_cache_bytes -= c->bytes;
    }
    lv_memzero(c, sizeof(*c));
    g_stats.image_cache_bytes = (uint32_t)g_cache_bytes;
}

static bool image_constant(const lv_image_dsc_t * img)
{
    if(img->header.flags & LV_IMAGE_FLAGS_MODIFIABLE) return false;
    if(g_constant_cb) return g_constant_cb(img);
#if defined(ESP_PLATFORM)
    return esp_ptr_in_drom(img->data);
#else
    return false;
#endif
}

static int slot_rank(const cached_t * c)
{
    return c->data == NULL ? 0 : c->alloc == NULL ? 1 : 2;
}

/* The slot for this picture at this rotation; a free or the least recently
 * used one for a picture not seen before, emptied. NULL when it is not to be
 * cached at all. */
static cached_t * cache_slot(const lv_image_dsc_t * img, int32_t sstride, lv_display_rotation_t rot)
{
    if(g_cache_budget == 0 || rot == LV_DISPLAY_ROTATION_0 || !image_constant(img)) return NULL;
    cached_t * victim = NULL;
    for(int i = 0; i < LV_DRAW_ORTHO_IMAGE_CACHE_SLOTS; i++) {
        cached_t * c = &g_cache[i];
        if(c->data == img->data && c->w == img->header.w && c->h == img->header.h &&
           c->stride == (uint32_t)sstride && c->cf == img->header.cf && c->rot == rot) return c;
        /* An empty slot first, then a picture only seen once, then a turned
         * one: in each, the one drawn longest ago. */
        if(victim == NULL || slot_rank(c) < slot_rank(victim) ||
           (slot_rank(c) == slot_rank(victim) && c->used < victim->used)) victim = c;
    }
    cache_forget(victim);
    victim->data = img->data;
    victim->w = img->header.w;
    victim->h = img->header.h;
    victim->stride = (uint32_t)sstride;
    victim->cf = img->header.cf;
    victim->rot = rot;
    return victim;
}

/* Make room for `bytes` by forgetting the least recently used turned
 * pictures, never `keep`. */
static bool cache_room(size_t bytes, const cached_t * keep)
{
    if(bytes > g_cache_budget) return false;
    while(g_cache_bytes + bytes > g_cache_budget) {
        cached_t * old = NULL;
        for(int i = 0; i < LV_DRAW_ORTHO_IMAGE_CACHE_SLOTS; i++) {
            cached_t * c = &g_cache[i];
            if(c == keep || c->alloc == NULL) continue;
            if(old == NULL || c->used < old->used) old = c;
        }
        if(old == NULL) return false;
        cache_forget(old);
    }
    return true;
}

/* Whether the cache will draw this picture, now or once it has turned it: a
 * picture it may keep and has not found too big, on a display that is
 * turned. Asks only; cache_slot is what remembers. */
static bool cache_takes(const lv_draw_task_t * t, const lv_image_dsc_t * img)
{
    ortho_t o;
    lv_display_rotation_t rot;
    if(g_cache_budget == 0 || !image_constant(img)) return false;
    if(!ortho_from_matrix(&t->matrix, &o) || !ortho_rotation(&o, &rot) || rot == LV_DISPLAY_ROTATION_0) return false;
    const uint32_t sstride = img->header.stride ? img->header.stride
                             : img->header.w * (uint32_t)lv_color_format_get_size(img->header.cf);
    for(int i = 0; i < LV_DRAW_ORTHO_IMAGE_CACHE_SLOTS; i++) {
        const cached_t * c = &g_cache[i];
        if(c->data == img->data && c->w == img->header.w && c->h == img->header.h && c->stride == sstride &&
           c->cf == img->header.cf && c->rot == rot) return !c->no_room;
    }
    return true;   /* not seen yet */
}

/* Turn the whole picture into its slot, once. False when it cannot be kept:
 * then it is drawn turned, as before. */
static bool cache_fill(cached_t * c, const ortho_t * o, const lv_image_dsc_t * img, int32_t sstride,
                       const lv_area_t * up, const lv_area_t * whole)
{
    const int32_t px = lv_color_format_get_size(c->cf);
    const size_t row = (size_t)lv_area_get_width(whole) * (size_t)px;
    /* One row more than the picture, in whole cache lines: the PPA writes
     * only into rows it can invalidate whole lines of, and is told the buffer
     * is that row taller so the picture's own last row is one of them. */
    lv_area_t room = *whole;
    room.y2++;
    const size_t bytes = (row * (size_t)lv_area_get_height(&room) + TILE_ALIGN - 1) & ~(size_t)(TILE_ALIGN - 1);
    if(!cache_room(bytes, c)) {
        c->no_room = true;
        return false;
    }
    void * a = lv_malloc(bytes + TILE_ALIGN);
    if(a == NULL) {
        c->no_room = true;
        return false;
    }
    c->alloc = a;
    c->nat = tile_aligned(a);
    c->bytes = bytes;
    g_cache_bytes += bytes;
    if(in_flash(img)) {
        /* Read once, by whoever reads the flash fastest. */
        fence();
        move_turned(o, px, c->nat, (int32_t)row, &room, (uint8_t *)img->data, sstride, up, whole, true);
    }
    else turn_or_cpu(o, c->cf, px, c->nat, (int32_t)row, &room, (uint8_t *)img->data, sstride, up, whole, true);
    g_stats.image_cache_builds++;
    g_stats.image_cache_bytes = (uint32_t)g_cache_bytes;
    return true;
}

/* A picture drawn from its turned copy: true when it was. */
static bool try_cached(lv_draw_task_t * t, const ortho_t * o, const lv_image_dsc_t * img, int32_t sstride,
                       const lv_area_t * parts, int32_t np)
{
    lv_display_rotation_t rot;
    if(!ortho_rotation(o, &rot)) return false;
    cached_t * c = cache_slot(img, sstride, rot);
    if(c == NULL || c->no_room) return false;
    lv_area_t whole;
    ortho_area(o, &t->area, &whole);
    if(c->alloc == NULL) {
        if(c->used == 0) {   /* seen for the first time: drawn turned, remembered */
            c->used = ++g_cache_clock;
            return false;
        }
        if(!cache_fill(c, o, img, sstride, &t->area, &whole)) return false;
    }
    else g_stats.images_cached++;
    c->used = ++g_cache_clock;

    lv_layer_t * target = t->target_layer;
    lv_draw_buf_t * nb = target->draw_buf;
    const int32_t px = lv_color_format_get_size(nb->header.cf);
    for(int32_t i = 0; i < np; i++) {
        straight_or_cpu(nb->header.cf, px, nb->data, nb->header.stride, &target->buf_area, c->nat,
                        lv_area_get_width(&whole) * px, &whole, &parts[i], true);
    }
    /* Drawn alone, so nothing else waits for these before LVGL takes the
     * buffer back -- in partial mode, to send it to the panel. */
    fence();
    return true;
}

static bool try_image(lv_draw_task_t * t, const ortho_t * o, const lv_area_t * todo)
{
    const lv_image_dsc_t * img;
    if(!plain_image(t, &img)) return false;

    lv_layer_t * target = t->target_layer;
    lv_draw_buf_t * nb = target->draw_buf;
    const int32_t px = lv_color_format_get_size(nb->header.cf);
    lv_area_t all, parts[4];
    ortho_area(o, todo, &all);
    const int32_t np = outside_window(target, &all, parts);

    if(img->header.flags & LV_DRAW_ORTHO_IMAGE_FLAG_NATIVE) {
        /* Already in the panel's orientation: its rows ARE native rows. */
        lv_area_t whole;
        ortho_area(o, &t->area, &whole);
        const int32_t sstride = img->header.stride ? (int32_t)img->header.stride
                                : lv_area_get_width(&whole) * px;
        const bool by_turner = !in_flash(img);
        for(int32_t i = 0; i < np; i++) {
            straight_or_cpu(nb->header.cf, px, nb->data, nb->header.stride, &target->buf_area, img->data,
                            sstride, &whole, &parts[i], by_turner);
        }
        fence();   /* the frame is the application's again once the task is finished */
        g_stats.images_native++;
        return true;
    }

    const int32_t sstride = img->header.stride ? (int32_t)img->header.stride
                            : (int32_t)img->header.w * px;
    if(try_cached(t, o, img, sstride, parts, np)) return true;
    if(in_flash(img)) {
        /* Not for the turner (see in_flash): through the tiles. */
        g_stats.images_flash++;
        return false;
    }
    const uint64_t before = g_stats.bytes_turned;
    /* No tile is drawn while the picture is turned and the other core is idle
     * (it is joined at the end of every pair), so the first tile is lent to
     * the turner: a picture in PSRAM can go through it a band at a time. The
     * fence below hands it back. */
#if defined(ESP_PLATFORM)
    const bool lend = g_loan && g_unit && g_tile_internal;
#else
    const bool lend = g_loan && g_unit;   /* a PC has one kind of memory */
#endif
    if(lend) g_loan(tile_aligned(g_unit->tile_alloc[0]), LV_DRAW_ORTHO_TILE_BYTES);
    for(int32_t i = 0; i < np; i++) {
        turn_or_cpu(o, nb->header.cf, px, nb->data, nb->header.stride, &target->buf_area,
                    (uint8_t *)img->data, sstride, &t->area, &parts[i], true);
    }
    fence();   /* the picture is the application's again when the task is finished */
    if(lend) g_loan(NULL, 0);
    g_stats.images_turned++;
    g_stats.bytes_turned_images += g_stats.bytes_turned - before;
    if(img->data == g_last_picture) g_stats.images_again++;
    g_last_picture = img->data;
    if(target->parent) g_stats.images_in_layer++;
    return true;
}

/* ------------------------------------------------------------------------ */
/* The draw unit                                                            */
/* ------------------------------------------------------------------------ */

/* A label's area is the size of its text, and a glyph may still reach past
 * it -- a bearing, an accent, a letter's anti-aliased edge. The software
 * renderer is held back only by the clip area, and drawing tiles over the
 * whole clip area would turn every pixel of a screen for one word. A glyph
 * cannot reach further than a line height from the text it belongs to, so
 * that is the margin. */
static void widen_for_glyphs(const lv_draw_task_t * t, lv_area_t * area)
{
    const lv_font_t * font = NULL;
    if(t->type == LV_DRAW_DSC_TYPE_LABEL) font = ((const lv_draw_label_dsc_t *)t->draw_dsc)->font;
    else if(t->type == LV_DRAW_DSC_TYPE_LETTER) font = ((const lv_draw_letter_dsc_t *)t->draw_dsc)->font;
    else return;
    int32_t m = font ? lv_font_get_line_height(font) : 32;
    lv_area_increase(area, m, m);
}

/* What a task may change, upright, inside the buffer; false if nothing. */
static bool task_todo(const lv_draw_task_t * t, const ortho_t * o, lv_area_t * todo)
{
    lv_area_t buf_up, real = t->_real_area;
    widen_for_glyphs(t, &real);
    if(!lv_area_intersect(todo, &real, &t->clip_area)) return false;
    ortho_unarea(o, &t->target_layer->buf_area, &buf_up);
    return lv_area_intersect(todo, todo, &buf_up);
}

/* A task that goes faster on its own than in a batch.
 *
 * A plain image turned straight into the frame buffer saves its pixels one
 * upright copy -- but leaving the batch splits the run around it, and every
 * tile the second half of the run shares with the first is read back out of
 * the frame buffer and turned again. For an icon that costs far more than it
 * saves: a 97x61 picture in the middle of a UI split one run in two and
 * turned 16% of the screen twice. So an icon stays in the run, and only a
 * picture of LV_DRAW_ORTHO_ALONE_BYTES or more (a camera frame, a wallpaper)
 * goes alone -- and always one stored native, which only the fast path can
 * draw. */
#ifndef LV_DRAW_ORTHO_ALONE_BYTES
#define LV_DRAW_ORTHO_ALONE_BYTES (32 * 1024)
#endif
/* A picture big enough to go alone, with its pixels in the flash. */
static bool big_in_flash(const lv_draw_task_t * t)
{
    const lv_image_dsc_t * img;
    if(!plain_image(t, &img) || (img->header.flags & LV_DRAW_ORTHO_IMAGE_FLAG_NATIVE)) return false;
    return (int64_t)img->header.w * img->header.h * lv_color_format_get_size(img->header.cf) >=
           LV_DRAW_ORTHO_ALONE_BYTES && in_flash(img);
}

static bool fast_alone(const lv_draw_task_t * t)
{
    const lv_image_dsc_t * img;
    if(!g_fast) return false;
    if(t->type == LV_DRAW_DSC_TYPE_BLUR) return true;
    if(!plain_image(t, &img)) return false;
    if(img->header.flags & LV_DRAW_ORTHO_IMAGE_FLAG_NATIVE) return true;
    if((int64_t)img->header.w * img->header.h * lv_color_format_get_size(img->header.cf) <
       LV_DRAW_ORTHO_ALONE_BYTES) return false;
    /* A picture in the flash goes through the tiles (see in_flash), and is
     * drawn there with its neighbours, in one run -- alone only for the
     * cache: to be remembered, turned into it once, and copied out of it. */
    if(in_flash(img) && !cache_takes(t, img)) return false;
    return true;
}

static void execute_alone(ortho_unit_t * u, lv_draw_task_t * t)
{
    ortho_t o;
    if(!ortho_from_matrix(&t->matrix, &o)) {
        g_stats.tasks_refused++;
        return;
    }
    job_t j;
    j.t = t;
    if(!task_todo(t, &o, &j.todo)) return;
    if(g_fast) {
        if(try_fill(t, &o, &j.todo)) return;
        if(try_image(t, &o, &j.todo)) return;
    }
    find_hole(&j);
    draw_jobs(u, &j, 1, &o);
}

static bool same_matrix(const lv_draw_task_t * a, const lv_draw_task_t * b)
{
    return lv_memcmp(&a->matrix, &b->matrix, sizeof(lv_matrix_t)) == 0;
}

static int32_t evaluate(lv_draw_unit_t * draw_unit, lv_draw_task_t * task)
{
    LV_UNUSED(draw_unit);
    ortho_t o;
    if(!ortho_from_matrix(&task->matrix, &o)) return 0;
    /* An identity matrix is the software renderer's own case -- unless a
     * window has to be kept clear, which only this unit knows about. */
    if(ortho_is_identity(&o) && !has_window(task->target_layer)) return 0;
    switch(task->type) {
        case LV_DRAW_DSC_TYPE_FILL:
        case LV_DRAW_DSC_TYPE_BORDER:
        case LV_DRAW_DSC_TYPE_BOX_SHADOW:
        case LV_DRAW_DSC_TYPE_LETTER:
        case LV_DRAW_DSC_TYPE_LABEL:
        case LV_DRAW_DSC_TYPE_IMAGE:
        case LV_DRAW_DSC_TYPE_ARC:
        case LV_DRAW_DSC_TYPE_LINE:
        case LV_DRAW_DSC_TYPE_BLUR:
        case LV_DRAW_DSC_TYPE_TRIANGLE:
        case LV_DRAW_DSC_TYPE_LAYER:
        case LV_DRAW_DSC_TYPE_MASK_RECTANGLE:
#if LV_USE_VECTOR_GRAPHIC && LV_USE_THORVG
        case LV_DRAW_DSC_TYPE_VECTOR:
#endif
            break;
        default:
            return 0;
    }
    /* Nothing else can draw a turned task, so take it whatever was offered. */
    task->preference_score = 0;
    task->preferred_draw_unit_id = DRAW_UNIT_ID_ORTHO;
    return 1;
}

static int32_t dispatch(lv_draw_unit_t * draw_unit, lv_layer_t * layer)
{
    ortho_unit_t * u = (ortho_unit_t *)draw_unit;
    if(u->task_act) return 0;

    lv_draw_task_t * t = lv_draw_get_available_task(layer, NULL, DRAW_UNIT_ID_ORTHO);
    if(t == NULL) return LV_DRAW_UNIT_IDLE;

    /* LVGL offers each task the moment it is created. Taking them one by one
     * would turn every widget's pixels separately; waiting until the layer is
     * complete (or a batch is full) turns them once. */
    if(!layer->all_tasks_added && g_batch) {
        int32_t waiting = 0;
        for(lv_draw_task_t * c = t; c; c = c->next) {
            if(c->state == LV_DRAW_TASK_STATE_WAITING && c->preferred_draw_unit_id == DRAW_UNIT_ID_ORTHO) waiting++;
        }
        if(waiting < LV_DRAW_ORTHO_MAX_BATCH) return LV_DRAW_UNIT_IDLE;
    }
    if(lv_draw_layer_alloc_buf(layer) == NULL) return LV_DRAW_UNIT_IDLE;

    /* A batch is a run of tasks with nothing unfinished before it, so drawing
     * them in list order is the order LVGL itself would have drawn them. */
    lv_draw_task_t * head = layer->draw_task_head;
    while(head && head->state == LV_DRAW_TASK_STATE_FINISHED) head = head->next;

    ortho_t o;
    int32_t n = 0;
    if(g_batch && head == t && !fast_alone(t) && ortho_from_matrix(&t->matrix, &o)) {
        for(lv_draw_task_t * c = t; c && n < LV_DRAW_ORTHO_MAX_BATCH; c = c->next) {
            if(c->state != LV_DRAW_TASK_STATE_WAITING || c->preferred_draw_unit_id != DRAW_UNIT_ID_ORTHO) break;
            if(fast_alone(c) || !same_matrix(c, t)) break;
            u->jobs[n].t = c;
            if(!task_todo(c, &o, &u->jobs[n].todo)) {
                /* Nothing to draw: still part of the run, and finished with it. */
                lv_area_set(&u->jobs[n].todo, 0, 0, -1, -1);
            }
            n++;
        }
    }

    if(n <= 1) {
        t->state = LV_DRAW_TASK_STATE_IN_PROGRESS;
        t->draw_unit = draw_unit;
        u->task_act = t;
        execute_alone(u, t);
        t->state = LV_DRAW_TASK_STATE_FINISHED;
        u->task_act = NULL;
        lv_draw_dispatch_request();
        return 1;
    }

    /* Drop the tasks with nothing to draw from the jobs, keep them finished. */
    int32_t m = 0;
    for(int32_t i = 0; i < n; i++) {
        lv_draw_task_t * c = u->jobs[i].t;
        c->state = LV_DRAW_TASK_STATE_IN_PROGRESS;
        c->draw_unit = draw_unit;
        if(u->jobs[i].todo.x2 < u->jobs[i].todo.x1) continue;
        if(g_fast && big_in_flash(c)) g_stats.images_flash++;   /* drawn in the tiles, see in_flash */
        u->jobs[m] = u->jobs[i];
        find_hole(&u->jobs[m]);
        m++;
    }
    u->task_act = t;
    if(m > 0) draw_jobs(u, u->jobs, m, &o);
    g_stats.batches++;
    /* jobs[] was compacted, so walk the list again for the states. */
    lv_draw_task_t * c = t;
    for(int32_t i = 0; i < n; i++, c = c->next) c->state = LV_DRAW_TASK_STATE_FINISHED;
    u->task_act = NULL;
    lv_draw_dispatch_request();
    return n;
}

/* ------------------------------------------------------------------------ */
/* LVGL's copies between frame buffers                                      */
/* ------------------------------------------------------------------------ */

/* With two or three frame buffers in DIRECT mode, LVGL brings the buffer it
 * is about to draw into up to date at the start of every refresh
 * (refr_sync_areas in lv_refr.c): what the last frame drew, less what this
 * one is about to draw, copied from the buffer last shown -- by the CPU,
 * through its cache, PSRAM into PSRAM. And it does so before it knows whether
 * there is anything to draw at all: a refresh with nothing invalid still
 * copies everything the last frame drew. Measured through esp_lvgl_adapter's
 * PR 818 on the boards, a picture changing every frame and LVGL's own timer
 * refreshing in between: 13.2 ms a frame of such copies on a Waveshare 7B,
 * 20.7 on a Tab5 -- for areas the next frame redrew whole anyway.
 *
 * LVGL makes every one of those copies through lv_draw_buf_copy(), which
 * calls the DESTINATION buffer's own buf_copy_cb, and the frame buffers' are
 * whatever they are given. So the copies are taken over there, without
 * touching LVGL: each is only written down as an area that buffer is behind
 * by. When a buffer is about to be drawn into (LV_EVENT_RENDER_START, after
 * LVGL's sync, before anything is drawn), what it is behind by, less what
 * this frame draws, is copied from the buffer shown last -- by the turner at
 * angle 0 (the PPA) when there is one. A refresh with nothing to draw sends
 * no LV_EVENT_RENDER_START, so what it was behind by waits for the next
 * frame, and is subtracted from it.
 *
 * Why that is right: the buffer shown last always holds the whole of the
 * latest frame, because it was brought up to date this way before it was
 * drawn into, and what a refresh draws it draws whole -- LVGL's own sync
 * rests on that same assumption when it subtracts the areas about to be
 * drawn. With three buffers it is what makes the third one's copies, which
 * LVGL makes a frame early, wait for the frame that draws into it. */
#ifndef LV_DRAW_ORTHO_SYNC_AREAS
#define LV_DRAW_ORTHO_SYNC_AREAS 32
#endif
/* Pieces of a subtraction; past it the areas are copied unsubtracted, which
 * copies more than needed and is never wrong. */
#define SYNC_PIECES 64

typedef struct {
    lv_display_t * disp;
    lv_draw_buf_t * buf[3];
    const lv_draw_buf_handlers_t * orig[3];
    lv_draw_buf_handlers_t handlers[3];   /* the buffer's own, buf_copy_cb ours */
    lv_area_t behind[3][LV_DRAW_ORTHO_SYNC_AREAS];
    uint8_t n[3];
    bool whole[3];                        /* behind by more than the list holds: everywhere */
} sync_t;

static sync_t g_sync;
/* The display our two event callbacks are on: they stay there, inert, once
 * the copies are given back from inside an event, where LVGL's list of them
 * may not be changed. */
static lv_display_t * g_sync_hooked;

static int sync_index(const lv_draw_buf_t * b)
{
    if(b == NULL) return -1;
    for(int i = 0; i < 3; i++) {
        if(g_sync.buf[i] == b) return i;
    }
    return -1;
}

/* Native area of a whole buffer. */
static void sync_whole_area(const lv_draw_buf_t * b, lv_area_t * a)
{
    lv_area_set(a, 0, 0, (int32_t)b->header.w - 1, (int32_t)b->header.h - 1);
}

/* Buffer `i` is behind by `area`. */
static void sync_behind(int i, const lv_area_t * area)
{
    if(g_sync.whole[i]) return;
    uint8_t m = 0;
    for(uint8_t k = 0; k < g_sync.n[i]; k++) {
        if(lv_area_is_in(area, &g_sync.behind[i][k], 0)) return;   /* already there */
        if(lv_area_is_in(&g_sync.behind[i][k], area, 0)) continue; /* inside the new one: dropped */
        g_sync.behind[i][m++] = g_sync.behind[i][k];
    }
    g_sync.n[i] = m;
    if(m == LV_DRAW_ORTHO_SYNC_AREAS) {
        g_sync.whole[i] = true;
        g_sync.n[i] = 0;
        return;
    }
    g_sync.behind[i][g_sync.n[i]++] = *area;
}

/* Whether `area` is one of the areas LVGL is syncing: refr_sync_areas hands
 * each element of its own list to lv_draw_buf_copy() as both areas. Another
 * copy between the same buffers -- esp_lvgl_adapter's own, bringing a third
 * panel buffer up to date at LV_EVENT_RENDER_START -- passes areas of its own,
 * and must be made at once. */
static bool sync_area_of_lvgl(lv_display_t * disp, const lv_area_t * area)
{
    for(const lv_area_t * a = lv_ll_get_head(&disp->sync_areas); a; a = lv_ll_get_next(&disp->sync_areas, a))
        if(a == area) return true;
    return false;
}

static bool sync_alike(const lv_draw_buf_t * a, const lv_draw_buf_t * b)
{
    return a->header.w == b->header.w && a->header.h == b->header.h && a->header.cf == b->header.cf &&
           a->header.stride == b->header.stride && a->data && b->data;
}

static void sync_copy_cb(lv_draw_buf_t * dest, const lv_area_t * dest_area, const lv_draw_buf_t * src,
                         const lv_area_t * src_area)
{
    const int d = sync_index(dest), s = sync_index(src);
    LV_ASSERT(d >= 0);   /* only our buffers have this callback */
    lv_display_t * disp = g_sync.disp;
    const bool same_place = s >= 0 && s != d && dest_area && dest_area == src_area && sync_alike(dest, src);
    /* LVGL's own sync, on our display: written down, not done. */
    if(same_place && disp && lv_refr_get_disp_refreshing() == disp && sync_area_of_lvgl(disp, dest_area)) {
        lv_area_t a, whole;
        sync_whole_area(dest, &whole);
        if(lv_area_intersect(&a, dest_area, &whole)) {
            sync_behind(d, &a);
            g_stats.sync_noted++;
            g_stats.sync_bytes_noted += (uint64_t)lv_area_get_size(&a) * lv_color_format_get_size(dest->header.cf);
        }
        return;
    }
    /* Any other copy from one frame buffer to another, at the same place: made
     * now, by the turner when there is one -- the caller expects it done. */
    if(same_place) {
        lv_area_t r, whole;
        sync_whole_area(dest, &whole);
        if(lv_area_intersect(&r, dest_area, &whole)) {
            const lv_color_format_t cf = dest->header.cf;
            const int32_t px = lv_color_format_get_size(cf);
            const uint64_t handed = g_stats.bytes_straight_handed;
            straight_or_cpu(cf, px, dest->data, (int32_t)dest->header.stride, &whole, src->data,
                            (int32_t)src->header.stride, &whole, &r, true);
            fence();
            g_stats.sync_others++;
            if(g_stats.bytes_straight_handed != handed)
                g_stats.sync_others_handed += (uint64_t)lv_area_get_size(&r) * (uint64_t)px;
        }
        return;
    }
    g_sync.orig[d]->buf_copy_cb(dest, dest_area, src, src_area);
}

/* `in[0..n)` less `cut`, into `out` (at most `max`). False when it would
 * not fit. */
static bool sync_subtract(const lv_area_t * in, int32_t n, const lv_area_t * cut, lv_area_t * out, int32_t * n_out,
                          int32_t max)
{
    int32_t m = 0;
    for(int32_t i = 0; i < n; i++) {
        lv_area_t res[4];
        const int8_t r = lv_area_diff(res, &in[i], cut);
        if(r < 0) {   /* apart: kept whole */
            if(m >= max) return false;
            out[m++] = in[i];
            continue;
        }
        for(int8_t k = 0; k < r; k++) {
            if(m >= max) return false;
            out[m++] = res[k];
        }
    }
    *n_out = m;
    return true;
}

static bool sync_still_ours(void)
{
    lv_display_t * disp = g_sync.disp;
    if(disp->render_mode != LV_DISPLAY_RENDER_MODE_DIRECT) return false;
    if(disp->buf_1 != g_sync.buf[0] || disp->buf_2 != g_sync.buf[1] || disp->buf_3 != g_sync.buf[2]) return false;
    for(int i = 0; i < 3; i++) {
        if(g_sync.buf[i] && g_sync.buf[i]->handlers != &g_sync.handlers[i]) return false;
    }
    return true;
}

static void sync_render_start(lv_event_t * e);
static void sync_delete(lv_event_t * e);

/* The buffers given their own handlers back; nothing written down kept. */
static void sync_release(void)
{
    for(int i = 0; i < 3; i++) {
        lv_draw_buf_t * b = g_sync.buf[i];
        if(b && b->handlers == &g_sync.handlers[i]) b->handlers = g_sync.orig[i];
    }
    lv_memzero(&g_sync, sizeof(g_sync));
}

static void sync_unhook(void)
{
    if(g_sync_hooked == NULL) return;
    lv_display_remove_event_cb_with_user_data(g_sync_hooked, sync_render_start, &g_sync);
    lv_display_remove_event_cb_with_user_data(g_sync_hooked, sync_delete, &g_sync);
    g_sync_hooked = NULL;
}

/* The buffer shown last, as LVGL works it out in refr_sync_areas. */
static lv_draw_buf_t * sync_on_screen(const lv_display_t * disp)
{
    const lv_draw_buf_t * act = disp->buf_act;
    return act == disp->buf_1 ? (disp->buf_3 ? disp->buf_3 : disp->buf_2)
           : act == disp->buf_2 ? disp->buf_1 : disp->buf_2;
}

/* What buffer `i` is behind by, less the areas `drawn` (native), copied from
 * `on`; nothing written down for it after. */
static void sync_repair(int i, lv_draw_buf_t * on, const lv_area_t * drawn, int32_t ndrawn)
{
    lv_draw_buf_t * b = g_sync.buf[i];
    lv_area_t pieces[2][SYNC_PIECES];
    int32_t n = 0, cur = 0;
    if(g_sync.whole[i]) sync_whole_area(b, &pieces[0][n++]);
    else {
        for(uint8_t k = 0; k < g_sync.n[i]; k++) pieces[0][n++] = g_sync.behind[i][k];
    }
    g_sync.n[i] = 0;
    g_sync.whole[i] = false;
    if(n == 0 || on == NULL || on == b || on->data == NULL || b->data == NULL ||
       on->header.w != b->header.w || on->header.h != b->header.h || on->header.cf != b->header.cf) return;
    for(int32_t k = 0; k < ndrawn && n > 0; k++) {
        int32_t m;
        if(!sync_subtract(pieces[cur], n, &drawn[k], pieces[!cur], &m, SYNC_PIECES)) continue;   /* copied as it is */
        cur = !cur;
        n = m;
    }
    const lv_color_format_t cf = b->header.cf;
    const int32_t px = lv_color_format_get_size(cf);
    lv_area_t whole;
    sync_whole_area(b, &whole);
    for(int32_t k = 0; k < n; k++) {
        lv_area_t r;
        if(!lv_area_intersect(&r, &pieces[cur][k], &whole)) continue;
        const uint64_t bytes = (uint64_t)lv_area_get_size(&r) * (uint64_t)px;
        const uint64_t handed = g_stats.bytes_straight_handed;
        straight_or_cpu(cf, px, b->data, (int32_t)b->header.stride, &whole, on->data, (int32_t)on->header.stride,
                        &whole, &r, true);
        g_stats.sync_copies++;
        g_stats.sync_bytes_copied += bytes;
        if(g_stats.bytes_straight_handed != handed) g_stats.sync_bytes_handed += bytes;
    }
}

/* LVGL waits for the flush in progress before its own copies: the buffer
 * about to be written may be on the panel until it is done. */
static void sync_wait_flush(lv_display_t * disp)
{
    if(!disp->flushing) return;
    if(disp->flush_wait_cb) {
        disp->flush_wait_cb(disp);
        disp->flushing = 0;
    }
    else {
        while(disp->flushing) {}
    }
}

/* Everything written down, copied now from the buffer shown last, and the
 * copies given back to LVGL -- whose own list of them was emptied as it
 * handed each one over. */
static void sync_settle_and_release(void)
{
    lv_display_t * disp = g_sync.disp;
    if(disp && disp->buf_1 == g_sync.buf[0] && disp->buf_2 == g_sync.buf[1] && disp->buf_3 == g_sync.buf[2]) {
        lv_draw_buf_t * on = sync_on_screen(disp);
        sync_wait_flush(disp);
        for(int i = 0; i < 3; i++) {
            if(g_sync.buf[i] && g_sync.buf[i] != on) sync_repair(i, on, NULL, 0);
        }
        fence();
    }
    /* Otherwise the display has other buffers now: drawn again whole by
     * whoever set them, as LVGL would need too. */
    sync_release();
}

static bool sync_install(lv_display_t * disp);

static void sync_render_start(lv_event_t * e)
{
    lv_display_t * disp = lv_event_get_current_target(e);
    if(disp != g_sync_hooked) return;
    if(g_sync.disp != disp || !sync_still_ours()) {
        /* The buffers were set again behind us (a dummy draw, a new panel):
         * what was written down is made, and they are taken again. LVGL
         * made this refresh's own copies itself. */
        if(g_sync.disp == disp) sync_settle_and_release();
        if(!sync_install(disp)) return;
    }
    const int a = sync_index(disp->buf_act);
    if(a < 0 || (g_sync.n[a] == 0 && !g_sync.whole[a])) return;

    /* What this frame draws, in the panel's orientation. */
    lv_area_t drawn[LV_INV_BUF_SIZE];
    int32_t nd = 0;
    const lv_area_t screen = {0, 0, lv_display_get_horizontal_resolution(disp) - 1,
                              lv_display_get_vertical_resolution(disp) - 1
                             };
    for(uint32_t i = 0; i < disp->inv_p; i++) {
        if(disp->inv_area_joined[i]) continue;
        if(!lv_area_intersect(&drawn[nd], &disp->inv_areas[i], &screen)) continue;
#if LV_DRAW_TRANSFORM_USE_MATRIX
        if(lv_display_get_matrix_rotation(disp)) lv_display_rotate_area(disp, &drawn[nd]);
#endif
        nd++;
    }
    sync_wait_flush(disp);
    sync_repair(a, sync_on_screen(disp), drawn, nd);
    /* Done before anything is drawn: LVGL's own units, and the PPA's fills,
     * are not in the turner's queue. */
    fence();
}

static void sync_delete(lv_event_t * e)
{
    lv_display_t * disp = lv_event_get_current_target(e);
    if(disp == g_sync.disp) sync_release();
    if(disp == g_sync_hooked) g_sync_hooked = NULL;   /* gone with it */
}

/* The buffers' copy handlers replaced; false and nothing done when they
 * cannot be. Changes no event list, so it may run inside an event. */
static bool sync_install(lv_display_t * disp)
{
    if(disp->render_mode != LV_DISPLAY_RENDER_MODE_DIRECT || disp->buf_1 == NULL || disp->buf_2 == NULL) return false;
#if LVGL_VERSION_MAJOR > 9 || (LVGL_VERSION_MAJOR == 9 && LVGL_VERSION_MINOR >= 6)
    if(disp->sync_cb) return false;   /* the application's own sync: its business */
#endif
    lv_draw_buf_t * bufs[3] = {disp->buf_1, disp->buf_2, disp->buf_3};
    for(int i = 0; i < 3; i++) {
        if(bufs[i] == NULL) continue;
        if(bufs[i]->handlers == NULL || bufs[i]->handlers->buf_copy_cb == NULL) return false;
    }
    for(int i = 0; i < 3; i++) {
        lv_draw_buf_t * b = bufs[i];
        g_sync.buf[i] = b;
        if(b == NULL) continue;
        g_sync.orig[i] = b->handlers;
        g_sync.handlers[i] = *b->handlers;
        g_sync.handlers[i].buf_copy_cb = sync_copy_cb;
        b->handlers = &g_sync.handlers[i];
    }
    /* Nothing is written down yet: LVGL has kept the buffers in step until
     * now, and what they are behind by is its own list, which it hands over
     * at the next refresh. */
    g_sync.disp = disp;
    return true;
}

bool lv_draw_ortho_take_sync(lv_display_t * disp)
{
    if(g_sync.disp) sync_settle_and_release();
    if(disp == NULL || !sync_install(disp)) return false;
    if(g_sync_hooked != disp) {
        sync_unhook();
        lv_display_add_event_cb(disp, sync_render_start, LV_EVENT_RENDER_START, &g_sync);
        lv_display_add_event_cb(disp, sync_delete, LV_EVENT_DELETE, &g_sync);
        g_sync_hooked = disp;
    }
    return true;
}

void lv_draw_ortho_release_sync(lv_display_t * disp)
{
    if(disp != NULL && disp != g_sync_hooked && disp != g_sync.disp) return;
    if(g_sync.disp) sync_settle_and_release();
    sync_unhook();
}

/* The tile is where every widget is drawn and every pixel of it is turned
 * out of, so it has to be the fastest memory there is. On an ESP32-P4 that is
 * internal RAM: LVGL's own allocator there (ESPHome's lv_malloc_core) asks
 * PSRAM first, which would send all of the drawing through the same bus the
 * frame buffer and the panel's scan-out already share -- the very traffic
 * this unit exists to avoid. PSRAM only if internal RAM cannot hold it. */
#if defined(ESP_PLATFORM)
#include "esp_heap_caps.h"
static void * tile_alloc(size_t size, bool * internal)
{
    void * p = heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    *internal = p != NULL;
    return p ? p : heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}
static void tile_free(void * p)
{
    heap_caps_free(p);
}
#else
/* LV_DRAW_ORTHO_TEST_TILES: at most that many tiles at once, so a test can
 * see what a board with little internal RAM does. */
#ifdef LV_DRAW_ORTHO_TEST_TILES
static int g_test_tiles;
#endif
static void * tile_alloc(size_t size, bool * internal)
{
    *internal = false;
#ifdef LV_DRAW_ORTHO_TEST_TILES
    if(g_test_tiles >= LV_DRAW_ORTHO_TEST_TILES) return NULL;
    g_test_tiles++;
#endif
    return lv_malloc(size);
}
static void tile_free(void * p)
{
#ifdef LV_DRAW_ORTHO_TEST_TILES
    g_test_tiles--;
#endif
    lv_free(p);
}
#endif

static uint8_t * tile_aligned(void * p)
{
    return (uint8_t *)(((uintptr_t)p + TILE_ALIGN - 1) & ~(uintptr_t)(TILE_ALIGN - 1));
}

/* The tiles the pipeline can use: one alone, two with a turner or a second
 * core, four with both. */
static int tiles_wanted(void)
{
    return g_turn && g_helper.on ? 4 : g_turn || g_helper.on ? 2 : 1;
}

/* The blocks there are, shared out into `want` tiles of one size: a block
 * each when there are enough, else each block cut into equal parts. Only
 * after a fence, since a turner may still be reading a tile. */
static void share_tiles(ortho_unit_t * u, int want)
{
    int blocks = 0;
    while(blocks < 4 && u->tile_alloc[blocks]) blocks++;
    const int per = (want + blocks - 1) / blocks;
    const size_t bytes = per == 1 ? LV_DRAW_ORTHO_TILE_BYTES
                         : (LV_DRAW_ORTHO_TILE_BYTES / (size_t)per) & ~(size_t)(TILE_ALIGN - 1);
    for(int j = 0; j < 4; j++) {
        u->tile[j] = j < want ? tile_aligned(u->tile_alloc[j / per]) + (size_t)(j % per) * bytes : NULL;
        u->ticket[j] = 0;
    }
    u->tile_bytes = bytes;
}

/* The blocks past the first: internal RAM or nothing, since a tile in PSRAM
 * would put all the drawing on the bus the turn is meant to keep it off. When
 * internal RAM has no room for a block per tile, the tiles are cut out of the
 * blocks there are: smaller tiles, for no RAM more. Without that, a board
 * whose internal RAM holds a single block -- an ESP32-P4 whose L2 cache takes
 * 256 KB leaves one 256 KB region, and two blocks do not fit in it beside
 * anything else -- would draw one tile and then wait for the turner to finish
 * it, with nothing overlapped and no second core. */
static void more_tiles(ortho_unit_t * u)
{
    const int want = tiles_wanted();
    int blocks = 0;
    while(blocks < 4 && u->tile_alloc[blocks]) blocks++;
    while(blocks < want) {
        bool internal = false;
        void * p = tile_alloc(LV_DRAW_ORTHO_TILE_BYTES + TILE_ALIGN, &internal);
#if !defined(ESP_PLATFORM)
        internal = true;   /* a PC has one kind of memory */
#endif
        if(p && !internal) {
            tile_free(p);
            p = NULL;
        }
        if(p == NULL) break;
        u->tile_alloc[blocks++] = p;
    }
    fence();
    share_tiles(u, want);
    if(blocks < want) {
        LV_LOG_WARN("ortho: internal RAM for %d block(s) of %u KiB; %d tiles of %u KiB cut from them",
                    blocks, (unsigned)(LV_DRAW_ORTHO_TILE_BYTES / 1024), want, (unsigned)(u->tile_bytes / 1024));
    }
}

static int32_t delete_cb(lv_draw_unit_t * draw_unit)
{
    ortho_unit_t * u = (ortho_unit_t *)draw_unit;
    fence();
    if(g_helper.on) {
        helper_stop_thread();
        g_helper.on = false;
    }
    if(u->text_mutex_ready) {
        lv_mutex_delete(&u->text_mutex);
        u->text_mutex_ready = false;
    }
    for(int i = 0; i < 4; i++) {
        if(u->tile_alloc[i]) tile_free(u->tile_alloc[i]);
        u->tile_alloc[i] = NULL;
        u->tile[i] = NULL;
    }
    lv_free(u->jobs);
    u->jobs = NULL;
    g_unit = NULL;
    return 0;
}

void lv_draw_ortho_init(void)
{
    if(g_unit) return;
    ortho_unit_t * u = lv_draw_create_unit(sizeof(ortho_unit_t));
    u->text_mutex_ready = lv_mutex_init(&u->text_mutex) == LV_RESULT_OK;
    u->base.name = "ORTHO";
    u->base.evaluate_cb = evaluate;
    u->base.dispatch_cb = dispatch;
    u->base.delete_cb = delete_cb;
    bool internal = false;
    u->tile_alloc[0] = tile_alloc(LV_DRAW_ORTHO_TILE_BYTES + TILE_ALIGN, &internal);
    LV_ASSERT_MALLOC(u->tile_alloc[0]);
    g_tile_internal = internal;
    u->tile[0] = tile_aligned(u->tile_alloc[0]);
    u->tile_bytes = LV_DRAW_ORTHO_TILE_BYTES;
    u->jobs = lv_malloc(sizeof(job_t) * LV_DRAW_ORTHO_MAX_BATCH);
    LV_ASSERT_MALLOC(u->jobs);
    g_unit = u;
    more_tiles(u);
}

void lv_draw_ortho_set_filler(lv_draw_ortho_fill_cb_t fill)
{
    g_fill = fill;
}

void lv_draw_ortho_set_loan(lv_draw_ortho_loan_cb_t loan)
{
    fence();
    g_loan = loan;
}

void lv_draw_ortho_set_turner(lv_draw_ortho_turn_cb_t turn, lv_draw_ortho_wait_cb_t wait)
{
    fence();
    g_turner_failed = false;
    g_turn = wait ? turn : NULL;
    g_wait = g_turn ? wait : NULL;
    if(g_unit) {
        for(int i = 0; i < 4; i++) g_unit->ticket[i] = 0;
        g_unit->next = 0;
        g_unit->pair = 0;
        more_tiles(g_unit);
    }
}

bool lv_draw_ortho_set_cores(uint32_t cores)
{
    if(g_unit == NULL) return false;
    if(cores < 2) {
        if(g_helper.on) helper_stop_thread();
        g_helper.on = false;
        more_tiles(g_unit);   /* the tiles one core uses, whole again if they were cut */
        return true;
    }
    if(g_helper.on) return true;
    if(ORTHO_HELPER == 0) {
        LV_LOG_WARN("ortho: no second core without an OS (LV_USE_OS), which LVGL's own locks need");
        return false;
    }
    if(!helper_start_thread()) {
        LV_LOG_WARN("ortho: the second core's task could not be started");
        return false;
    }
    g_helper.on = true;
    more_tiles(g_unit);   /* tiles_wanted() now asks for the second core's */
    g_unit->pair = 0;
    return true;
}

void lv_draw_ortho_set_copy_cb(lv_draw_ortho_copy_cb_t cb)
{
    g_copy_cb = cb;
}

void lv_draw_ortho_set_fast_paths(bool enable)
{
    g_fast = enable;
}

void lv_draw_ortho_set_batching(bool enable)
{
    g_batch = enable;
}

const lv_draw_ortho_stats_t * lv_draw_ortho_get_stats(void)
{
    g_stats.tile_internal = g_tile_internal;
    g_stats.tiles_in_turn = g_unit && g_unit->tile[1] && g_turn ? (g_helper.on && g_unit->tile[3] ? 4 : 2) : 1;
    g_stats.cores = g_helper.on ? 2 : 1;
#if ORTHO_HELPER == 2
    g_stats.helper_cores = (g_helper.on_core[0].task ? 1 : 0) | (g_helper.on_core[1].task ? 2 : 0);
#endif
    g_stats.turner_failed = g_turner_failed;
    return &g_stats;
}

void lv_draw_ortho_set_window(lv_display_t * disp, const lv_area_t * native_area)
{
    if(disp == NULL || native_area == NULL) {
        g_win_disp = NULL;
        return;
    }
    if(lv_display_get_render_mode(disp) != LV_DISPLAY_RENDER_MODE_DIRECT) {
        LV_LOG_WARN("ortho: a video window needs LV_DISPLAY_RENDER_MODE_DIRECT; ignored");
        g_win_disp = NULL;
        return;
    }
    g_win = *native_area;
    g_win_disp = disp;
}

void lv_draw_ortho_native_area(lv_display_t * disp, const lv_area_t * area, lv_area_t * native_area)
{
    *native_area = *area;
    lv_display_rotate_area(disp, native_area);
}

void lv_draw_ortho_image_cache_drop(const void * data)
{
    for(int i = 0; i < LV_DRAW_ORTHO_IMAGE_CACHE_SLOTS; i++) {
        if(g_cache[i].data == NULL) continue;
        if(data == NULL || g_cache[i].data == data) cache_forget(&g_cache[i]);
    }
}

void lv_draw_ortho_set_image_cache(size_t bytes)
{
    g_cache_budget = bytes;
    if(bytes == 0) {
        lv_draw_ortho_image_cache_drop(NULL);
        return;
    }
    /* Shrunk: what no longer fits goes, least recently used first. Grown: a
     * picture that did not fit before may now. */
    cache_room(0, NULL);
    for(int i = 0; i < LV_DRAW_ORTHO_IMAGE_CACHE_SLOTS; i++) g_cache[i].no_room = false;
}

void lv_draw_ortho_set_image_constant_cb(lv_draw_ortho_constant_cb_t cb)
{
    g_constant_cb = cb;
    lv_draw_ortho_image_cache_drop(NULL);
}

void lv_draw_ortho_set_flash_cb(lv_draw_ortho_constant_cb_t cb)
{
    g_flash_cb = cb;
}

void lv_draw_ortho_reset_stats(void)
{
    lv_memzero(&g_stats, sizeof(g_stats));
    g_stats.image_cache_bytes = (uint32_t)g_cache_bytes;
}

#endif /* LV_USE_DRAW_ORTHO */
