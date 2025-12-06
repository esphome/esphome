#pragma once
#include "esphome/core/defines.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif  // USE_BINARY_SENSOR
#ifdef USE_IMAGE
#include "esphome/components/image/image.h"
#endif  // USE_LVGL_IMAGE
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

#ifdef USE_FONT
#include "esphome/components/font/font.h"
#endif  // USE_LVGL_FONT
#ifdef USE_TOUCHSCREEN
#include "esphome/components/touchscreen/touchscreen.h"
#endif  // USE_LVGL_TOUCHSCREEN

#if defined(USE_LVGL_BUTTONMATRIX) || defined(USE_LVGL_KEYBOARD)
#include "esphome/components/key_provider/key_provider.h"
#endif  // USE_LVGL_BUTTONMATRIX

namespace esphome::lvgl {

#if LV_COLOR_DEPTH == 16
using lv_color_data = uint16_t;
#endif
#if LV_COLOR_DEPTH == 32
using lv_color_data = uint32_t;
#endif

extern lv_event_code_t lv_api_event;     // NOLINT
extern lv_event_code_t lv_update_event;  // NOLINT
extern std::string lv_event_code_name_for(lv_event_t *event);

lv_obj_t *lv_container_create(lv_obj_t *parent);
void lv_scale_draw_event_cb(lv_event_t *e, uint16_t range_start, uint16_t range_end, lv_color_t color_start,
                            lv_color_t color_end, bool local);
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
// Shortcut / overload, so that the source of an image can easily be updated
// from within a lambda.
inline void lv_image_set_src(lv_obj_t *obj, esphome::image::Image *image) {
  lv_image_set_src(obj, image->get_lv_image_dsc());
}

inline void lv_obj_set_style_bg_image_src(lv_obj_t *obj, esphome::image::Image *image, lv_style_selector_t selector) {
  lv_obj_set_style_bg_image_src(obj, image->get_lv_image_dsc(), selector);
}
#endif  // USE_LVGL_IMAGE
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

#ifdef USE_LVGL_METER
int16_t lv_get_needle_angle_for_value(lv_obj_t *obj, int value);
#endif

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

using LvLambdaType = std::function<void(lv_obj_t *)>;
using set_value_lambda_t = std::function<void(float)>;
using event_callback_t = void(lv_event_t *);
using text_lambda_t = std::function<const char *()>;

template<typename... Ts> class ObjUpdateAction : public Action<Ts...> {
 public:
  explicit ObjUpdateAction(std::function<void(Ts...)> &&lamb) : lamb_(std::move(lamb)) {}

  void play(const Ts &...x) override { this->lamb_(x...); }

 protected:
  std::function<void(Ts...)> lamb_;
};
#ifdef USE_LVGL_ANIMIMG
void lv_animimg_stop(lv_obj_t *obj);
#endif  // USE_LVGL_ANIMIMG

class LvglComponent : public PollingComponent {
  constexpr static const char *const TAG = "lvgl";

 public:
  LvglComponent(std::vector<display::Display *> displays, float buffer_frac, bool full_refresh, int draw_rounding,
                bool resume_on_input, bool update_when_display_idle);
  static void static_flush_cb(lv_display_t *disp_drv, const lv_area_t *area, uint8_t *color_p);

  float get_setup_priority() const override { return setup_priority::PROCESSOR; }
  void setup() override;
  void update() override;
  void loop() override;
  void add_on_idle_callback(std::function<void(uint32_t)> &&callback) {
    this->idle_callbacks_.add(std::move(callback));
  }

  static void render_end_cb(lv_event_t *event);
  static void render_start_cb(lv_event_t *event);
  void dump_config() override;
  lv_disp_t *get_disp() { return this->disp_; }
  lv_obj_t *get_screen_active() { return lv_display_get_screen_active(this->disp_); }
  // Pause or resume the display.
  // @param paused If true, pause the display. If false, resume the display.
  // @param show_snow If true, show the snow effect when paused.
  void set_paused(bool paused, bool show_snow);

  // Returns true if the display is explicitly paused, or a blocking display update is in progress.
  bool is_paused() const;
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
  static void add_event_cb(lv_obj_t *obj, event_callback_t callback, lv_event_code_t event);
  static void add_event_cb(lv_obj_t *obj, event_callback_t callback, lv_event_code_t event1, lv_event_code_t event2);
  static void add_event_cb(lv_obj_t *obj, event_callback_t callback, lv_event_code_t event1, lv_event_code_t event2,
                           lv_event_code_t event3);

  void add_page(LvPageType *page);
  void show_page(size_t index, lv_scr_load_anim_t anim, uint32_t time);
  void show_next_page(lv_scr_load_anim_t anim, uint32_t time);
  void show_prev_page(lv_scr_load_anim_t anim, uint32_t time);
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

  display::DisplayRotation rotation{display::DISPLAY_ROTATION_0_DEGREES};
  void set_pause_trigger(Trigger<> *trigger) { this->pause_callback_ = trigger; }
  void set_resume_trigger(Trigger<> *trigger) { this->resume_callback_ = trigger; }
  void set_draw_start_trigger(Trigger<> *trigger) { this->draw_start_callback_ = trigger; }
  void set_draw_end_trigger(Trigger<> *trigger) { this->draw_end_callback_ = trigger; }

 protected:
  void draw_end_();
  // Not checking for non-null callback since the
  // LVGL callback that calls it is not set in that case
  void draw_start_() const { this->draw_start_callback_->trigger(); }

  void write_random_();
  void draw_buffer_(const lv_area_t *area, lv_color_data *ptr);
  void flush_cb_(lv_display_t *disp_drv, const lv_area_t *area, uint8_t *color_p);

  std::vector<display::Display *> displays_{};
  size_t buffer_frac_{1};
  bool full_refresh_{};
  bool resume_on_input_{};
  bool update_when_display_idle_{};

  uint8_t *draw_buf_{};
  lv_display_t *disp_{};
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
  void *rotate_buf_{};
};

class IdleTrigger : public Trigger<> {
 public:
  explicit IdleTrigger(LvglComponent *parent, TemplatableValue<uint32_t> timeout);

 protected:
  TemplatableValue<uint32_t> timeout_;
  bool is_idle_{};
};

template<typename... Ts> class LvglAction : public Action<Ts...>, public Parented<LvglComponent> {
 public:
  explicit LvglAction(std::function<void(LvglComponent *)> &&lamb) : action_(std::move(lamb)) {}
  void play(const Ts &...x) override { this->action_(this->parent_); }

 protected:
  std::function<void(LvglComponent *)> action_{};
};

template<typename Tc, typename... Ts> class LvglCondition : public Condition<Ts...>, public Parented<Tc> {
 public:
  LvglCondition(std::function<bool(Tc *)> &&condition_lambda) : condition_lambda_(std::move(condition_lambda)) {}
  bool check(const Ts &...x) override { return this->condition_lambda_(this->parent_); }

 protected:
  std::function<bool(Tc *)> condition_lambda_{};
};

#ifdef USE_LVGL_TOUCHSCREEN
class LVTouchListener : public touchscreen::TouchListener, public Parented<LvglComponent> {
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

void lv_image_set_needle_value(lv_obj_t *obj, int value);
void lv_arc_set_needle_value(lv_obj_t *obj, int value);

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
class LVEncoderListener : public Parented<LvglComponent> {
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
