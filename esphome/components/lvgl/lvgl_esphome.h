#pragma once
#include "esphome/core/defines.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif  // USE_BINARY_SENSOR
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
#include <vector>
#include <map>
#ifdef USE_LVGL_IMAGE
#include "esphome/components/image/image.h"
#endif  // USE_LVGL_IMAGE

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
extern bool lv_is_pre_initialise();
#if LV_COLOR_DEPTH == 16
static const display::ColorBitness LV_BITNESS = display::ColorBitness::COLOR_BITNESS_565;
#elif LV_COLOR_DEPTH == 32
static const display::ColorBitness LV_BITNESS = display::ColorBitness::COLOR_BITNESS_888;
#else   // LV_COLOR_DEPTH
static const display::ColorBitness LV_BITNESS = display::ColorBitness::COLOR_BITNESS_332;
#endif  // LV_COLOR_DEPTH

// Parent class for things that wrap an LVGL object
class LvCompound {
 public:
  virtual void set_obj(lv_obj_t *lv_obj) { this->obj = lv_obj; }
  lv_obj_t *obj{};
};

class LvPageType {
 public:
  LvPageType(bool skip) : skip(skip) {}

  void setup(size_t index) {
    this->index = index;
    this->obj = lv_obj_create(nullptr);
  }
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
#ifdef USE_LVGL_IMAGE
lv_img_dsc_t *lv_img_from(image::Image *src, lv_img_dsc_t *img_dsc = nullptr);
#endif  // USE_LVGL_IMAGE

#ifdef USE_LVGL_ANIMIMG
void lv_animimg_stop(lv_obj_t *obj);
#endif  // USE_LVGL_ANIMIMG

class LvglComponent : public PollingComponent {
  constexpr static const char *const TAG = "lvgl";

 public:
  static void static_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);

  float get_setup_priority() const override { return setup_priority::PROCESSOR; }
  void setup() override;
  void update() override;
  void loop() override;
  void add_on_idle_callback(std::function<void(uint32_t)> &&callback) {
    this->idle_callbacks_.add(std::move(callback));
  }
  void add_display(display::Display *display) { this->displays_.push_back(display); }
  void add_init_lambda(const std::function<void(LvglComponent *)> &lamb) { this->init_lambdas_.push_back(lamb); }
  void dump_config() override;
  void set_full_refresh(bool full_refresh) { this->full_refresh_ = full_refresh; }
  bool is_idle(uint32_t idle_ms) { return lv_disp_get_inactive_time(this->disp_) > idle_ms; }
  void set_buffer_frac(size_t frac) { this->buffer_frac_ = frac; }
  lv_disp_t *get_disp() { return this->disp_; }
  void set_paused(bool paused, bool show_snow);
  void add_event_cb(lv_obj_t *obj, event_callback_t callback, lv_event_code_t event);
  void add_event_cb(lv_obj_t *obj, event_callback_t callback, lv_event_code_t event1, lv_event_code_t event2);
  void add_event_cb(lv_obj_t *obj, event_callback_t callback, lv_event_code_t event1, lv_event_code_t event2,
                    lv_event_code_t event3);
  bool is_paused() const { return this->paused_; }
  void add_page(LvPageType *page);
  void show_page(size_t index, lv_scr_load_anim_t anim, uint32_t time);
  void show_next_page(lv_scr_load_anim_t anim, uint32_t time);
  void show_prev_page(lv_scr_load_anim_t anim, uint32_t time);
  void set_page_wrap(bool wrap) { this->page_wrap_ = wrap; }
  void set_focus_mark(lv_group_t *group) { this->focus_marks_[group] = lv_group_get_focused(group); }
  void restore_focus_mark(lv_group_t *group) {
    auto *mark = this->focus_marks_[group];
    if (mark != nullptr) {
      lv_group_focus_obj(mark);
    }
  }

 protected:
  void write_random_();
  void draw_buffer_(const lv_area_t *area, const uint8_t *ptr);
  void flush_cb_(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
  std::vector<display::Display *> displays_{};
  lv_disp_draw_buf_t draw_buf_{};
  lv_disp_drv_t disp_drv_{};
  lv_disp_t *disp_{};
  bool paused_{};
  std::vector<LvPageType *> pages_{};
  size_t current_page_{0};
  bool show_snow_{};
  lv_coord_t snow_line_{};
  bool page_wrap_{true};
  std::map<lv_group_t *, lv_obj_t *> focus_marks_{};

  std::vector<std::function<void(LvglComponent *lv_component)>> init_lambdas_;
  CallbackManager<void(uint32_t)> idle_callbacks_{};
  size_t buffer_frac_{1};
  bool full_refresh_{};
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
  void play(Ts... x) override { this->action_(this->parent_); }

 protected:
  std::function<void(LvglComponent *)> action_{};
};

template<typename... Ts> class LvglCondition : public Condition<Ts...>, public Parented<LvglComponent> {
 public:
  LvglCondition(std::function<bool(LvglComponent *)> &&condition_lambda)
      : condition_lambda_(std::move(condition_lambda)) {}
  bool check(Ts... x) override { return this->condition_lambda_(this->parent_); }

 protected:
  std::function<bool(LvglComponent *)> condition_lambda_{};
};

#ifdef USE_LVGL_TOUCHSCREEN
class LVTouchListener : public touchscreen::TouchListener, public Parented<LvglComponent> {
 public:
  LVTouchListener(uint16_t long_press_time, uint16_t long_press_repeat_time);
  void update(const touchscreen::TouchPoints_t &tpoints) override;
  void release() override { touch_pressed_ = false; }
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

  void set_left_button(binary_sensor::BinarySensor *left_button) {
    left_button->add_on_state_callback([this](bool state) { this->event(LV_KEY_LEFT, state); });
  }
  void set_right_button(binary_sensor::BinarySensor *right_button) {
    right_button->add_on_state_callback([this](bool state) { this->event(LV_KEY_RIGHT, state); });
  }

  void set_enter_button(binary_sensor::BinarySensor *enter_button) {
    enter_button->add_on_state_callback([this](bool state) { this->event(LV_KEY_ENTER, state); });
  }

#ifdef USE_LVGL_ROTARY_ENCODER
  void set_sensor(rotary_encoder::RotaryEncoderSensor *sensor) {
    sensor->register_listener([this](int32_t count) { this->set_count(count); });
  }
#endif  // USE_LVGL_ROTARY_ENCODER

  void event(int key, bool pressed) {
    if (!this->parent_->is_paused()) {
      this->pressed_ = pressed;
      this->key_ = key;
    }
  }

  void set_count(int32_t count) {
    if (!this->parent_->is_paused())
      this->count_ = count;
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
