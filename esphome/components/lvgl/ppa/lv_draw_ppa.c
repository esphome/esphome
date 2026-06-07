/**
 * @file lv_draw_ppa.c
 * Fixed PPA draw unit for LVGL 9.4 on ESP32-P4
 * Backported from https://github.com/lvgl/lvgl/pull/9162
 * Adapted for C++ compilation (ESPHome build system)
 */

#include "sdkconfig.h"
#ifdef CONFIG_SOC_PPA_SUPPORTED

#include "lv_draw_ppa_private.h"
#include "lv_draw_ppa.h"

#ifndef LV_PPA_BURST_LENGTH
#define LV_PPA_BURST_LENGTH (128)
#endif

#if LV_PPA_BURST_LENGTH == 128
#define LV_DRAW_PPA_DATA_BURST_LENGTH PPA_DATA_BURST_LENGTH_128
#elif LV_PPA_BURST_LENGTH == 64
#define LV_DRAW_PPA_DATA_BURST_LENGTH PPA_DATA_BURST_LENGTH_64
#elif LV_PPA_BURST_LENGTH == 32
#define LV_DRAW_PPA_DATA_BURST_LENGTH PPA_DATA_BURST_LENGTH_32
#elif LV_PPA_BURST_LENGTH == 16
#define LV_DRAW_PPA_DATA_BURST_LENGTH PPA_DATA_BURST_LENGTH_16
#elif LV_PPA_BURST_LENGTH == 8
#define LV_DRAW_PPA_DATA_BURST_LENGTH PPA_DATA_BURST_LENGTH_8
#else
#error "LV_PPA_BURST_LENGTH must be 8, 16, 32, 64 or 128"
#endif

/*********************
 *      DEFINES
 *********************/
#define PPA_BUF_ALIGN (16) /* PPA needs at least 16-byte aligned buffers (128-bit burst) */

static const char *TAG = "ppa_draw";
static uint32_t s_ppa_fill_tasks = 0;
static uint32_t s_ppa_img_tasks = 0;

/* Check if a draw buffer is suitable for PPA (non-NULL, aligned, has data) */
static inline bool ppa_buf_usable(lv_draw_buf_t *buf) {
  if (buf == NULL || buf->data == NULL || buf->data_size == 0)
    return false;
  if (((uintptr_t) buf->data) % PPA_BUF_ALIGN != 0)
    return false;
  return true;
}

/**********************
 *  STATIC PROTOTYPES
 **********************/
static int32_t ppa_evaluate(lv_draw_unit_t *draw_unit, lv_draw_task_t *task);
static int32_t ppa_dispatch(lv_draw_unit_t *draw_unit, lv_layer_t *layer);
static int32_t ppa_delete(lv_draw_unit_t *draw_unit);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_draw_ppa_init(void) {
  lv_draw_ppa_unit_t *draw_ppa_unit = (lv_draw_ppa_unit_t *) lv_draw_create_unit(sizeof(lv_draw_ppa_unit_t));
  draw_ppa_unit->base_unit.evaluate_cb = ppa_evaluate;
  draw_ppa_unit->base_unit.dispatch_cb = ppa_dispatch;
  draw_ppa_unit->base_unit.delete_cb = ppa_delete;

  ESP_LOGD(TAG, "PPA draw unit registered, idx=%d", (int) draw_ppa_unit->base_unit.idx);

  /* Register PPA clients */
  esp_err_t res;
  ppa_client_config_t cfg;
  lv_memzero(&cfg, sizeof(cfg));

  /* Register SRM client */
  cfg.oper_type = PPA_OPERATION_SRM;
  cfg.max_pending_trans_num = 1;
  cfg.data_burst_length = LV_DRAW_PPA_DATA_BURST_LENGTH;

  res = ppa_register_client(&cfg, &draw_ppa_unit->srm_client);
  if (res != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register SRM client: %d", res);
  }

  /* Register Fill client */
  lv_memzero(&cfg, sizeof(cfg));
  cfg.oper_type = PPA_OPERATION_FILL;
  cfg.max_pending_trans_num = 1;
  cfg.data_burst_length = LV_DRAW_PPA_DATA_BURST_LENGTH;

  res = ppa_register_client(&cfg, &draw_ppa_unit->fill_client);
  if (res != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register Fill client: %d", res);
  }

  /* Register Blend client */
  lv_memzero(&cfg, sizeof(cfg));
  cfg.oper_type = PPA_OPERATION_BLEND;
  cfg.max_pending_trans_num = 1;
  cfg.data_burst_length = LV_DRAW_PPA_DATA_BURST_LENGTH;

  res = ppa_register_client(&cfg, &draw_ppa_unit->blend_client);
  if (res != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register Blend client: %d", res);
  }
}

void lv_draw_ppa_deinit(void) {}

uint32_t lv_draw_ppa_get_fill_task_count(void) { return s_ppa_fill_tasks; }

uint32_t lv_draw_ppa_get_img_task_count(void) { return s_ppa_img_tasks; }

/**********************
 *   STATIC FUNCTIONS
 **********************/
static int32_t ppa_evaluate(lv_draw_unit_t *draw_unit, lv_draw_task_t *t) {
  switch (t->type) {
    case LV_DRAW_TASK_TYPE_FILL: {
      const lv_draw_fill_dsc_t *dsc = (const lv_draw_fill_dsc_t *) t->draw_dsc;
      if (dsc->radius != 0)
        return 0;
      if (dsc->grad.dir != LV_GRAD_DIR_NONE)
        return 0;
      if (dsc->opa < (lv_opa_t) LV_OPA_MAX)
        return 0;

      lv_draw_buf_t *draw_buf = t->target_layer->draw_buf;
      if (!ppa_buf_usable(draw_buf))
        return 0;
      if (!ppa_dest_cf_supported((lv_color_format_t) draw_buf->header.cf))
        return 0;

      if (t->preference_score > 40) {
        t->preference_score = 40;
        t->preferred_draw_unit_id = draw_unit->idx;
      }
      return 1;
    }

    case LV_DRAW_TASK_TYPE_IMAGE: {
      const lv_draw_image_dsc_t *dsc = (const lv_draw_image_dsc_t *) t->draw_dsc;

#ifdef LV_USE_PPA_IMG
      /* PPA SRM handles 90-degree increment rotation in hardware */
      if (dsc->rotation != 0) {
        int32_t angle = dsc->rotation % 3600;
        if (angle < 0)
          angle += 3600;
        /* Only accept exact 90 degree multiples */
        if (angle != 0 && angle != 900 && angle != 1800 && angle != 2700)
          return 0;
        if (dsc->skew_x != 0 || dsc->skew_y != 0)
          return 0;
        if (dsc->opa < (lv_opa_t) LV_OPA_MAX)
          return 0;
        if (dsc->blend_mode != LV_BLEND_MODE_NORMAL)
          return 0;
        if (!ppa_src_cf_supported((lv_color_format_t) dsc->header.cf))
          return 0;

        lv_draw_buf_t *dest_buf = t->target_layer->draw_buf;
        if (!ppa_buf_usable(dest_buf))
          return 0;
        if (!ppa_dest_cf_supported((lv_color_format_t) dest_buf->header.cf))
          return 0;

        /* SRM rotation gets higher priority than software */
        if (t->preference_score > 30) {
          t->preference_score = 30;
          t->preferred_draw_unit_id = draw_unit->idx;
        }
        return 1;
      }
#else
      if (dsc->rotation != 0)
        return 0;
#endif
      if (dsc->skew_x != 0 || dsc->skew_y != 0)
        return 0;

#ifdef LV_USE_PPA_IMG
      /* PPA SRM handles scale+translate (Ken Burns) with rotation=0 */
      if (dsc->scale_x != LV_SCALE_NONE || dsc->scale_y != LV_SCALE_NONE) {
        if (dsc->opa < (lv_opa_t) LV_OPA_MAX)
          return 0;
        if (dsc->blend_mode != LV_BLEND_MODE_NORMAL)
          return 0;
        if (!ppa_src_cf_supported((lv_color_format_t) dsc->header.cf))
          return 0;
        lv_draw_buf_t *scale_dest = t->target_layer->draw_buf;
        if (!ppa_buf_usable(scale_dest))
          return 0;
        if (!ppa_dest_cf_supported((lv_color_format_t) scale_dest->header.cf))
          return 0;
        if (t->preference_score > 50) {
          t->preference_score = 50;
          t->preferred_draw_unit_id = draw_unit->idx;
        }
        return 1;
      }
#else
      if (dsc->scale_x != LV_SCALE_NONE || dsc->scale_y != LV_SCALE_NONE)
        return 0;
#endif
      if (dsc->opa < (lv_opa_t) LV_OPA_MAX)
        return 0;
      if (dsc->blend_mode != LV_BLEND_MODE_NORMAL)
        return 0;
      if (!ppa_src_cf_supported((lv_color_format_t) dsc->header.cf))
        return 0;

      lv_draw_buf_t *dest_buf = t->target_layer->draw_buf;
      if (!ppa_buf_usable(dest_buf))
        return 0;
      if (!ppa_dest_cf_supported((lv_color_format_t) dest_buf->header.cf))
        return 0;

      if (t->preference_score > 30) {
        t->preference_score = 30;
        t->preferred_draw_unit_id = draw_unit->idx;
      }
      return 1;
    }

    default:
      break;
  }

  return 0;
}

static int32_t ppa_dispatch(lv_draw_unit_t *draw_unit, lv_layer_t *layer) {
  lv_draw_ppa_unit_t *u = (lv_draw_ppa_unit_t *) draw_unit;

  /* Already processing a task */
  if (u->task_act) {
    return LV_DRAW_UNIT_IDLE;
  }

  /* Allocate layer buffer once for all tasks in this batch */
  if (lv_draw_layer_alloc_buf(layer) == NULL) {
    return LV_DRAW_UNIT_IDLE;
  }

  lv_layer_t *target = NULL;
  lv_draw_buf_t *buf = NULL;
  bool cache_synced = false;
  int32_t task_count = 0;

  /* Process all available PPA tasks in one dispatch call */
  lv_draw_task_t *t = lv_draw_get_available_task(layer, NULL, draw_unit->idx);
  while (t && t->preferred_draw_unit_id == draw_unit->idx) {
    t->state = LV_DRAW_TASK_STATE_IN_PROGRESS;
    t->draw_unit = draw_unit;
    u->task_act = t;

    target = t->target_layer;
    buf = (target) ? target->draw_buf : NULL;

    if (buf != NULL && buf->data != NULL) {
      /* Flush CPU cache once before first PPA operation */
      if (!cache_synced) {
        lv_draw_ppa_cache_sync(buf);
        cache_synced = true;
      }

      switch (t->type) {
        case LV_DRAW_TASK_TYPE_FILL:
          s_ppa_fill_tasks++;
          lv_draw_ppa_fill(t, (lv_draw_fill_dsc_t *) t->draw_dsc, &t->area);
          break;
        case LV_DRAW_TASK_TYPE_IMAGE: {
          s_ppa_img_tasks++;
          lv_draw_image_dsc_t *img_dsc = (lv_draw_image_dsc_t *) t->draw_dsc;
#ifdef LV_USE_PPA_IMG
          if (img_dsc->rotation != 0) {
            lv_draw_ppa_img_rotate(t, img_dsc, &t->area);
          } else if (img_dsc->scale_x != LV_SCALE_NONE || img_dsc->scale_y != LV_SCALE_NONE) {
            lv_draw_ppa_img_srm(t, img_dsc, &t->area);
          } else
#endif
          {
            lv_draw_ppa_img(t, img_dsc, &t->area);
          }
          break;
        }
        default:
          break;
      }
    }

    t->state = LV_DRAW_TASK_STATE_FINISHED;
    u->task_act = NULL;
    task_count++;

    /* Get next available task */
    t = lv_draw_get_available_task(layer, NULL, draw_unit->idx);
  }

  if (task_count > 0) {
    /* Single cache invalidate after all PPA operations */
    if (cache_synced && buf != NULL) {
      lv_draw_ppa_cache_sync(buf);
    }
    lv_draw_dispatch_request();
    return 1;
  }

  return LV_DRAW_UNIT_IDLE;
}

static int32_t ppa_delete(lv_draw_unit_t *draw_unit) {
  LV_UNUSED(draw_unit);
  return 0;
}

#endif /* CONFIG_SOC_PPA_SUPPORTED */
