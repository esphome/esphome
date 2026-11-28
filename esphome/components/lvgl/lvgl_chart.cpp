#include "lvgl_esphome.h"

#ifdef USE_LVGL_CHART
#include <cinttypes>
#include <cmath>
#include "esphome/core/log.h"

namespace esphome::lvgl {

ESPHOME_LOG_TAG(TAG, "lvgl.chart");

void chart_set_axis_range(lv_obj_t *chart, lv_chart_axis_t axis, float min, float max, float multiplier) {
  if (std::isnan(min) || std::isnan(max)) {
    ESP_LOGV(TAG, "Axis range bound is NaN, using the default bound");
    if (std::isnan(min))
      min = 0.0f;
    if (std::isnan(max))
      max = 100.0f;
  }
  const int32_t low = chart_point_value(min, multiplier);
  int32_t high = chart_point_value(max, multiplier);
  if (high == low) {
    ESP_LOGV(TAG, "Axis range %" PRId32 " to %" PRId32 " is empty, widening it by 1", low, high);
    high = low + 1;  // cannot overflow: low is at most CHART_POINT_LIMIT
  }
  lv_chart_set_axis_range(chart, axis, low, high);
}

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
