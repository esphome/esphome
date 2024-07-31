#pragma once
#include "esphome/core/defines.h"

#ifdef USE_LVGL_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif  // USE_LVGL_BINARY_SENSOR
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
#include <utility>
#include <vector>
#ifdef USE_LVGL_IMAGE
#include "esphome/components/image/image.h"
#endif  // USE_LVGL_IMAGE

#ifdef USE_LVGL_FONT
#include "esphome/components/font/font.h"
#endif  // USE_LVGL_FONT
#ifdef USE_LVGL_TOUCHSCREEN
#include "esphome/components/touchscreen/touchscreen.h"
#endif  // USE_LVGL_TOUCHSCREEN

namespace esphome {
namespace lvgl {

extern lv_event_code_t lv_custom_event;  // NOLINT
#ifdef USE_LVGL_COLOR
inline lv_color_t lv_color_from(Color color) { return lv_color_make(color.red, color.green, color.blue); }
#endif  // USE_LVGL_COLOR
#if LV_COLOR_DEPTH == 16
static const display::ColorBitness LV_BITNESS = display::ColorBitness::COLOR_BITNESS_565;
#elif LV_COLOR_DEPTH == 32
static const display::ColorBitness LV_BITNESS = display::ColorBitness::COLOR_BITNESS_888;
#else   // LV_COLOR_DEPTH
static const display::ColorBitness LV_BITNESS = display::ColorBitness::COLOR_BITNESS_332;
#endif  // LV_COLOR_DEPTH

// Parent class for things that wrap an LVGL object
class LvCompound final {
 public:
  void set_obj(lv_obj_t *lv_obj) { this->obj = lv_obj; }
  lv_obj_t *obj{};
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

class LvglComponent : public PollingComponent {
  constexpr static const char *const TAG = "lvgl";

 public:
  static void static_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    reinterpret_cast<LvglComponent *>(disp_drv->user_data)->flush_cb_(disp_drv, area, color_p);
  }

  float get_setup_priority() const override { return setup_priority::PROCESSOR; }
  static void log_cb(const char *buf) {
    esp_log_printf_(ESPHOME_LOG_LEVEL_INFO, TAG, 0, "%.*s", (int) strlen(buf) - 1, buf);
  }
  static void rounder_cb(lv_disp_drv_t *disp_drv, lv_area_t *area) {
    // make sure all coordinates are even
    if (area->x1 & 1)
      area->x1--;
    if (!(area->x2 & 1))
      area->x2++;
    if (area->y1 & 1)
      area->y1--;
    if (!(area->y2 & 1))
      area->y2++;
  }

  void setup() override;

  void update() override {
    // update indicators
    if (this->paused_) {
      return;
    }
    this->idle_callbacks_.call(lv_disp_get_inactive_time(this->disp_));
  }

  void loop() override {
    if (this->paused_) {
      if (this->show_snow_)
        this->write_random_();
    }
    lv_timer_handler_run_in_period(5);
  }

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
  void set_paused(bool paused, bool show_snow) {
    this->paused_ = paused;
    this->show_snow_ = show_snow;
    this->snow_line_ = 0;
    if (!paused && lv_scr_act() != nullptr) {
      lv_disp_trig_activity(this->disp_);  // resets the inactivity time
      lv_obj_invalidate(lv_scr_act());
    }
  }

  void add_event_cb(lv_obj_t *obj, event_callback_t callback, lv_event_code_t event) {
    lv_obj_add_event_cb(obj, callback, event, this);
    if (event == LV_EVENT_VALUE_CHANGED) {
      lv_obj_add_event_cb(obj, callback, lv_custom_event, this);
    }
  }
  bool is_paused() const { return this->paused_; }

 protected:
  void write_random_();
  void draw_buffer_(const lv_area_t *area, const uint8_t *ptr);
  void flush_cb_(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
  std::vector<display::Display *> displays_{};
  lv_disp_draw_buf_t draw_buf_{};
  lv_disp_drv_t disp_drv_{};
  lv_disp_t *disp_{};
  bool paused_{};
  bool show_snow_{};
  lv_coord_t snow_line_{};

  std::vector<std::function<void(LvglComponent *lv_component)>> init_lambdas_;
  CallbackManager<void(uint32_t)> idle_callbacks_{};
  size_t buffer_frac_{1};
  bool full_refresh_{};
};

class IdleTrigger : public Trigger<> {
 public:
  explicit IdleTrigger(LvglComponent *parent, TemplatableValue<uint32_t> timeout) : timeout_(std::move(timeout)) {
    parent->add_on_idle_callback([this](uint32_t idle_time) {
      if (!this->is_idle_ && idle_time > this->timeout_.value()) {
        this->is_idle_ = true;
        this->trigger();
      } else if (this->is_idle_ && idle_time < this->timeout_.value()) {
        this->is_idle_ = false;
      }
    });
  }

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
  LVTouchListener(uint16_t long_press_time, uint16_t long_press_repeat_time) {
    lv_indev_drv_init(&this->drv_);
    this->drv_.long_press_repeat_time = long_press_repeat_time;
    this->drv_.long_press_time = long_press_time;
    this->drv_.type = LV_INDEV_TYPE_POINTER;
    this->drv_.user_data = this;
    this->drv_.read_cb = [](lv_indev_drv_t *d, lv_indev_data_t *data) {
      auto *l = static_cast<LVTouchListener *>(d->user_data);
      if (l->touch_pressed_) {
        data->point.x = l->touch_point_.x;
        data->point.y = l->touch_point_.y;
        data->state = LV_INDEV_STATE_PRESSED;
      } else {
        data->state = LV_INDEV_STATE_RELEASED;
      }
    };
  }
  void update(const touchscreen::TouchPoints_t &tpoints) override {
    this->touch_pressed_ = !this->parent_->is_paused() && !tpoints.empty();
    if (this->touch_pressed_)
      this->touch_point_ = tpoints[0];
  }
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
  LVEncoderListener(lv_indev_type_t type, uint16_t lpt, uint16_t lprt) {
    lv_indev_drv_init(&this->drv_);
    this->drv_.type = type;
    this->drv_.user_data = this;
    this->drv_.long_press_time = lpt;
    this->drv_.long_press_repeat_time = lprt;
    this->drv_.read_cb = [](lv_indev_drv_t *d, lv_indev_data_t *data) {
      auto *l = static_cast<LVEncoderListener *>(d->user_data);
      data->state = l->pressed_ ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
      data->key = l->key_;
      data->enc_diff = (int16_t) (l->count_ - l->last_count_);
      l->last_count_ = l->count_;
      data->continue_reading = false;
    };
  }

  void set_left_button(binary_sensor::BinarySensor *left_button) {
    left_button->add_on_state_callback([this](bool state) { this->event(LV_KEY_LEFT, state); });
  }
  void set_right_button(binary_sensor::BinarySensor *right_button) {
    right_button->add_on_state_callback([this](bool state) { this->event(LV_KEY_RIGHT, state); });
  }

  void set_enter_button(binary_sensor::BinarySensor *enter_button) {
    enter_button->add_on_state_callback([this](bool state) { this->event(LV_KEY_ENTER, state); });
  }

  void set_sensor(rotary_encoder::RotaryEncoderSensor *sensor) {
    sensor->register_listener([this](int32_t count) { this->set_count(count); });
  }

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
#endif  // USE_LVGL_KEY_LISTENER
}  // namespace lvgl
}  // namespace esphome
