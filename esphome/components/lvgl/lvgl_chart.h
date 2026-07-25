#pragma once
#include "esphome/core/defines.h"

#ifdef USE_LVGL_CHART
#include "lvgl_esphome.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <type_traits>
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/preferences.h"
#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif  // USE_TIME

namespace esphome::lvgl {

// Floor/ceil `v` to the nearest multiple of `unit`, rounding outward (away from zero handled
// correctly for negatives -- plain integer division truncates toward zero, which would round the
// wrong way for negative bounds). `inline`: this header can be included by multiple translation
// units.
inline int32_t lv_chart_floor_to_unit(int32_t v, int32_t unit) {
  int32_t r = v % unit;
  if (r != 0 && v < 0)
    r -= unit;
  return v - r;
}
inline int32_t lv_chart_ceil_to_unit(int32_t v, int32_t unit) {
  int32_t r = v % unit;
  if (r != 0 && v > 0)
    r -= unit;
  return v - r;
}

// Trivially-copyable, fixed-size flash blob for `persist: true`. Series values are stored
// oldest-to-newest (a plain shift-array, not lv_chart's internal ring buffer -- lv_chart_series_t
// is opaque/private API in LVGL 9.x, so this class never reaches into it). N_SERIES/N_POINTS come
// from LvChartType's own template arguments -- see there for why this is per-instance-sized rather
// than a single shared shape.
template<uint8_t N_SERIES, uint16_t N_POINTS> struct ChartStore {
  // Guards the blob *contents* on load, independent of chart.py's CHART_PERSIST_VERSION (which
  // guards the preference *key* instead) -- see that constant's comment. Bump both together.
  uint8_t version{1};
  int64_t last_ts{0};
  int32_t y[N_SERIES][N_POINTS]{};
};

// N_SERIES/N_POINTS are baked in per widget instance by
// esphome/components/lvgl/widgets/chart.py's validate_chart() (`config[CONF_ID].type = lv_chart_t
// .template(len(series), point_count)`), so each chart's persisted-history blob (ChartStore, see
// above) is sized to exactly what that one chart needs -- no sharing/waste between differently
// sized charts in the same config. The trade-off: each distinct (series, point_count) combination
// used across a config is a distinct class instantiation, i.e. its own copy of this code in flash.
template<uint8_t N_SERIES, uint16_t N_POINTS> class LvChartType : public LvCompound {
  static_assert(std::is_trivially_copyable<ChartStore<N_SERIES, N_POINTS>>::value, "ChartStore must be POD for prefs");

 public:
  void set_decimals(uint8_t decimals) { this->scale_ = std::pow(10.0f, decimals); }
  void set_point_count(uint16_t n) {
    this->point_count_ = n;
    lv_chart_set_point_count(this->obj, n);
  }
  // LV_PART_INDICATOR's size is the only point-marker control lv_chart exposes, and it applies to
  // every series on the chart at once -- there is no per-series point size in LVGL 9.x.
  void set_point_radius(int32_t radius) { lv_obj_set_style_size(this->obj, radius * 2, radius * 2, LV_PART_INDICATOR); }
  void set_y_range(float min_v, float max_v) {
    this->auto_range_ = false;
    auto min_scaled = static_cast<int32_t>(min_v * this->scale_);
    auto max_scaled = static_cast<int32_t>(max_v * this->scale_);
    lv_chart_set_axis_range(this->obj, LV_CHART_AXIS_PRIMARY_Y, min_scaled, max_scaled);
    this->update_axis_labels_(min_scaled, max_scaled);
  }
  // n is the number of series; point_count must already be set (set_point_count runs first in
  // generated code). Allocates the RAM-only sliding-window buffer used for auto-range and, when
  // persistence is enabled, as the source copied into the flash blob. Filled with
  // LV_CHART_POINT_NONE so recompute_range_() correctly ignores not-yet-written slots.
  void init_series(size_t n) {
    this->series_.init(n);
    size_t total = n * this->point_count_;
    this->history_.init(total);
    for (size_t i = 0; i < total; i++)
      this->history_.push_back(LV_CHART_POINT_NONE);
  }
  void add_series(sensor::Sensor *sens, lv_color_t color, const char *format);
  void set_update_interval(uint32_t ms) { this->update_interval_ = ms; }
  void start_updates();
  void on_value(size_t idx, float value);
#ifdef USE_TIME
  void set_time(time::RealTimeClock *t) { this->time_ = t; }
#endif
  void enable_persistence(uint32_t hash);
  // Shows the current Y-range bounds as two labels (top-left = max, bottom-left = min), formatted
  // with `format` (e.g. "%.0f°"). `format` points at a schema-supplied flash string literal.
  void enable_y_axis(const char *format);

 protected:
  void recompute_range_();
  void restore_();
  void save_();
  void update_axis_labels_(int32_t min_scaled, int32_t max_scaled);
  // Oldest-to-newest window for series `idx`; a plain RAM shift-array, entirely independent of
  // lv_chart's own (opaque, private-API-only in LVGL 9.x) internal series storage.
  int32_t *history_row_(size_t idx) { return &this->history_[idx * this->point_count_]; }

  struct ChartSeries {
    lv_chart_series_t *series;
    sensor::Sensor *sensor;
    lv_obj_t *label;     // nullptr if this series has no legend format
    const char *format;  // points at a flash string literal (schema-supplied), never freed
    // Backs label via lv_label_set_text_static(): a persistent buffer instead of a stack temporary
    // means the label just re-points at this same address on every update, so LVGL never has to
    // free+malloc its text storage on the heap (see y_min_buf_/y_max_buf_ below for the same reasoning).
    char label_buf[32]{};
  };

  FixedVector<ChartSeries> series_{};
  FixedVector<int32_t> history_{};  // [n_series][point_count], see history_row_()
  float scale_{10.0f};
  uint16_t point_count_{0};
  uint32_t update_interval_{0};  // 0 => reactive (sensor callbacks) instead of a timer
  bool auto_range_{true};
  bool persist_{false};
  ESPPreferenceObject pref_{};
  ChartStore<N_SERIES, N_POINTS> store_{};  // scratch buffer for save/load only; history_ is the live copy
  lv_obj_t *y_min_label_{nullptr};
  lv_obj_t *y_max_label_{nullptr};
  const char *y_axis_format_{nullptr};  // flash string literal (schema-supplied), never freed
  // Backs y_min_label_/y_max_label_ via lv_label_set_text_static() -- see ChartSeries::label_buf.
  char y_min_buf_[32]{};
  char y_max_buf_[32]{};
#ifdef USE_TIME
  time::RealTimeClock *time_{nullptr};
  bool gap_computed_{false};
#endif
};

template<uint8_t N_SERIES, uint16_t N_POINTS>
void LvChartType<N_SERIES, N_POINTS>::add_series(sensor::Sensor *sens, lv_color_t color, const char *format) {
  auto *series = lv_chart_add_series(this->obj, color, LV_CHART_AXIS_PRIMARY_Y);
  lv_obj_t *label = nullptr;
  if (format != nullptr && format[0] != '\0') {
    label = lv_label_create(this->obj);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, static_cast<lv_coord_t>(this->series_.size() * 14));
  }
  this->series_.push_back(ChartSeries{series, sens, label, format});
}

template<uint8_t N_SERIES, uint16_t N_POINTS> void LvChartType<N_SERIES, N_POINTS>::start_updates() {
  if (this->persist_)
    this->restore_();

  if (this->update_interval_ == 0) {
    // Reactive: push a new point whenever the sensor publishes a state.
    for (size_t idx = 0; idx < this->series_.size(); idx++) {
      this->series_[idx].sensor->add_on_state_callback([this, idx](float value) { this->on_value(idx, value); });
    }
  } else {
    // Interval sampling: a single native LVGL timer reads every series' current state.
    lv_timer_create(
        [](lv_timer_t *timer) {
          auto *self = static_cast<LvChartType *>(lv_timer_get_user_data(timer));
          for (size_t idx = 0; idx < self->series_.size(); idx++)
            self->on_value(idx, self->series_[idx].sensor->get_state());
        },
        this->update_interval_, this);
  }
}

template<uint8_t N_SERIES, uint16_t N_POINTS> void LvChartType<N_SERIES, N_POINTS>::on_value(size_t idx, float value) {
  if (idx >= this->series_.size())
    return;
  auto &s = this->series_[idx];

  int32_t scaled;
  if (std::isnan(value)) {
    scaled = LV_CHART_POINT_NONE;
  } else {
    scaled = static_cast<int32_t>(lroundf(value * this->scale_));
  }
  lv_chart_set_next_value(this->obj, s.series, scaled);

  if (s.label != nullptr && !std::isnan(value)) {
    snprintf(s.label_buf, sizeof(s.label_buf), s.format, static_cast<double>(value));
    lv_label_set_text_static(s.label, s.label_buf);
  }

  if (this->point_count_ > 0) {
    int32_t *row = this->history_row_(idx);
    memmove(row, row + 1, (this->point_count_ - 1) * sizeof(int32_t));
    row[this->point_count_ - 1] = scaled;
  }

  if (this->auto_range_)
    this->recompute_range_();

  if (this->persist_) {
#ifdef USE_TIME
    if (this->time_ != nullptr) {
      auto t = this->time_->now();
      if (t.is_valid())
        this->store_.last_ts = t.timestamp;
    }
#endif
    this->save_();
  }
}

template<uint8_t N_SERIES, uint16_t N_POINTS> void LvChartType<N_SERIES, N_POINTS>::recompute_range_() {
  int32_t min_v = std::numeric_limits<int32_t>::max();
  int32_t max_v = std::numeric_limits<int32_t>::min();
  bool any = false;
  for (size_t idx = 0; idx < this->series_.size(); idx++) {
    const int32_t *row = this->history_row_(idx);
    for (uint16_t k = 0; k < this->point_count_; k++) {
      int32_t v = row[k];
      if (v == LV_CHART_POINT_NONE)
        continue;
      any = true;
      min_v = std::min(min_v, v);
      max_v = std::max(max_v, v);
    }
  }
  if (!any)
    return;

  // Snap the range outward to whole units (real-world integers) instead of hugging the raw data
  // extremes, e.g. 12.7-21.3 -> 12-22.
  int32_t unit = std::max(static_cast<int32_t>(this->scale_), static_cast<int32_t>(1));
  min_v = lv_chart_floor_to_unit(min_v, unit);
  max_v = lv_chart_ceil_to_unit(max_v, unit);
  if (min_v == max_v)
    max_v += unit;

  lv_chart_set_axis_range(this->obj, LV_CHART_AXIS_PRIMARY_Y, min_v, max_v);
  this->update_axis_labels_(min_v, max_v);
}

template<uint8_t N_SERIES, uint16_t N_POINTS> void LvChartType<N_SERIES, N_POINTS>::enable_y_axis(const char *format) {
  this->y_axis_format_ = format;
  this->y_max_label_ = lv_label_create(this->obj);
  this->y_min_label_ = lv_label_create(this->obj);
}

template<uint8_t N_SERIES, uint16_t N_POINTS>
void LvChartType<N_SERIES, N_POINTS>::update_axis_labels_(int32_t min_scaled, int32_t max_scaled) {
  if (this->y_axis_format_ == nullptr)
    return;
  snprintf(this->y_max_buf_, sizeof(this->y_max_buf_), this->y_axis_format_,
           static_cast<double>(max_scaled) / this->scale_);
  lv_label_set_text_static(this->y_max_label_, this->y_max_buf_);
  snprintf(this->y_min_buf_, sizeof(this->y_min_buf_), this->y_axis_format_,
           static_cast<double>(min_scaled) / this->scale_);
  lv_label_set_text_static(this->y_min_label_, this->y_min_buf_);

  // lv_chart insets its own plot/grid drawing by the chart's `pad_left` style; sizing that to fit
  // whichever label is currently widest reserves just enough margin for the text, automatically,
  // instead of requiring the user to guess and hard-code a pixel value. lv_obj_update_layout forces
  // the label's LV_SIZE_CONTENT width to be recomputed for the text just set, rather than reading a
  // stale size left over from the previous value.
  lv_obj_update_layout(this->y_max_label_);
  lv_obj_update_layout(this->y_min_label_);
  int32_t pad_left = std::max(lv_obj_get_width(this->y_max_label_), lv_obj_get_width(this->y_min_label_)) + 4;
  lv_obj_set_style_pad_left(this->obj, pad_left, LV_PART_MAIN);

  // LV_ALIGN_TOP_LEFT/BOTTOM_LEFT (unlike the LV_ALIGN_OUT_* variants) are natively supported by
  // plain lv_obj_align(); negating the same pad_left here cancels the inset lv_obj_align would
  // otherwise apply to any child, landing the label inside the reserved margin instead of at the
  // start of the plot area.
  lv_obj_align(this->y_max_label_, LV_ALIGN_TOP_LEFT, -pad_left, 0);
  lv_obj_align(this->y_min_label_, LV_ALIGN_BOTTOM_LEFT, -pad_left, 0);
}

template<uint8_t N_SERIES, uint16_t N_POINTS> void LvChartType<N_SERIES, N_POINTS>::enable_persistence(uint32_t hash) {
  this->persist_ = true;
  this->pref_ = global_preferences->make_preference<ChartStore<N_SERIES, N_POINTS>>(hash, true);
}

template<uint8_t N_SERIES, uint16_t N_POINTS> void LvChartType<N_SERIES, N_POINTS>::save_() {
  for (size_t idx = 0; idx < this->series_.size(); idx++) {
    const int32_t *row = this->history_row_(idx);
    memcpy(this->store_.y[idx], row, this->point_count_ * sizeof(int32_t));
  }
  this->pref_.save(&this->store_);
}

template<uint8_t N_SERIES, uint16_t N_POINTS> void LvChartType<N_SERIES, N_POINTS>::restore_() {
  // Load straight into the store_ scratch member rather than a local ChartStore: that struct can be
  // several KB (N_SERIES * N_POINTS int32s), and a second copy on the stack risks overflowing the
  // task stack, especially in this deep a call chain (setup() -> start_updates() -> restore_() ->
  // NVS driver internals).
  uint8_t expected_version = this->store_.version;
  if (!this->pref_.load(&this->store_))
    return;  // first boot, or a schema/version change invalidated the old blob
  if (this->store_.version != expected_version) {
    this->store_ = {};  // discard the incompatible load; a later save_() must not persist its version
    return;
  }

  for (size_t idx = 0; idx < this->series_.size(); idx++) {
    int32_t *row = this->history_row_(idx);
    memcpy(row, this->store_.y[idx], this->point_count_ * sizeof(int32_t));
    for (uint16_t k = 0; k < this->point_count_; k++)
      lv_chart_set_next_value(this->obj, this->series_[idx].series, row[k]);
  }
  if (this->auto_range_)
    this->recompute_range_();

#ifdef USE_TIME
  if (this->time_ != nullptr) {
    auto compute_gap = [this]() {
      if (this->gap_computed_ || this->update_interval_ == 0)
        return;
      auto t = this->time_->now();
      if (!t.is_valid())
        return;
      this->gap_computed_ = true;
      int64_t elapsed_s = t.timestamp - this->store_.last_ts;
      int64_t missing = elapsed_s * 1000 / static_cast<int64_t>(this->update_interval_);
      if (missing <= 0)
        return;
      uint16_t to_push = static_cast<uint16_t>(std::min<int64_t>(missing, this->point_count_));
      for (size_t idx = 0; idx < this->series_.size(); idx++) {
        for (uint16_t k = 0; k < to_push; k++)
          this->on_value(idx, NAN);
      }
    };
    if (this->time_->now().is_valid()) {
      compute_gap();
    } else {
      this->time_->add_on_time_sync_callback(compute_gap);
    }
  }
#endif
}

}  // namespace esphome::lvgl

#endif  // USE_LVGL_CHART
