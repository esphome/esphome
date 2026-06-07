/**
 * @file lv_draw_ppa_fill.c
 * Fixed PPA fill operation for LVGL 9.4 on ESP32-P4
 * Backported from https://github.com/lvgl/lvgl/pull/9162
 * Adapted for C++ compilation (ESPHome build system)
 */

#include "sdkconfig.h"
#ifdef CONFIG_SOC_PPA_SUPPORTED

#include "lv_draw_ppa_private.h"
#include "lv_draw_ppa.h"

void lv_draw_ppa_fill(lv_draw_task_t * t, const lv_draw_fill_dsc_t * dsc,
                      const lv_area_t * coords)
{
    lv_draw_ppa_unit_t * u = (lv_draw_ppa_unit_t *)t->draw_unit;
    lv_draw_buf_t * draw_buf = t->target_layer->draw_buf;

    lv_area_t rel_coords;
    lv_area_copy(&rel_coords, coords);
    lv_area_move(&rel_coords, -t->target_layer->buf_area.x1, -t->target_layer->buf_area.y1);

    lv_area_t rel_clip_area;
    lv_area_copy(&rel_clip_area, &t->clip_area);
    lv_area_move(&rel_clip_area, -t->target_layer->buf_area.x1, -t->target_layer->buf_area.y1);

    lv_area_t blend_area;
    if(!lv_area_intersect(&blend_area, &rel_coords, &rel_clip_area))
        return; /*Fully clipped, nothing to do*/

    ppa_fill_oper_config_t fill_cfg;
    lv_memzero(&fill_cfg, sizeof(fill_cfg));

    fill_cfg.fill_argb_color.val = lv_color_to_u32(dsc->color);
    fill_cfg.out.block_offset_x  = (uint32_t)blend_area.x1;
    fill_cfg.out.block_offset_y  = (uint32_t)blend_area.y1;
    fill_cfg.out.fill_cm         = lv_color_format_to_ppa_fill((lv_color_format_t)draw_buf->header.cf);
    fill_cfg.fill_block_w        = (uint32_t)lv_area_get_width(&blend_area);
    fill_cfg.fill_block_h        = (uint32_t)lv_area_get_height(&blend_area);
    fill_cfg.out.buffer          = draw_buf->data;
    /* PPA hardware rejects unaligned out.buffer_size (issue #9868). */
    fill_cfg.out.buffer_size     = lv_draw_ppa_align_size(draw_buf->data_size);
    fill_cfg.out.pic_w           = draw_buf->header.w;
    fill_cfg.out.pic_h           = draw_buf->header.h;
    fill_cfg.mode                = PPA_TRANS_MODE_BLOCKING;

    esp_err_t ret = ppa_do_fill(u->fill_client, &fill_cfg);
    if(ret != ESP_OK) {
        LV_LOG_ERROR("PPA fill failed: %d", ret);
    }
}

#endif /* CONFIG_SOC_PPA_SUPPORTED */
