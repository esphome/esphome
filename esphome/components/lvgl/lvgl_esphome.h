#pragma once
#include "esphome/core/defines.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif  // USE_BINARY_SENSOR
#ifdef USE_IMAGE
#include "esphome/components/image/image.h"
#endif  // USE_IMAGE
#ifdef USE_LVGL_ROTARY_ENCODER
#include "esphome/components/rotary_encoder/rotary_encoder.h"
#endif  // USE_LVGL_ROTARY_ENCODER

// required for clang-tidy
#ifndef LV_CONF_H
#define LV_CONF_SKIP 1  // NOLINT
#endif                  // LV_CONF_H

#include "esphome/components/display/display.h"
#include "esphome/components/display/display_color_utils.h"
#include "esphome/core/component.h"

#include <list>
#include <lvgl.h>
#include <map>
#include <utility>
#include <vector>

#ifdef USE_ESP32_VARIANT_ESP32P4
#include "driver/ppa.h"
#endif

#ifdef USE_FONT
#include "esphome/components/font/font.h"
#endif  // USE_FONT
#ifdef USE_TOUCHSCREEN
#include "esphome/components/touchscreen/touchscreen.h"
#endif  // USE_TOUCHSCREEN

#if defined(USE_LVGL_BUTTONMATRIX) || defined(USE_LVGL_KEYBOARD)
#include "esphome/components/key_provider/key_provider.h"
#endif  // USE_LVGL_BUTTONMATRIX

#ifdef USE_LVGL_CHART
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
#endif  // USE_LVGL_CHART

namespace esphome::lvgl {

#if LV_COLOR_DEPTH == 16
using lv_color_data = uint16_t;
#endif
#if LV_COLOR_DEPTH == 32
using lv_color_data = uint32_t;
#endif

extern lv_event_code_t lv_update_event;  // NOLINT
extern std::string lv_event_code_name_for(lv_event_t *event);

lv_obj_t *lv_container_create(lv_obj_t *parent);
#ifdef USE_LVGL_SCALE
void lv_scale_draw_event_cb(lv_event_t *e, int16_t range_start, int16_t range_end, lv_color_t color_start,
                            lv_color_t color_end, int width, bool local);
#endif
#if LV_COLOR_DEPTH == 16
static const display::ColorBitness LV_BITNESS = display::ColorBitness::COLOR_BITNESS_565;
#elif LV_COLOR_DEPTH == 32
static const display::ColorBitness LV_BITNESS = display::ColorBitness::COLOR_BITNESS_888;
#else   // LV_COLOR_DEPTH
static const display::ColorBitness LV_BITNESS = display::ColorBitness::COLOR_BITNESS_332;
#endif  // LV_COLOR_DEPTH

#if defined(USE_FONT) && defined(USE_LVGL_FONT)
inline void lv_obj_set_style_text_font(lv_obj_t *obj, const font::Font *font, lv_style_selector_t part) {
  lv_obj_set_style_text_font(obj, font->get_lv_font(), part);
}
inline void lv_style_set_text_font(lv_style_t *style, const font::Font *font) {
  lv_style_set_text_font(style, font->get_lv_font());
}
#endif

#ifdef USE_IMAGE
#ifdef USE_LVGL_IMAGE
// Shortcut / overload, so that the source of an image widget can easily be updated from within a lambda.
inline void lv_image_set_src(lv_obj_t *obj, image::Image *image) { ::lv_image_set_src(obj, image->get_lv_image_dsc()); }

inline void lv_obj_set_style_bitmap_mask_src(lv_obj_t *obj, image::Image *image, lv_style_selector_t selector) {
  ::lv_obj_set_style_bitmap_mask_src(obj, image->get_lv_image_dsc(), selector);
}

inline void lv_obj_set_style_bg_image_src(lv_obj_t *obj, image::Image *image, lv_style_selector_t selector) {
  ::lv_obj_set_style_bg_image_src(obj, image->get_lv_image_dsc(), selector);
}
inline void lv_style_set_bg_image_src(lv_style_t *style, image::Image *image) {
  ::lv_style_set_bg_image_src(style, image->get_lv_image_dsc());
}
inline void lv_style_set_bitmap_mask_src(lv_style_t *style, image::Image *image) {
  ::lv_style_set_bitmap_mask_src(style, image->get_lv_image_dsc());
}
#endif

#ifdef USE_LVGL_ANIMIMG
inline void lv_animimg_set_src(lv_obj_t *img, std::vector<image::Image *> images) {
  auto *dsc = static_cast<std::vector<lv_image_dsc_t *> *>(lv_obj_get_user_data(img));
  if (dsc == nullptr) {
    // object will be lazily allocated but never freed.
    dsc = new std::vector<lv_image_dsc_t *>(images.size());  // NOLINT
    lv_obj_set_user_data(img, dsc);
  }
  dsc->clear();
  for (auto &image : images) {
    dsc->push_back(image->get_lv_image_dsc());
  }
  lv_animimg_set_src(img, (const void **) dsc->data(), dsc->size());
}
#endif  // USE_LVGL_ANIMIMG
#endif  // USE_IMAGE

#ifdef USE_LVGL_METER
int16_t lv_get_needle_angle_for_value(lv_obj_t *obj, int32_t value);
#endif

#ifdef USE_LVGL_GRADIENT
/**
 *
 * @param dsc The gradient descriptor containing the color stops
 * @param pos The current position to calculate the color for
 * @return The color for the given position
 */

lv_color_t lv_grad_calculate_color(const lv_grad_dsc_t *dsc, int32_t pos);
#endif  // USE_LVGL_GRADIENT

// Parent class for things that wrap an LVGL object
class LvCompound {
 public:
  virtual ~LvCompound() = default;
  virtual void set_obj(lv_obj_t *lv_obj) { this->obj = lv_obj; }
  lv_obj_t *obj{};
};

class LvglComponent;

class LvPageType : public Parented<LvglComponent> {
 public:
  LvPageType(bool skip) : skip(skip) {}

  void setup(size_t index) {
    this->index = index;
    this->obj = lv_obj_create(nullptr);
  }

  bool is_showing() const;

  lv_obj_t *obj{};
  size_t index{};
  bool skip;
};

using event_callback_t = void(lv_event_t *);

class LvLambdaComponent final : public Component {
 public:
  LvLambdaComponent(void (*callback)()) : callback_(callback) {}

  void setup() override { this->callback_(); }
  // execute after the LvglComponent is setup
  float get_setup_priority() const override { return setup_priority::PROCESSOR - 5; }

 protected:
  void (*callback_)();
};

template<typename... Ts> class ObjUpdateAction final : public Action<Ts...> {
 public:
  explicit ObjUpdateAction(std::function<void(Ts...)> &&lamb) : lamb_(std::move(lamb)) {}

 protected:
  void play(const Ts &...x) override { this->lamb_(x...); }

  std::function<void(Ts...)> lamb_;
};
#ifdef USE_LVGL_ANIMIMG
void lv_animimg_stop(lv_obj_t *obj);
#endif  // USE_LVGL_ANIMIMG
enum RotationType : uint8_t {
  ROTATION_UNUSED,
  ROTATION_SOFTWARE,
  ROTATION_HARDWARE,
};

enum class Orientation : uint8_t {
  UNKNOWN,
  LANDSCAPE,
  PORTRAIT,
};

class LvglComponent final : public PollingComponent {
  constexpr static const char *const TAG = "lvgl";

 public:
  LvglComponent(std::vector<display::Display *> displays, float buffer_frac, bool full_refresh, int draw_rounding,
                bool resume_on_input, bool update_when_display_idle, RotationType rotation_type);
  static void static_flush_cb(lv_display_t *disp_drv, const lv_area_t *area, uint8_t *color_p);
  /**
   *
   * @param obj A widget
   * @return The position of the last indev point relative to the widget's origin.
   */
  static lv_point_t get_touch_relative_to_obj(lv_obj_t *obj);

  float get_setup_priority() const override { return setup_priority::PROCESSOR; }
  void setup() override;
  void update() override;
  void loop() override;
  template<typename F> void add_on_idle_callback(F &&callback) { this->idle_callbacks_.add(std::forward<F>(callback)); }

  static void render_end_cb(lv_event_t *event);
  static void render_start_cb(lv_event_t *event);
  void dump_config() override;
  lv_display_t *get_disp() { return this->disp_; }
  lv_obj_t *get_screen_active() { return lv_display_get_screen_active(this->disp_); }
  // Pause or resume the display.
  // @param paused If true, pause the display. If false, resume the display.
  // @param show_snow If true, show the snow effect when paused.
  void set_paused(bool paused, bool show_snow);
  void set_refresh_interval(uint32_t period) {
    this->refr_timer_period_ = period;
    if (this->refr_timer_ != nullptr)
      lv_timer_set_period(this->refr_timer_, period);
  }

  // Returns true if the display has been explicitly paused via set_paused().
  bool is_paused() const { return this->paused_; }
  // If the display is paused and we have resume_on_input_ set to true, resume the display.
  void maybe_wakeup() {
    if (this->paused_ && this->resume_on_input_) {
      this->set_paused(false, false);
    }
  }

  /**
   * Initialize the LVGL library and register custom events.
   */
  static void esphome_lvgl_init();

  //  Convenience overloads for adding a callback for one or more events
  static void add_event_cb(lv_obj_t *obj, event_callback_t callback, lv_event_code_t event);
  static void add_event_cb(lv_obj_t *obj, event_callback_t callback, lv_event_code_t event1, lv_event_code_t event2);
  static void add_event_cb(lv_obj_t *obj, event_callback_t callback, lv_event_code_t event1, lv_event_code_t event2,
                           lv_event_code_t event3);

  // change the state of a widget and fire an event if changed (only needed for CHECKED)

  static void lv_obj_set_state_value(lv_obj_t *obj, lv_state_t state, bool value) {
    if (value != lv_obj_has_state(obj, state)) {
      if (value) {
        lv_obj_add_state(obj, state);
      } else {
        lv_obj_remove_state(obj, state);
      }
      if (state == LV_STATE_CHECKED)
        lv_obj_send_event(obj, lv_update_event, nullptr);
    }
  }

  // change the state of a buttonmatrix button and fire an event if changed (only needed for CHECKED)
#ifdef USE_LVGL_BUTTONMATRIX
  static void lv_buttonmatrix_set_button_ctrl_value(lv_obj_t *obj, uint32_t index, lv_buttonmatrix_ctrl_t ctrl,
                                                    bool value) {
    if (value != lv_buttonmatrix_has_button_ctrl(obj, index, ctrl)) {
      if (value) {
        lv_buttonmatrix_set_button_ctrl(obj, index, ctrl);
      } else {
        lv_buttonmatrix_clear_button_ctrl(obj, index, ctrl);
      }
      if (ctrl == LV_BUTTONMATRIX_CTRL_CHECKED)
        lv_obj_send_event(obj, lv_update_event, nullptr);
    }
  }
#endif

  void add_page(LvPageType *page);
  void show_page(size_t index, lv_screen_load_anim_t anim, uint32_t time);
  void show_next_page(lv_screen_load_anim_t anim, uint32_t time);
  void show_prev_page(lv_screen_load_anim_t anim, uint32_t time);
  void set_page_wrap(bool wrap) { this->page_wrap_ = wrap; }
  void set_big_endian(bool big_endian) { this->big_endian_ = big_endian; }
  size_t get_current_page() const;
  void set_focus_mark(lv_group_t *group) { this->focus_marks_[group] = lv_group_get_focused(group); }
  void restore_focus_mark(lv_group_t *group) {
    auto *mark = this->focus_marks_[group];
    if (mark != nullptr) {
      lv_group_focus_obj(mark);
    }
  }
  // rounding factor to align bounds of update area when drawing
  size_t draw_rounding{2};

  void set_pause_trigger(Trigger<> *trigger) { this->pause_callback_ = trigger; }
  void set_resume_trigger(Trigger<> *trigger) { this->resume_callback_ = trigger; }
  void set_draw_start_trigger(Trigger<> *trigger) { this->draw_start_callback_ = trigger; }
  void set_draw_end_trigger(Trigger<> *trigger) { this->draw_end_callback_ = trigger; }
  void set_landscape_trigger(Trigger<> *trigger) { this->landscape_callback_ = trigger; }
  void set_portrait_trigger(Trigger<> *trigger) { this->portrait_callback_ = trigger; }
  void set_rotation(display::DisplayRotation rotation);
  /// Set the rotation from an angle in degrees. Must be a multiple of 90.
  void set_rotation(int angle);
  display::DisplayRotation get_rotation() const { return this->rotation_; }
  void rotate_coordinates(int32_t &x, int32_t &y) const;

  uint16_t get_width() const { return lv_display_get_horizontal_resolution(this->disp_); }
  uint16_t get_height() const { return lv_display_get_vertical_resolution(this->disp_); }

 protected:
  void set_resolution_() const;
  // Determine the current orientation from the effective resolution and fire the
  // landscape/portrait trigger if it has changed since the last check.
  void update_orientation_();
  void draw_end_();
  // Not checking for non-null callback since the
  // LVGL callback that calls it is not set in that case
  void draw_start_() const { this->draw_start_callback_->trigger(); }
  // Returns true if update_when_display_idle is enabled and at least one underlying display
  // component is currently busy (e.g. mid-refresh).
  bool displays_busy_() const;

  void write_random_();
  void draw_buffer_(const lv_area_t *area, lv_color_data *ptr);
#ifdef USE_ESP32_VARIANT_ESP32P4
  bool ppa_rotate_(const lv_color_data *src, lv_color_data *dst, uint16_t width, uint16_t height,
                   uint32_t height_rounded);
#endif
  void flush_cb_(lv_display_t *disp_drv, const lv_area_t *area, uint8_t *color_p);

  std::vector<display::Display *> displays_{};
  size_t buffer_frac_{1};
  bool full_refresh_{};
  bool resume_on_input_{};
  bool update_when_display_idle_{};

  uint8_t *draw_buf_{};
  lv_display_t *disp_{};
  // The display's own periodic refresh timer, effectively paused while the display is busy (see
  // displays_busy_()) so LVGL neither renders nor flushes to it, without losing track of
  // invalidated areas. Other timers (indev reading, animations, ...) keep running as normal.
  lv_timer_t *refr_timer_{};
  // Tracks whether refr_timer_ is currently paused, so loop() can detect the busy -> idle edge
  // and kick off an immediate refresh instead of waiting for the timer's next natural period.
  bool refr_timer_paused_{};
  uint32_t refr_timer_period_{16};
  uint16_t width_{};
  uint16_t height_{};
  bool paused_{};
  std::vector<LvPageType *> pages_{};
  size_t current_page_{0};
  bool show_snow_{};
  bool page_wrap_{true};
  bool big_endian_{};
  std::map<lv_group_t *, lv_obj_t *> focus_marks_{};

  CallbackManager<void(uint32_t)> idle_callbacks_{};
  Trigger<> *pause_callback_{};
  Trigger<> *resume_callback_{};
  Trigger<> *draw_start_callback_{};
  Trigger<> *draw_end_callback_{};
  Trigger<> *landscape_callback_{};
  Trigger<> *portrait_callback_{};
  Orientation orientation_{Orientation::UNKNOWN};
  void *rotate_buf_{};
  display::DisplayRotation rotation_{display::DISPLAY_ROTATION_0_DEGREES};
  RotationType rotation_type_;
#ifdef USE_ESP32_VARIANT_ESP32P4
  ppa_client_handle_t ppa_client_{};
#endif
};

class IdleTrigger final : public Trigger<> {
 public:
  explicit IdleTrigger(LvglComponent *parent, TemplatableFn<uint32_t> timeout);

 protected:
  TemplatableFn<uint32_t> timeout_;
  bool is_idle_{};
};

template<typename... Ts> class LvglAction final : public Action<Ts...>, public Parented<LvglComponent> {
 public:
  explicit LvglAction(std::function<void(LvglComponent *)> &&lamb) : action_(std::move(lamb)) {}

 protected:
  void play(const Ts &...x) override { this->action_(this->parent_); }
  std::function<void(LvglComponent *)> action_{};
};

template<typename Tc, typename... Ts> class LvglCondition final : public Condition<Ts...>, public Parented<Tc> {
 public:
  LvglCondition(std::function<bool(Tc *)> &&condition_lambda) : condition_lambda_(std::move(condition_lambda)) {}
  bool check(const Ts &...x) override { return this->condition_lambda_(this->parent_); }

 protected:
  std::function<bool(Tc *)> condition_lambda_{};
};

#ifdef USE_LVGL_TOUCHSCREEN
class LVTouchListener final : public touchscreen::TouchListener, public Parented<LvglComponent> {
 public:
  LVTouchListener(uint16_t long_press_time, uint16_t long_press_repeat_time, LvglComponent *parent);
  void update(const touchscreen::TouchPoints_t &tpoints) override;
  void release() override {
    touch_pressed_ = false;
    this->parent_->maybe_wakeup();
  }
  lv_indev_t *get_drv() { return this->drv_; }

 protected:
  lv_indev_t *drv_{};
  touchscreen::TouchPoint touch_point_{};
  bool touch_pressed_{};
};
#endif  // USE_LVGL_TOUCHSCREEN

#ifdef USE_LVGL_METER

class IndicatorLine : public LvCompound {
 public:
  IndicatorLine() = default;

  void set_obj(lv_obj_t *lv_obj) override;

  void set_value(int value);

 private:
  void update_length_();

  int16_t angle_{};
  lv_point_precise_t points_[2]{};
};
#endif

#ifdef USE_LVGL_KEY_LISTENER
class LVEncoderListener final : public Parented<LvglComponent> {
 public:
  LVEncoderListener(lv_indev_type_t type, uint16_t long_press_time, uint16_t long_press_repeat_time);

#ifdef USE_BINARY_SENSOR
  void add_button(binary_sensor::BinarySensor *button, lv_key_t key) {
    button->add_on_state_callback([this, key](bool state) { this->event(key, state); });
  }
#endif

#ifdef USE_LVGL_ROTARY_ENCODER
  void set_sensor(rotary_encoder::RotaryEncoderSensor *sensor) {
    sensor->register_listener([this](int32_t count) { this->set_count(count); });
  }
#endif  // USE_LVGL_ROTARY_ENCODER

  void event(int key, bool pressed) {
    if (!this->parent_->is_paused()) {
      this->pressed_ = pressed;
      this->key_ = key;
    } else if (!pressed) {
      // maybe wakeup on release if paused
      this->parent_->maybe_wakeup();
    }
  }

  void set_count(int32_t count) {
    if (!this->parent_->is_paused()) {
      this->count_ = count;
    } else {
      this->parent_->maybe_wakeup();
    }
  }

  lv_indev_t *get_drv() { return this->drv_; }

 protected:
  lv_indev_t *drv_{};
  bool pressed_{};
  int32_t count_{};
  int32_t last_count_{};
  int key_{};
};
#endif  //  USE_LVGL_KEY_LISTENER

#ifdef USE_LVGL_LINE
class LvLineType : public LvCompound {
 public:
  void set_points(FixedVector<lv_point_precise_t> points) {
    this->points_ = std::move(points);
    lv_line_set_points(this->obj, this->points_.begin(), this->points_.size());
  }

 protected:
  FixedVector<lv_point_precise_t> points_{};
};
#endif

#ifdef USE_LVGL_CHART
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
#endif  // USE_LVGL_CHART

#if defined(USE_LVGL_DROPDOWN) || defined(LV_USE_ROLLER)
class LvSelectable : public LvCompound {
 public:
  virtual size_t get_selected_index() = 0;
  virtual void set_selected_index(size_t index, lv_anim_enable_t anim) = 0;
  void set_selected_text(const std::string &text, lv_anim_enable_t anim);
  std::string get_selected_text();
  const std::vector<std::string> &get_options() { return this->options_; }
  void set_options(std::vector<std::string> options);

 protected:
  virtual void set_option_string(const char *options) = 0;
  std::vector<std::string> options_{};
};

#ifdef USE_LVGL_DROPDOWN
class LvDropdownType : public LvSelectable {
 public:
  size_t get_selected_index() override { return lv_dropdown_get_selected(this->obj); }
  void set_selected_index(size_t index, lv_anim_enable_t anim) override { lv_dropdown_set_selected(this->obj, index); }

 protected:
  void set_option_string(const char *options) override { lv_dropdown_set_options(this->obj, options); }
};
#endif  // USE_LVGL_DROPDOWN

#ifdef USE_LVGL_ROLLER
class LvRollerType : public LvSelectable {
 public:
  size_t get_selected_index() override { return lv_roller_get_selected(this->obj); }
  void set_selected_index(size_t index, lv_anim_enable_t anim) override {
    lv_roller_set_selected(this->obj, index, anim);
  }
  void set_mode(lv_roller_mode_t mode) { this->mode_ = mode; }

 protected:
  void set_option_string(const char *options) override { lv_roller_set_options(this->obj, options, this->mode_); }
  lv_roller_mode_t mode_{LV_ROLLER_MODE_NORMAL};
};
#endif
#endif  // defined(USE_LVGL_DROPDOWN) || defined(LV_USE_ROLLER)

#ifdef USE_LVGL_BUTTONMATRIX
class LvButtonMatrixType : public key_provider::KeyProvider, public LvCompound {
 public:
  void set_obj(lv_obj_t *lv_obj) override;
  uint16_t get_selected() { return lv_buttonmatrix_get_selected_button(this->obj); }
  void set_key(size_t idx, uint8_t key) { this->key_map_[idx] = key; }

 protected:
  std::map<size_t, uint8_t> key_map_{};
};
#endif  // USE_LVGL_BUTTONMATRIX

#ifdef USE_LVGL_KEYBOARD
class LvKeyboardType : public key_provider::KeyProvider, public LvCompound {
 public:
  void set_obj(lv_obj_t *lv_obj) override;
};
#endif  // USE_LVGL_KEYBOARD
}  // namespace esphome::lvgl
