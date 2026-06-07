#pragma once

#ifdef USE_ESP32

// Only compile when SVG widget is actually used (LV_USE_SVG=1 in lv_conf.h).
// This header is pulled in via esphome.h for all builds, so we must guard it.
#include <lvgl.h>
#if LV_USE_SVG

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include <cstdio>
#include <cstring>

// ThorVG C API - compiled into LVGL when LV_USE_THORVG_INTERNAL=1.
#include <src/libs/thorvg/thorvg_capi.h>

namespace esphome {
namespace lvgl {

static constexpr size_t SVG_TASK_STACK_SIZE = 64 * 1024;

// Persistent context for each SVG widget - tracks all PSRAM allocations
// so they can be freed on screen unload and re-created on screen load.
struct SvgContext {
  // --- Config (set once, never freed) ---
  lv_obj_t *canvas_obj;
  const char *svg_data;  // PROGMEM pointer (embedded) or nullptr
  size_t svg_data_size;
  const char *file_path;  // string literal (filesystem) or nullptr
  uint32_t width;
  uint32_t height;

  // --- Runtime state (freed on screen unload) ---
  uint32_t *pixel_buffer;   // PSRAM - width*height*4 bytes
  lv_draw_buf_t *draw_buf;  // internal RAM
  StackType_t *task_stack;  // PSRAM - 64 KB
  StaticTask_t *task_tcb;   // internal RAM
  TaskHandle_t task_handle;
  volatile bool task_done;  // set by render task when finished
  bool user_wants_hidden;   // Save user's 'hidden' config before forcing hide during render
};

// --------------------------------------------------------------------------
// Render task - rasterises SVG via ThorVG, then suspends itself.
// The task does NOT self-delete; the cleanup code deletes it so that
// the stack/TCB can be safely freed afterwards.
// --------------------------------------------------------------------------
inline void svg_render_task(void *param) {
  SvgContext *ctx = (SvgContext *) param;

  vTaskDelay(pdMS_TO_TICKS(500));

  // --- Resolve SVG data ---
  const char *svg_data = ctx->svg_data;
  size_t svg_data_size = ctx->svg_data_size;
  char *file_buf = nullptr;

  if (svg_data == nullptr && ctx->file_path != nullptr) {
    LV_LOG_TRACE("Reading SVG from %s ...", ctx->file_path);
    FILE *f = fopen(ctx->file_path, "r");
    if (!f) {
      LV_LOG_ERROR("Cannot open: %s", ctx->file_path);
      goto done;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) {
      fclose(f);
      goto done;
    }
    file_buf = (char *) heap_caps_malloc(sz + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!file_buf) {
      fclose(f);
      goto done;
    }
    size_t nread = fread(file_buf, 1, sz, f);
    fclose(f);
    file_buf[nread] = '\0';
    svg_data = file_buf;
    svg_data_size = nread;
  }

  if (!svg_data || svg_data_size == 0) {
    LV_LOG_ERROR("No SVG data");
    goto done;
  }

  LV_LOG_TRACE("Rendering SVG (%u bytes) to %ux%u ...", (unsigned) svg_data_size, (unsigned) ctx->width,
               (unsigned) ctx->height);

  memset(ctx->pixel_buffer, 0, ctx->width * ctx->height * sizeof(uint32_t));

  {
    tvg_engine_init(TVG_ENGINE_SW, 0);

    Tvg_Canvas *tc = tvg_swcanvas_create();
    if (!tc) {
      LV_LOG_ERROR("swcanvas_create failed");
      goto done;
    }

    if (tvg_swcanvas_set_target(tc, ctx->pixel_buffer, ctx->width, ctx->width, ctx->height, TVG_COLORSPACE_ARGB8888) !=
        TVG_RESULT_SUCCESS) {
      tvg_canvas_destroy(tc);
      goto done;
    }

    Tvg_Paint *pic = tvg_picture_new();
    if (!pic) {
      tvg_canvas_destroy(tc);
      goto done;
    }

    if (tvg_picture_load_data(pic, svg_data, (uint32_t) svg_data_size, "svg", true) != TVG_RESULT_SUCCESS) {
      LV_LOG_ERROR("picture_load_data failed");
      tvg_paint_del(pic);
      tvg_canvas_destroy(tc);
      goto done;
    }

    float ow = 0, oh = 0;
    tvg_picture_get_size(pic, &ow, &oh);
    LV_LOG_TRACE("SVG %.0fx%.0f -> %ux%u", ow, oh, (unsigned) ctx->width, (unsigned) ctx->height);
    tvg_picture_set_size(pic, (float) ctx->width, (float) ctx->height);

    if (tvg_canvas_push(tc, pic) != TVG_RESULT_SUCCESS) {
      tvg_paint_del(pic);
      tvg_canvas_destroy(tc);
      goto done;
    }

    tvg_canvas_draw(tc);
    tvg_canvas_sync(tc);
    tvg_canvas_destroy(tc);

    LV_LOG_TRACE("SVG rendered OK");
  }

  // Restore user's 'hidden' configuration (only show if user didn't set hidden: true)
  lv_lock();
  if (!ctx->user_wants_hidden) {
    lv_obj_remove_flag(ctx->canvas_obj, LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_invalidate(ctx->canvas_obj);
  lv_unlock();

done:
  if (file_buf)
    heap_caps_free(file_buf);
  ctx->task_done = true;
  // Suspend - do NOT vTaskDelete; the cleanup callback will delete us
  // so it can safely free stack + TCB afterwards.
  vTaskSuspend(NULL);
}

// --------------------------------------------------------------------------
// Free all PSRAM/internal-RAM resources for one SVG widget.
// Called from the screen-unload event callback (runs under lv_lock).
// --------------------------------------------------------------------------
inline void svg_free_resources(SvgContext *ctx) {
  // Delete the task first (it is either suspended or blocked on lv_lock)
  if (ctx->task_handle) {
    vTaskDelete(ctx->task_handle);
    ctx->task_handle = nullptr;
  }
  if (ctx->task_stack) {
    heap_caps_free(ctx->task_stack);
    ctx->task_stack = nullptr;
  }
  if (ctx->task_tcb) {
    heap_caps_free(ctx->task_tcb);
    ctx->task_tcb = nullptr;
  }
  if (ctx->pixel_buffer) {
    heap_caps_free(ctx->pixel_buffer);
    ctx->pixel_buffer = nullptr;
  }
  if (ctx->draw_buf) {
    heap_caps_free(ctx->draw_buf);
    ctx->draw_buf = nullptr;
  }
  ctx->task_done = false;

  LV_LOG_TRACE("SVG PSRAM freed (%ux%u = %u KB)", (unsigned) ctx->width, (unsigned) ctx->height,
               (unsigned) (ctx->width * ctx->height * 4 / 1024));
}

// --------------------------------------------------------------------------
// (Re-)allocate buffers and launch the render task.
// Called from svg_setup_and_render (first time) and screen-load callback.
// Must be called under lv_lock.
// --------------------------------------------------------------------------
inline bool svg_launch(SvgContext *ctx) {
  // Allocate pixel buffer in PSRAM
  size_t buf_bytes = (size_t) ctx->width * ctx->height * sizeof(uint32_t);
  ctx->pixel_buffer = (uint32_t *) heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!ctx->pixel_buffer) {
    LV_LOG_ERROR("PSRAM alloc failed (%u bytes)", (unsigned) buf_bytes);
    return false;
  }
  memset(ctx->pixel_buffer, 0, buf_bytes);

  // Create draw-buf and attach to canvas
  ctx->draw_buf = (lv_draw_buf_t *) heap_caps_malloc(sizeof(lv_draw_buf_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!ctx->draw_buf) {
    heap_caps_free(ctx->pixel_buffer);
    ctx->pixel_buffer = nullptr;
    return false;
  }
  lv_draw_buf_init(ctx->draw_buf, ctx->width, ctx->height, LV_COLOR_FORMAT_ARGB8888, 0, ctx->pixel_buffer, buf_bytes);
  lv_draw_buf_set_flag(ctx->draw_buf, LV_IMAGE_FLAGS_MODIFIABLE);
  lv_canvas_set_draw_buf(ctx->canvas_obj, ctx->draw_buf);

  // Hide temporarily during async render (user's config is already saved in ctx->user_wants_hidden)
  lv_obj_add_flag(ctx->canvas_obj, LV_OBJ_FLAG_HIDDEN);

  // Allocate task stack + TCB
  ctx->task_stack = (StackType_t *) heap_caps_malloc(SVG_TASK_STACK_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  ctx->task_tcb = (StaticTask_t *) heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!ctx->task_stack || !ctx->task_tcb) {
    LV_LOG_ERROR("Task alloc failed");
    svg_free_resources(ctx);
    return false;
  }

  ctx->task_done = false;
  ctx->task_handle = xTaskCreateStatic(svg_render_task, "svg_render", SVG_TASK_STACK_SIZE / sizeof(StackType_t), ctx, 5,
                                       ctx->task_stack, ctx->task_tcb);

  if (!ctx->task_handle) {
    svg_free_resources(ctx);
    return false;
  }

  LV_LOG_TRACE("SVG render task launched (%u KB PSRAM)", (unsigned) (SVG_TASK_STACK_SIZE / 1024));
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
inline void svg_screen_unload_start_cb(lv_event_t *e) {
  SvgContext *ctx = (SvgContext *) lv_event_get_user_data(e);

  // Stop the render task immediately
  if (ctx->task_handle) {
    vTaskDelete(ctx->task_handle);
    ctx->task_handle = nullptr;
  }

  // Hide widget so LVGL won't try to draw the canvas during transition
  lv_obj_add_flag(ctx->canvas_obj, LV_OBJ_FLAG_HIDDEN);

  LV_LOG_TRACE("SVG task stopped, widget hidden (transition starting)");
}

inline void svg_screen_unloaded_cb(lv_event_t *e) {
  SvgContext *ctx = (SvgContext *) lv_event_get_user_data(e);

  // Now safe to free - screen is no longer visible
  if (ctx->task_stack) {
    heap_caps_free(ctx->task_stack);
    ctx->task_stack = nullptr;
  }
  if (ctx->task_tcb) {
    heap_caps_free(ctx->task_tcb);
    ctx->task_tcb = nullptr;
  }
  if (ctx->pixel_buffer) {
    heap_caps_free(ctx->pixel_buffer);
    ctx->pixel_buffer = nullptr;
  }
  if (ctx->draw_buf) {
    heap_caps_free(ctx->draw_buf);
    ctx->draw_buf = nullptr;
  }
  ctx->task_done = false;

  LV_LOG_TRACE("SVG PSRAM freed (%ux%u = %u KB)", (unsigned) ctx->width, (unsigned) ctx->height,
               (unsigned) (ctx->width * ctx->height * 4 / 1024));
}

inline void svg_screen_loaded_cb(lv_event_t *e) {
  SvgContext *ctx = (SvgContext *) lv_event_get_user_data(e);
  if (ctx->pixel_buffer == nullptr) {
    svg_launch(ctx);
  }
}

// --------------------------------------------------------------------------
// Public API: set up canvas and render embedded SVG data.
// --------------------------------------------------------------------------
inline bool svg_setup_and_render(lv_obj_t *canvas_obj, const char *svg_data, size_t svg_data_size, uint32_t width,
                                 uint32_t height, bool user_wants_hidden) {
  SvgContext *ctx = (SvgContext *) heap_caps_malloc(sizeof(SvgContext), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!ctx)
    return false;
  memset(ctx, 0, sizeof(SvgContext));

  ctx->canvas_obj = canvas_obj;
  ctx->svg_data = svg_data;
  ctx->svg_data_size = svg_data_size;
  ctx->file_path = nullptr;
  ctx->width = width;
  ctx->height = height;
  ctx->user_wants_hidden = user_wants_hidden;  // Save user's 'hidden' config from YAML

  // Register screen events for PSRAM lifecycle
  lv_obj_t *screen = lv_obj_get_screen(canvas_obj);
  lv_obj_add_event_cb(screen, svg_screen_unload_start_cb, LV_EVENT_SCREEN_UNLOAD_START, ctx);
  lv_obj_add_event_cb(screen, svg_screen_unloaded_cb, LV_EVENT_SCREEN_UNLOADED, ctx);
  lv_obj_add_event_cb(screen, svg_screen_loaded_cb, LV_EVENT_SCREEN_LOADED, ctx);

  return svg_launch(ctx);
}

// --------------------------------------------------------------------------
// Public API: set up canvas and render SVG from filesystem.
// --------------------------------------------------------------------------
inline bool svg_setup_and_render_file(lv_obj_t *canvas_obj, const char *file_path, uint32_t width, uint32_t height,
                                      bool user_wants_hidden) {
  SvgContext *ctx = (SvgContext *) heap_caps_malloc(sizeof(SvgContext), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!ctx)
    return false;
  memset(ctx, 0, sizeof(SvgContext));

  ctx->canvas_obj = canvas_obj;
  ctx->svg_data = nullptr;
  ctx->svg_data_size = 0;
  ctx->file_path = file_path;
  ctx->width = width;
  ctx->height = height;
  ctx->user_wants_hidden = user_wants_hidden;  // Save user's 'hidden' config from YAML

  lv_obj_t *screen = lv_obj_get_screen(canvas_obj);
  lv_obj_add_event_cb(screen, svg_screen_unload_start_cb, LV_EVENT_SCREEN_UNLOAD_START, ctx);
  lv_obj_add_event_cb(screen, svg_screen_unloaded_cb, LV_EVENT_SCREEN_UNLOADED, ctx);
  lv_obj_add_event_cb(screen, svg_screen_loaded_cb, LV_EVENT_SCREEN_LOADED, ctx);

  return svg_launch(ctx);
}

}  // namespace lvgl
}  // namespace esphome

#endif  // LV_USE_SVG
#endif  // USE_ESP32
