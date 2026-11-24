#pragma once
// Test only: LVGL screen snapshot dumped as RLE hex over the log (esphome#23227), plus chart series readback.
// Needs the build flag -DLV_USE_SNAPSHOT=1. start(tag) takes an RGB565 snapshot of the active screen; step() logs
// four lines of 32 runs per call, so call it from a 20 ms interval; unsnap.py rebuilds the PNG.
#include "esphome/core/log.h"
#include "lvgl.h"
#include <cinttypes>
#include <cstdio>

namespace snaptest {
static constexpr const char *const TAG = "snap";

struct State {
  lv_draw_buf_t *buf{nullptr};
  uint32_t pos{0};  // pixel index
  uint32_t total{0};
  uint32_t runs{0};
  uint32_t sum{0};
  uint32_t line{0};
  uint32_t tag{0};
};
static State st;

inline uint16_t px(uint32_t i) {
  uint32_t w = st.buf->header.w;
  uint32_t y = i / w, x = i % w;
  const uint8_t *row = st.buf->data + y * st.buf->header.stride;
  return row[x * 2] | (row[x * 2 + 1] << 8);
}

inline void start(uint32_t tag) {
  if (st.buf != nullptr)
    return;
  uint32_t t0 = millis();
  lv_draw_buf_t *buf = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB565);
  if (buf == nullptr) {
    ESP_LOGE(TAG, "SNAPFAIL tag=%" PRIu32, tag);
    return;
  }
  st = State{};
  st.buf = buf;
  st.total = buf->header.w * buf->header.h;
  st.tag = tag;
  ESP_LOGI(TAG, "SNAPBEGIN tag=%" PRIu32 " w=%u h=%u stride=%u cf=%u took=%" PRIu32 "ms", tag,
           (unsigned) st.buf->header.w, (unsigned) st.buf->header.h, (unsigned) st.buf->header.stride,
           (unsigned) st.buf->header.cf, millis() - t0);
}

// Emit a few lines per call; call from a short interval so the log keeps up.
inline void step() {
  if (st.buf == nullptr)
    return;
  for (int l = 0; l < 4 && st.pos < st.total; l++) {
    uint32_t first = st.pos;
    char out[32 * 8 + 1];
    int n = 0;
    for (int r = 0; r < 32 && st.pos < st.total; r++) {
      uint16_t c = px(st.pos);
      uint32_t cnt = 1;
      while (st.pos + cnt < st.total && cnt < 0xFFFF && px(st.pos + cnt) == c)
        cnt++;
      n += snprintf(out + n, sizeof(out) - n, "%04x%04x", (unsigned) cnt, (unsigned) c);
      st.sum += cnt * 31 + c;
      st.pos += cnt;
      st.runs++;
    }
    ESP_LOGI(TAG, "SNAP %" PRIu32 " %" PRIu32 " %" PRIu32 " %s", st.tag, st.line++, first, out);
  }
  if (st.pos >= st.total) {
    ESP_LOGI(TAG, "SNAPEND tag=%" PRIu32 " lines=%" PRIu32 " runs=%" PRIu32 " sum=%" PRIu32, st.tag, st.line, st.runs,
             st.sum);
    lv_draw_buf_destroy(st.buf);
    st.buf = nullptr;
  }
}

#if LV_USE_CHART
inline void dump_series(const char *name, lv_obj_t *chart, lv_chart_series_t *ser, bool with_x) {
  uint32_t cnt = lv_chart_get_point_count(chart);
  uint32_t start = lv_chart_get_x_start_point(chart, ser);
  int32_t *y = lv_chart_get_y_array(chart, ser);
  int32_t *x = with_x ? lv_chart_get_x_array(chart, ser) : nullptr;
  char out[400];
  int n = 0;
  // logical order, oldest first, starting at the start point
  for (uint32_t i = 0; i < cnt && n < (int) sizeof(out) - 24; i++) {
    uint32_t k = (start + i) % cnt;
    if (y[k] == LV_CHART_POINT_NONE) {
      n += snprintf(out + n, sizeof(out) - n, "%s-", i ? "," : "");
    } else if (x != nullptr) {
      n += snprintf(out + n, sizeof(out) - n, "%s(%" PRId32 ",%" PRId32 ")", i ? "," : "", x[k], y[k]);
    } else {
      n += snprintf(out + n, sizeof(out) - n, "%s%" PRId32, i ? "," : "", y[k]);
    }
  }
  ESP_LOGI(TAG, "SER %s cnt=%" PRIu32 " start=%" PRIu32 " [%s]", name, cnt, start, out);
}
#endif
}  // namespace snaptest
