#include "lvgl_esphome.h"

#ifdef USE_LVGL_CHART
#include <cinttypes>
#include "esphome/core/log.h"

namespace esphome::lvgl {

ESPHOME_LOG_TAG(TAG, "lvgl.chart");

uint32_t chart_point_slot(lv_obj_t *chart, lv_chart_series_t *series, uint32_t index) {
  const uint32_t count = lv_chart_get_point_count(chart);
  if (index >= count) {
    // A negative index from a lambda arrives as a large unsigned value, so print it signed
    ESP_LOGW(TAG, "Index %" PRId32 " is outside the %" PRIu32 " points of the chart", static_cast<int32_t>(index),
             count);
    return index;
  }
  if (lv_chart_get_update_mode(chart) != LV_CHART_UPDATE_MODE_SHIFT)
    return index;
  return (lv_chart_get_x_start_point(chart, series) + index) % count;
}

}  // namespace esphome::lvgl

#endif  // USE_LVGL_CHART
