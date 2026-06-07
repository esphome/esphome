#pragma once

#ifdef USE_ESP32

// Only compile when Lottie widget is actually used (LV_USE_LOTTIE=1 in lv_conf.h).
// This header is pulled in via esphome.h for all builds, so we must guard it.
#include <lvgl.h>
#if LV_USE_LOTTIE

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_cache.h"
#include "esp_memory_utils.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <cstring>

// Access lv_lottie_t internals for safe re-initialisation on screen re-load.
// Needed to null out the dangling anim pointer and to clear the ThorVG canvas
// before re-pushing the paint.
#include <src/widgets/lottie/lv_lottie_private.h>

namespace esphome {
namespace lvgl {

static const char *const LOTTIE_TAG = "lottie";
static constexpr size_t LOTTIE_TASK_STACK_SIZE = 64 * 1024;
static constexpr size_t LOTTIE_CACHE_ALIGN = 128;
static constexpr size_t LOTTIE_INTERNAL_BUFFER_MAX_BYTES = 256 * 1024;
static constexpr size_t LOTTIE_INTERNAL_BUFFER_HEADROOM_BYTES = 48 * 1024;

// Persistent context for each Lottie widget - tracks all PSRAM allocations,
// the render task, and cached animation parameters for safe re-load.
struct LottieContext {
    // --- Config (set once, never freed) ---
    lv_obj_t *obj;
    const void *data;           // PROGMEM (embedded) or nullptr
    size_t data_size;
    const char *file_path;      // string literal or nullptr
    bool loop;
    bool auto_start;
    uint32_t width;
    uint32_t height;

    // --- Animation params (captured on first load, reused on re-loads) ---
    lv_anim_exec_xcb_t exec_cb;
    void *anim_var;
    int32_t start_frame;
    int32_t end_frame;
    uint32_t duration_ms;
    bool data_loaded;           // true after first successful parse

    // --- Runtime state (freed on screen unload) ---
    uint8_t *pixel_buffer;      // canvas buffer - width*height*4
    bool pixel_buffer_internal;
    StackType_t *task_stack;    // PSRAM - 64 KB
    StaticTask_t *task_tcb;     // internal RAM
    TaskHandle_t task_handle;
    volatile bool stop_requested;
    volatile bool restart_requested;  // Flag to restart animation from frame 0
    TickType_t start_tick;            // Animation start time (can be reset)
    bool user_wants_hidden;     // Save user's 'hidden' config from YAML
    bool runtime_hidden;        // Actual visibility at time of unload (captures script changes)
};

inline size_t lottie_align_up(size_t value, size_t align) {
    return (value + align - 1) & ~(align - 1);
}

inline void lottie_sync_canvas_buffer(LottieContext *ctx) {
    if (ctx == nullptr || ctx->obj == nullptr) return;

    lv_draw_buf_t *draw_buf = lv_canvas_get_draw_buf(ctx->obj);
    if (draw_buf == nullptr || draw_buf->data == nullptr) return;

    uint8_t *data = static_cast<uint8_t *>(draw_buf->data);
    if (!esp_ptr_external_ram(data)) return;

    size_t len = draw_buf->data_size;
    if (len == 0 && draw_buf->header.stride > 0 && draw_buf->header.h > 0) {
        len = static_cast<size_t>(draw_buf->header.stride) * draw_buf->header.h;
    }
    if (len == 0) return;

    uintptr_t start = reinterpret_cast<uintptr_t>(data) & ~(LOTTIE_CACHE_ALIGN - 1);
    uintptr_t end = lottie_align_up(reinterpret_cast<uintptr_t>(data) + len, LOTTIE_CACHE_ALIGN);
    if (end <= start) return;

    esp_cache_msync(reinterpret_cast<void *>(start), end - start,
                    ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_TYPE_DATA);
}

inline uint8_t *lottie_alloc_pixel_buffer(size_t alloc_bytes, bool *internal) {
    if (internal != nullptr) {
        *internal = false;
    }

    if (alloc_bytes <= LOTTIE_INTERNAL_BUFFER_MAX_BYTES) {
        size_t largest_internal = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (largest_internal >= alloc_bytes + LOTTIE_INTERNAL_BUFFER_HEADROOM_BYTES) {
            uint8_t *buf = (uint8_t *)heap_caps_aligned_alloc(
                LOTTIE_CACHE_ALIGN, alloc_bytes,
                MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
            if (buf != nullptr) {
                if (internal != nullptr) {
                    *internal = true;
                }
                return buf;
            }
        }
    }

    return (uint8_t *)heap_caps_aligned_alloc(
        LOTTIE_CACHE_ALIGN, alloc_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

// --------------------------------------------------------------------------
// Render task - runs on 64 KB PSRAM stack.
//
// First load:  set buffer -> parse data -> capture anim params -> render loop
// Re-load:     clear canvas -> set buffer (no re-parse) -> render loop
//
// lv_lottie_set_buffer() MUST be called from this task (not from an LVGL
// event callback) because it internally triggers a ThorVG render that
// needs the large stack.
// --------------------------------------------------------------------------
inline void lottie_load_task(void *param) {
    LottieContext *ctx = (LottieContext *)param;

    // Wait for LVGL to settle after the page/overlay event.  The widget is
    // already hidden while loading, so a long first-load delay only makes the
    // animation appear as a visible pop after the overlay is on screen.
    vTaskDelay(pdMS_TO_TICKS(ctx->data_loaded ? 100 : 50));

    lv_lock();

    if (!ctx->data_loaded) {
        // ===== FIRST LOAD =====
        ESP_LOGD(LOTTIE_TAG, "First load: parsing lottie data...");

        // Set pixel buffer - this calls anim_exec_cb internally but since
        // no data is loaded yet ThorVG has nothing to render (safe).
        lv_lottie_set_buffer(ctx->obj, ctx->width, ctx->height, ctx->pixel_buffer);

        // Parse lottie data (heavy ThorVG work - needs 64 KB stack)
        if (ctx->data != nullptr) {
            lv_lottie_set_src_data(ctx->obj, ctx->data, ctx->data_size);
            ESP_LOGD(LOTTIE_TAG, "Data loaded from embedded source (%d bytes)", (int)ctx->data_size);
        } else if (ctx->file_path != nullptr) {
            lv_lottie_set_src_file(ctx->obj, ctx->file_path);
            ESP_LOGD(LOTTIE_TAG, "Data loaded from file: %s", ctx->file_path);
        }

        // Capture animation parameters before deleting the LVGL animation
        lv_anim_t *anim = lv_lottie_get_anim(ctx->obj);
        if (anim != nullptr) {
            ctx->exec_cb     = anim->exec_cb;
            ctx->anim_var    = anim->var;
            ctx->start_frame = anim->start_value;
            ctx->end_frame   = anim->end_value;
            ctx->duration_ms = (uint32_t)lv_anim_get_time(anim);

            ESP_LOGD(LOTTIE_TAG, "Anim: frames %d..%d, duration %u ms",
                     (int)ctx->start_frame, (int)ctx->end_frame, (unsigned)ctx->duration_ms);

            // Delete the LVGL animation - we drive rendering ourselves
            // from this PSRAM task instead of the main task (small stack).
            lv_anim_delete(ctx->anim_var, ctx->exec_cb);

            // CRITICAL: null out the dangling pointer in lv_lottie_t.
            // Without this, anim_exec_cb (called by lv_lottie_set_buffer
            // on re-load) would dereference freed memory.
            lv_lottie_t *lottie = (lv_lottie_t *)ctx->obj;
            lottie->anim = NULL;

            ctx->data_loaded = true;
            ESP_LOGD(LOTTIE_TAG, "LVGL anim removed - rendering from PSRAM task");
        } else {
            ESP_LOGE(LOTTIE_TAG, "Animation INVALID - parsing may have failed!");
        }
    } else {
        // ===== RE-LOAD (screen came back) =====
        // Data is already parsed in the lv_lottie widget.  We just need
        // to point ThorVG + LVGL canvas at the new pixel buffer.
        //
        // tvg_canvas_clear removes the paint from the canvas without
        // deleting it (false), so lv_lottie_set_buffer can push it again
        // without a double-push.
        ESP_LOGD(LOTTIE_TAG, "Re-load: updating buffer (no re-parse)");

        lv_lottie_t *lottie = (lv_lottie_t *)ctx->obj;
        tvg_canvas_clear(lottie->tvg_canvas, false);

        // Safe to call: widget is hidden (lv_obj_is_visible -> false)
        // and lottie->anim is NULL (no dangling pointer access).
        lv_lottie_set_buffer(ctx->obj, ctx->width, ctx->height, ctx->pixel_buffer);
    }

    // Render the first frame before the object can become visible.  The pixel
    // buffer exists at this point, but without this draw LVGL can briefly flush
    // the blank buffer created in lottie_launch().
    if (ctx->data_loaded && ctx->exec_cb != nullptr && ctx->anim_var != nullptr &&
        ctx->end_frame > ctx->start_frame) {
        ctx->exec_cb(ctx->anim_var, ctx->start_frame);
        lottie_sync_canvas_buffer(ctx);
        lv_obj_invalidate(ctx->obj);
    }

    const bool reveal_after_prepare =
        !ctx->runtime_hidden && ctx->data_loaded && ctx->exec_cb != nullptr &&
        ctx->anim_var != nullptr && ctx->end_frame > ctx->start_frame;

    lv_unlock();

    // Keep the object hidden for one LVGL tick after the heavy ThorVG parse and
    // first-frame render.  This prevents the first visible flush from racing the
    // buffer setup path, while preserving normal restart/update behaviour.
    if (reveal_after_prepare) {
        vTaskDelay(pdMS_TO_TICKS(32));
        lv_lock();
        if (!ctx->runtime_hidden) {
            lv_obj_remove_flag(ctx->obj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_invalidate(ctx->obj);
        }
        lv_unlock();
    }

    // Validate animation parameters
    if (!ctx->data_loaded || ctx->exec_cb == nullptr ||
        ctx->duration_ms == 0 || ctx->end_frame <= ctx->start_frame) {
        ESP_LOGW(LOTTIE_TAG, "No valid animation, task suspending");
        vTaskSuspend(NULL);
        return;
    }
    if (!ctx->auto_start) {
        ESP_LOGD(LOTTIE_TAG, "auto_start=false, task suspending");
        vTaskSuspend(NULL);
        return;
    }

    // --- Frame render loop (64 KB PSRAM stack) ---
    int32_t total_frames = ctx->end_frame - ctx->start_frame;
    uint32_t frame_delay_ms = ctx->duration_ms / (uint32_t)total_frames;
    if (frame_delay_ms < 16)  frame_delay_ms = 16;   // Cap at ~60 fps
    if (frame_delay_ms > 100) frame_delay_ms = 100;

    ESP_LOGD(LOTTIE_TAG, "Render loop: %u ms/frame, loop=%d",
             (unsigned)frame_delay_ms, (int)ctx->loop);

    ctx->start_tick = xTaskGetTickCount();  // Store in context for restart capability
    uint32_t perf_frames = 0;
    uint64_t perf_render_us = 0;
    uint64_t perf_sync_us = 0;
    uint32_t perf_render_max_us = 0;
    uint32_t perf_sync_max_us = 0;
    int64_t perf_last_log_us = esp_timer_get_time();

    while (!ctx->stop_requested) {
        if (ctx->runtime_hidden) {
            if (ctx->restart_requested) {
                ctx->start_tick = xTaskGetTickCount();
                ctx->restart_requested = false;
                lv_lock();
                ctx->exec_cb(ctx->anim_var, ctx->start_frame);
                lottie_sync_canvas_buffer(ctx);
                lv_obj_invalidate(ctx->obj);
                lv_unlock();
            }
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        // Check if restart requested
        if (ctx->restart_requested) {
            ctx->start_tick = xTaskGetTickCount();
            ctx->restart_requested = false;
            ESP_LOGD(LOTTIE_TAG, "Animation restarted from frame 0");
        }

        uint32_t elapsed_ms = (uint32_t)((xTaskGetTickCount() - ctx->start_tick) * portTICK_PERIOD_MS);

        int32_t frame;
        if (ctx->loop) {
            uint32_t phase = elapsed_ms % ctx->duration_ms;
            frame = ctx->start_frame + (int32_t)((int64_t)total_frames * phase / ctx->duration_ms);
        } else {
            if (elapsed_ms >= ctx->duration_ms) {
                lv_lock();
                ctx->exec_cb(ctx->anim_var, ctx->end_frame);
                lottie_sync_canvas_buffer(ctx);
                lv_unlock();
                ESP_LOGD(LOTTIE_TAG, "Animation complete");
                break;
            }
            frame = ctx->start_frame + (int32_t)((int64_t)total_frames * elapsed_ms / ctx->duration_ms);
        }

        lv_lock();
        bool perf_log_enabled = false;
        int64_t render_start_us = perf_log_enabled ? esp_timer_get_time() : 0;
        ctx->exec_cb(ctx->anim_var, frame);
        int64_t sync_start_us = perf_log_enabled ? esp_timer_get_time() : 0;
        lottie_sync_canvas_buffer(ctx);
        int64_t sync_end_us = perf_log_enabled ? esp_timer_get_time() : 0;
        lv_unlock();

        if (perf_log_enabled) {
            uint32_t render_us = (uint32_t)(sync_start_us - render_start_us);
            uint32_t sync_us = (uint32_t)(sync_end_us - sync_start_us);
            perf_frames++;
            perf_render_us += render_us;
            perf_sync_us += sync_us;
            if (render_us > perf_render_max_us) perf_render_max_us = render_us;
            if (sync_us > perf_sync_max_us) perf_sync_max_us = sync_us;

            int64_t now_us = sync_end_us;
            if (now_us - perf_last_log_us >= 2000000 && perf_frames > 0) {
                ESP_LOGD(LOTTIE_TAG,
                         "perf2s: frames=%u render_avg=%lluus render_max=%uus sync_avg=%lluus sync_max=%uus",
                         (unsigned)perf_frames,
                         (unsigned long long)(perf_render_us / perf_frames),
                         (unsigned)perf_render_max_us,
                         (unsigned long long)(perf_sync_us / perf_frames),
                         (unsigned)perf_sync_max_us);
                perf_frames = 0;
                perf_render_us = 0;
                perf_sync_us = 0;
                perf_render_max_us = 0;
                perf_sync_max_us = 0;
                perf_last_log_us = now_us;
            }
        } else if (perf_frames != 0) {
            perf_frames = 0;
            perf_render_us = 0;
            perf_sync_us = 0;
            perf_render_max_us = 0;
            perf_sync_max_us = 0;
            perf_last_log_us = esp_timer_get_time();
        }

        vTaskDelay(pdMS_TO_TICKS(frame_delay_ms));
    }

    if (ctx->stop_requested) {
        ESP_LOGD(LOTTIE_TAG, "Stop requested - task suspending");
    }

    // Suspend (NOT delete) - cleanup callback will delete us safely
    vTaskSuspend(NULL);
}

// --------------------------------------------------------------------------
// Free all PSRAM/internal-RAM resources for one Lottie widget.
// --------------------------------------------------------------------------
inline void lottie_free_resources(LottieContext *ctx) {
    ctx->stop_requested = true;
    if (ctx->task_handle) {
        vTaskDelete(ctx->task_handle);
        ctx->task_handle = nullptr;
    }
    if (ctx->task_stack)    { heap_caps_free(ctx->task_stack);    ctx->task_stack = nullptr; }
    if (ctx->task_tcb)      { heap_caps_free(ctx->task_tcb);      ctx->task_tcb = nullptr; }
    if (ctx->pixel_buffer)  { heap_caps_free(ctx->pixel_buffer);  ctx->pixel_buffer = nullptr; }
    ctx->stop_requested = false;

    ESP_LOGD(LOTTIE_TAG, "Lottie freed (%ux%u = %u KB %s buf + 64 KB stack)",
             (unsigned)ctx->width, (unsigned)ctx->height,
             (unsigned)(ctx->width * ctx->height * 4 / 1024),
             ctx->pixel_buffer_internal ? "SRAM" : "PSRAM");
    ctx->pixel_buffer_internal = false;
}

// --------------------------------------------------------------------------
// (Re-)allocate pixel buffer and launch the render task.
// lv_lottie_set_buffer is NOT called here - it is called inside the task
// because it triggers ThorVG rendering which needs the 64 KB stack.
// --------------------------------------------------------------------------
inline bool lottie_launch(LottieContext *ctx) {
    // NOTE: Do NOT re-capture runtime_hidden here!
    // On re-loads the widget is already hidden (by lottie_screen_unload_start_cb),
    // so reading LV_OBJ_FLAG_HIDDEN would always return true, losing the real
    // visibility state that was correctly saved in the unload callback.
    // runtime_hidden is set by:
    //   - lottie_init()                      -> first load (from YAML config)
    //   - lottie_screen_unload_start_cb()    -> re-loads  (actual state before hide)

    // Prefer internal SRAM for small ARGB canvas buffers. Lottie updates the
    // same canvas repeatedly, and keeping that source out of PSRAM avoids a
    // fragile cache/PPA/display-read path on ESP32-P4. Larger animations still
    // fall back to PSRAM to keep the UI bootable.
    size_t buf_bytes = (size_t)ctx->width * ctx->height * 4;
    size_t alloc_bytes = lottie_align_up(buf_bytes, LOTTIE_CACHE_ALIGN);
    ctx->pixel_buffer_internal = false;
    ctx->pixel_buffer = lottie_alloc_pixel_buffer(alloc_bytes, &ctx->pixel_buffer_internal);
    if (!ctx->pixel_buffer) {
        ESP_LOGE(LOTTIE_TAG, "Pixel buffer alloc failed (%u bytes)", (unsigned)alloc_bytes);
        return false;
    }
    memset(ctx->pixel_buffer, 0, alloc_bytes);

    // Hide temporarily during async load (pixel buffer is blank)
    lv_obj_add_flag(ctx->obj, LV_OBJ_FLAG_HIDDEN);

    // Allocate task stack + TCB
    ctx->task_stack = (StackType_t *)heap_caps_malloc(
        LOTTIE_TASK_STACK_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ctx->task_tcb = (StaticTask_t *)heap_caps_malloc(
        sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!ctx->task_stack || !ctx->task_tcb) {
        ESP_LOGE(LOTTIE_TAG, "Task alloc failed");
        lottie_free_resources(ctx);
        return false;
    }

    ctx->stop_requested = false;
    ctx->task_handle = xTaskCreateStatic(
        lottie_load_task, "lottie_anim",
        LOTTIE_TASK_STACK_SIZE / sizeof(StackType_t),
        ctx, 5, ctx->task_stack, ctx->task_tcb);

    if (!ctx->task_handle) {
        lottie_free_resources(ctx);
        return false;
    }

    ESP_LOGD(LOTTIE_TAG, "Lottie launched (runtime_hidden=%d, %s: %u KB buf + 64 KB stack, free PSRAM: %u KB, free SRAM: %u KB, largest SRAM: %u KB)",
             (int)ctx->runtime_hidden,
             ctx->pixel_buffer_internal ? "SRAM" : "PSRAM",
             (unsigned)(buf_bytes / 1024),
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024),
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
             (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) / 1024));
    return true;
}

// --------------------------------------------------------------------------
// Screen event callbacks - two-phase unload to avoid drawing freed buffer
// during screen transition animation.
//
//   SCREEN_UNLOAD_START  -> stop task + hide widget (LVGL still draws screen)
//   SCREEN_UNLOADED      -> free PSRAM (screen no longer visible)
//   SCREEN_LOADED        -> re-allocate and re-launch
// --------------------------------------------------------------------------
inline void lottie_screen_unload_start_cb(lv_event_t *e) {
    LottieContext *ctx = (LottieContext *)lv_event_get_user_data(e);

    // Capture actual visibility BEFORE hiding - this preserves dynamic
    // show/hide from user scripts (e.g. weather widget selection).
    ctx->runtime_hidden = lv_obj_has_flag(ctx->obj, LV_OBJ_FLAG_HIDDEN);

    // Stop the render task immediately
    ctx->stop_requested = true;
    if (ctx->task_handle) {
        vTaskDelete(ctx->task_handle);
        ctx->task_handle = nullptr;
    }

    // Hide widget so LVGL won't try to draw the image during transition
    lv_obj_add_flag(ctx->obj, LV_OBJ_FLAG_HIDDEN);

    ESP_LOGD(LOTTIE_TAG, "Lottie task stopped, widget hidden (was_hidden=%d)", (int)ctx->runtime_hidden);
}

inline void lottie_screen_unloaded_cb(lv_event_t *e) {
    LottieContext *ctx = (LottieContext *)lv_event_get_user_data(e);

    // Now safe to free - screen is no longer visible
    if (ctx->task_stack)    { heap_caps_free(ctx->task_stack);    ctx->task_stack = nullptr; }
    if (ctx->task_tcb)      { heap_caps_free(ctx->task_tcb);      ctx->task_tcb = nullptr; }
    if (ctx->pixel_buffer)  { heap_caps_free(ctx->pixel_buffer);  ctx->pixel_buffer = nullptr; }
    ctx->stop_requested = false;

    ESP_LOGD(LOTTIE_TAG, "Lottie FREED (%ux%u = %u KB %s buf + 64 KB stack) -> free PSRAM: %u KB, free SRAM: %u KB",
             (unsigned)ctx->width, (unsigned)ctx->height,
             (unsigned)(ctx->width * ctx->height * 4 / 1024),
             ctx->pixel_buffer_internal ? "SRAM" : "PSRAM",
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024),
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024));
    ctx->pixel_buffer_internal = false;
}

inline void lottie_screen_loaded_cb(lv_event_t *e) {
    LottieContext *ctx = (LottieContext *)lv_event_get_user_data(e);
    if (ctx->pixel_buffer == nullptr) {
        lottie_launch(ctx);
    }
}

// --------------------------------------------------------------------------
// Public API: Restart animation from frame 0 (preserves loop/hidden state)
// Safe to call at any time - sets a flag checked by the render loop.
// --------------------------------------------------------------------------
inline void lottie_restart(LottieContext *ctx) {
    if (ctx && ctx->task_handle) {
        ctx->restart_requested = true;
        ESP_LOGD(LOTTIE_TAG, "Restart requested (will reset on next frame)");
    }
}

// --------------------------------------------------------------------------
// Public API: initialise Lottie widget - allocate buffer, register screen
// events, and launch the load/render task.
// Call under lv_lock (from LVGL init code).
// --------------------------------------------------------------------------
inline bool lottie_init(lv_obj_t *obj, const void *data, size_t data_size,
                         const char *file_path, uint32_t width, uint32_t height,
                         bool loop, bool auto_start, bool user_wants_hidden) {
    LottieContext *ctx = (LottieContext *)heap_caps_malloc(
        sizeof(LottieContext), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!ctx) return false;
    memset(ctx, 0, sizeof(LottieContext));

    ctx->obj       = obj;
    ctx->data      = data;
    ctx->data_size = data_size;
    ctx->file_path = file_path;
    ctx->loop      = loop;
    ctx->auto_start = auto_start;
    ctx->width     = width;
    ctx->height    = height;
    ctx->user_wants_hidden = user_wants_hidden;  // Save user's 'hidden' config from YAML
    ctx->runtime_hidden = user_wants_hidden;    // Initially matches YAML config

    // Store context on the LVGL object so user scripts can retrieve it
    // via lv_obj_get_user_data() for lottie_restart() calls
    lv_obj_set_user_data(obj, ctx);

    // Register screen events for PSRAM lifecycle (two-phase unload).
    // IMPORTANT: We do NOT call lottie_launch() here.  Resources are only
    // allocated when the page actually becomes visible (SCREEN_LOADED event).
    // This prevents non-active pages from consuming PSRAM at startup.
    // With 19+ widgets across multiple pages, this saves megabytes of PSRAM.
    lv_obj_t *screen = lv_obj_get_screen(obj);
    lv_obj_add_event_cb(screen, lottie_screen_unload_start_cb,
                        LV_EVENT_SCREEN_UNLOAD_START, ctx);
    lv_obj_add_event_cb(screen, lottie_screen_unloaded_cb,
                        LV_EVENT_SCREEN_UNLOADED, ctx);
    lv_obj_add_event_cb(screen, lottie_screen_loaded_cb,
                        LV_EVENT_SCREEN_LOADED, ctx);

    ESP_LOGD(LOTTIE_TAG, "Lottie registered (%ux%u), waiting for page load to allocate",
             (unsigned)width, (unsigned)height);
    return true;
}

}  // namespace lvgl
}  // namespace esphome

#endif  // LV_USE_LOTTIE
#endif  // USE_ESP32
