#pragma once
#include "esphome/core/defines.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif  // USE_BINARY_SENSOR
#ifdef USE_LVGL_IMAGE
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
#include "esphome/core/log.h"
#include <lvgl.h>
#include <map>
#include <utility>
#include <vector>

#ifdef USE_LVGL_FONT
#include "esphome/components/font/font.h"
#endif  // USE_LVGL_FONT
#ifdef USE_LVGL_TOUCHSCREEN
#include "esphome/components/touchscreen/touchscreen.h"
#endif  // USE_LVGL_TOUCHSCREEN

#if defined(USE_LVGL_BUTTONMATRIX) || defined(USE_LVGL_KEYBOARD)
#include "esphome/components/key_provider/key_provider.h"
#endif  // USE_LVGL_BUTTONMATRIX

namespace esphome {
namespace lvgl {

extern lv_event_code_t lv_api_event;     // NOLINT
extern lv_event_code_t lv_update_event;  // NOLINT
extern std::string lv_event_code_name_for(uint8_t event_code);
#if LV_COLOR_DEPTH == 16
static const display::ColorBitness LV_BITNESS = display::ColorBitness::COLOR_BITNESS_565;
#elif LV_COLOR_DEPTH == 32
static const display::ColorBitness LV_BITNESS = display::ColorBitness::COLOR_BITNESS_888;
#else   // LV_COLOR_DEPTH
static const display::ColorBitness LV_BITNESS = display::ColorBitness::COLOR_BITNESS_332;
#endif  // LV_COLOR_DEPTH

#ifdef USE_LVGL_IMAGE
// Shortcut / overload, so that the source of an image can easily be updated
// from within a lambda.
inline void lv_img_set_src(lv_obj_t *obj, esphome::image::Image *image) {
  lv_img_set_src(obj, image->get_lv_img_dsc());
}
inline void lv_disp_set_bg_image(lv_disp_t *disp, esphome::image::Image *image) {
  lv_disp_set_bg_image(disp, image->get_lv_img_dsc());
}

inline void lv_obj_set_style_bg_img_src(lv_obj_t *obj, esphome::image::Image *image, lv_style_selector_t selector) {
  lv_obj_set_style_bg_img_src(obj, image->get_lv_img_dsc(), selector);
}
#ifdef USE_LVGL_CANVAS
inline void lv_canvas_draw_img(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y, image::Image *image,
                               lv_draw_img_dsc_t *dsc) {
  lv_canvas_draw_img(canvas, x, y, image->get_lv_img_dsc(), dsc);
}
#endif

#ifdef USE_LVGL_METER
inline lv_meter_indicator_t *lv_meter_add_needle_img(lv_obj_t *obj, lv_meter_scale_t *scale, esphome::image::Image *src,
                                                     lv_coord_t pivot_x, lv_coord_t pivot_y) {
  return lv_meter_add_needle_img(obj, scale, src->get_lv_img_dsc(), pivot_x, pivot_y);
}
#endif  // USE_LVGL_METER
#endif  // USE_LVGL_IMAGE
#ifdef USE_LVGL_ANIMIMG
inline void lv_animimg_set_src(lv_obj_t *img, std::vector<image::Image *> images) {
  auto *dsc = static_cast<std::vector<lv_img_dsc_t *> *>(lv_obj_get_user_data(img));
  if (dsc == nullptr) {
    // object will be lazily allocated but never freed.
    dsc = new std::vector<lv_img_dsc_t *>(images.size());  // NOLINT
    lv_obj_set_user_data(img, dsc);
  }
  dsc->clear();
  for (auto &image : images) {
    dsc->push_back(image->get_lv_img_dsc());
  }
  lv_animimg_set_src(img, (const void **) dsc->data(), dsc->size());
}

#endif  // USE_LVGL_ANIMIMG

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
using event_callback_t = void(_lv_event_t *);
using text_lambda_t = std::function<const char *()>;

template<typename... Ts> class ObjUpdateAction : public Action<Ts...> {
 public:
  explicit ObjUpdateAction(std::function<void(Ts...)> &&lamb) : lamb_(std::move(lamb)) {}

  void play(Ts... x) override { this->lamb_(x...); }

 protected:
  std::function<void(Ts...)> lamb_;
};
#ifdef USE_LVGL_FONT
class FontEngine {
 public:
  FontEngine(font::Font *esp_font);
  const lv_font_t *get_lv_font();

  const font::GlyphData *get_glyph_data(uint32_t unicode_letter);
  uint16_t baseline{};
  uint16_t height{};
  uint8_t bpp{};

 protected:
  font::Font *font_{};
  uint32_t last_letter_{};
  const font::GlyphData *last_data_{};
  lv_font_t lv_font_{};
};
#endif  // USE_LVGL_FONT
#ifdef USE_LVGL_ANIMIMG
void lv_animimg_stop(lv_obj_t *obj);
#endif  // USE_LVGL_ANIMIMG

class LvglComponent : public PollingComponent {
  constexpr static const char *const TAG = "lvgl";

 public:
  LvglComponent(std::vector<display::Display *> displays, float buffer_frac, bool full_refresh, int draw_rounding,
                bool resume_on_input);
  static void static_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);

  float get_setup_priority() const override { return setup_priority::PROCESSOR; }
  void setup() override;
  void update() override;
  void loop() override;
  void add_on_idle_callback(std::function<void(uint32_t)> &&callback) {
    this->idle_callbacks_.add(std::move(callback));
  }
  void add_on_pause_callback(std::function<void(bool)> &&callback) { this->pause_callbacks_.add(std::move(callback)); }
  void dump_config() override;
  bool is_idle(uint32_t idle_ms) { return lv_disp_get_inactive_time(this->disp_) > idle_ms; }
  lv_disp_t *get_disp() { return this->disp_; }
  lv_obj_t *get_scr_act() { return lv_disp_get_scr_act(this->disp_); }
  // Pause or resume the display.
  // @param paused If true, pause the display. If false, resume the display.
  // @param show_snow If true, show the snow effect when paused.
  void set_paused(bool paused, bool show_snow);
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
  static void add_event_cb(lv_obj_t *obj, event_callback_t callback, lv_event_code_t event);
  static void add_event_cb(lv_obj_t *obj, event_callback_t callback, lv_event_code_t event1, lv_event_code_t event2);
  static void add_event_cb(lv_obj_t *obj, event_callback_t callback, lv_event_code_t event1, lv_event_code_t event2,
                           lv_event_code_t event3);
  void add_page(LvPageType *page);
  void show_page(size_t index, lv_scr_load_anim_t anim, uint32_t time);
  void show_next_page(lv_scr_load_anim_t anim, uint32_t time);
  void show_prev_page(lv_scr_load_anim_t anim, uint32_t time);
  void set_page_wrap(bool wrap) { this->page_wrap_ = wrap; }
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

 protected:
  void write_random_();
  void draw_buffer_(const lv_area_t *area, lv_color_t *ptr);
  void flush_cb_(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);

  std::vector<display::Display *> displays_{};
  size_t buffer_frac_{1};
  bool full_refresh_{};
  bool resume_on_input_{};

  lv_disp_draw_buf_t draw_buf_{};
  lv_disp_drv_t disp_drv_{};
  lv_disp_t *disp_{};
  bool paused_{};
  std::vector<LvPageType *> pages_{};
  size_t current_page_{0};
  bool show_snow_{};
  bool page_wrap_{true};
  std::map<lv_group_t *, lv_obj_t *> focus_marks_{};

  CallbackManager<void(uint32_t)> idle_callbacks_{};
  CallbackManager<void(bool)> pause_callbacks_{};
  lv_color_t *rotate_buf_{};
};

class IdleTrigger : public Trigger<> {
 public:
  explicit IdleTrigger(LvglComponent *parent, TemplatableValue<uint32_t> timeout);

 protected:
  TemplatableValue<uint32_t> timeout_;
  bool is_idle_{};
};

class PauseTrigger : public Trigger<> {
 public:
  explicit PauseTrigger(LvglComponent *parent, TemplatableValue<bool> paused);

 protected:
  TemplatableValue<bool> paused_;
};

template<typename... Ts> class LvglAction : public Action<Ts...>, public Parented<LvglComponent> {
 public:
  explicit LvglAction(std::function<void(LvglComponent *)> &&lamb) : action_(std::move(lamb)) {}
  void play(Ts... x) override { this->action_(this->parent_); }

 protected:
  std::function<void(LvglComponent *)> action_{};
};

template<typename Tc, typename... Ts> class LvglCondition : public Condition<Ts...>, public Parented<Tc> {
 public:
  LvglCondition(std::function<bool(Tc *)> &&condition_lambda) : condition_lambda_(std::move(condition_lambda)) {}
  bool check(Ts... x) override { return this->condition_lambda_(this->parent_); }

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
  lv_indev_drv_t *get_drv() { return &this->drv_; }

 protected:
  lv_indev_drv_t drv_{};
  touchscreen::TouchPoint touch_point_{};
  bool touch_pressed_{};
};
#endif  // USE_LVGL_TOUCHSCREEN

#ifdef USE_LVGL_KEY_LISTENER
class LVEncoderListener : public Parented<LvglComponent> {
 public:
  LVEncoderListener(lv_indev_type_t type, uint16_t lpt, uint16_t lprt);

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

  lv_indev_drv_t *get_drv() { return &this->drv_; }

 protected:
  lv_indev_drv_t drv_{};
  bool pressed_{};
  int32_t count_{};
  int32_t last_count_{};
  int key_{};
};
#endif  //  USE_LVGL_KEY_LISTENER

#ifdef USE_LVGL_LINE
class LvLineType : public LvCompound {
 public:
  std::vector<lv_point_t> get_points() { return this->points_; }
  void set_points(std::vector<lv_point_t> points) {
    this->points_ = std::move(points);
    lv_line_set_points(this->obj, this->points_.data(), this->points_.size());
  }

 protected:
  std::vector<lv_point_t> points_{};
};
#endif
#if defined(USE_LVGL_DROPDOWN) || defined(LV_USE_ROLLER)
class LvSelectable : public LvCompound {
 public:
  virtual size_t get_selected_index() = 0;
  virtual void set_selected_index(size_t index, lv_anim_enable_t anim) = 0;
  void set_selected_text(const std::string &text, lv_anim_enable_t anim);
  std::string get_selected_text();
  std::vector<std::string> get_options() { return this->options_; }
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
  uint16_t get_selected() { return lv_btnmatrix_get_selected_btn(this->obj); }
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
}  // namespace lvgl
}  // namespace esphome
