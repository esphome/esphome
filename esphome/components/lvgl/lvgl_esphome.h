#pragma once

// required for clang-tidy
#ifndef LV_CONF_SKIP
#define LV_CONF_SKIP 1  // NOLINT
#endif
#include "esphome/components/display/display.h"
#include "esphome/components/key_provider/key_provider.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include <map>

#if LVGL_USES_IMAGE
#include "esphome/components/image/image.h"
#endif
#if LV_USE_FONT
#include "esphome/components/font/font.h"
#endif
#if LV_USE_TOUCHSCREEN
#include "esphome/components/touchscreen/touchscreen.h"
#endif
#if LV_USE_ROTARY_ENCODER
#include "esphome/components/rotary_encoder/rotary_encoder.h"
#endif
#include "esphome/components/display/display_color_utils.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "lvgl_hal.h"
#include <lvgl.h>
#include <vector>

namespace esphome {
namespace lvgl {
static const char *const TAG = "lvgl";

static lv_color_t lv_color_from(Color color) { return lv_color_make(color.red, color.green, color.blue); }
#if LV_COLOR_DEPTH == 16
static const display::ColorBitness LV_BITNESS = display::ColorBitness::COLOR_BITNESS_565;
#elif LV_COLOR_DEPTH == 32
static const display::ColorBitness LV_BITNESS = display::ColorBitness::COLOR_BITNESS_888;
#else
static const display::ColorBitness LV_BITNESS = display::ColorBitness::COLOR_BITNESS_332;
#endif

// The ESPHome name munging does not work well with the lv_ types, and will produce variable names
// that are the same as the type.
// to work-around this these typedefs are used.
typedef lv_obj_t LvScreenType;
typedef lv_color_t LvColorType;
typedef lv_font_t LvFontType;
typedef lv_obj_t LvObjType;
typedef lv_style_t LvStyleType;
typedef lv_point_t LvPointType;
typedef lv_label_t LvLabelType;
typedef lv_meter_t LvMeterType;
typedef lv_meter_indicator_t LvMeterIndicatorType;
typedef lv_slider_t LvSliderType;
typedef lv_btn_t LvBtnType;
typedef lv_msgbox_t LvMsgBoxType;
typedef lv_line_t LvLineType;
typedef lv_img_t LvImgType;
typedef lv_animimg_t LvAnimImgType;
typedef lv_spinbox_t LvSpinBoxType;
typedef lv_arc_t LvArcType;
typedef lv_bar_t LvBarType;
typedef lv_theme_t LvThemeType;
typedef lv_checkbox_t LvCheckboxType;
typedef lv_canvas_t LvCanvasType;
typedef lv_dropdown_t LvDropdownType;
typedef lv_dropdown_list_t LvDropdownListType;
typedef lv_roller_t LvRollerType;
typedef lv_led_t LvLedType;
typedef lv_switch_t LvSwitchType;
typedef lv_table_t LvTableType;
typedef lv_textarea_t LvTextareaType;
typedef lv_obj_t LvBtnmBtn;

// Parent class for things that wrap an LVGL object
class LvCompound {
 public:
  virtual void set_obj(lv_obj_t *lv_obj) { this->obj = lv_obj; }
  lv_obj_t *obj{};
};

class LvBtnmatrixType : public key_provider::KeyProvider, public LvCompound {
 public:
  void set_obj(lv_obj_t *lv_obj) override {
    LvCompound::set_obj(lv_obj);
    lv_obj_add_event_cb(
        lv_obj,
        [](lv_event_t *event) {
          LvBtnmatrixType *self = (LvBtnmatrixType *) event->user_data;
          if (self->key_callback_.size() == 0)
            return;
          auto key_idx = lv_btnmatrix_get_selected_btn(self->obj);
          if (key_idx == LV_BTNMATRIX_BTN_NONE)
            return;
          if (self->key_map_.count(key_idx) != 0) {
            self->send_key_(self->key_map_[key_idx]);
            return;
          }
          auto str = lv_btnmatrix_get_btn_text(self->obj, key_idx);
          auto len = strlen(str);
          while (len--)
            self->send_key_(*str++);
        },
        LV_EVENT_PRESSED, this);
  }

  void set_key(size_t idx, uint8_t key) { this->key_map_[idx] = key; }

 protected:
  std::map<size_t, uint8_t> key_map_{};
};
// typedef lv_btnmatrix_t LvBtnmatrixType;

typedef struct {
  lv_obj_t *page;
  size_t index;
  bool skip;
} LvPageType;

typedef std::function<void(lv_obj_t *)> LvLambdaType;
typedef std::function<void(float)> set_value_lambda_t;
typedef void(event_callback_t)(_lv_event_t *);
typedef std::function<const char *(void)> text_lambda_t;

template<typename... Ts> class ObjUpdateAction : public Action<Ts...> {
 public:
  explicit ObjUpdateAction(std::function<void(Ts...)> &&lamb) : lamb_(std::move(lamb)) {}

  void play(Ts... x) override { this->lamb_(x...); }

 protected:
  std::function<void(Ts...)> lamb_;
};

#if LV_USE_FONT
class FontEngine {
 public:
  FontEngine(font::Font *esp_font) : font_(esp_font) {
    this->lv_font_.line_height = this->height_ = esp_font->get_height();
    this->lv_font_.base_line = this->baseline_ = this->lv_font_.line_height - esp_font->get_baseline();
    this->lv_font_.get_glyph_dsc = get_glyph_dsc_cb;
    this->lv_font_.get_glyph_bitmap = get_glyph_bitmap;
    this->lv_font_.dsc = this;
    this->lv_font_.subpx = LV_FONT_SUBPX_NONE;
    this->lv_font_.underline_position = -1;
    this->lv_font_.underline_thickness = 1;
    this->bpp_ = esp_font->get_bpp();
  }

  const lv_font_t *get_lv_font() { return &this->lv_font_; }

  static bool get_glyph_dsc_cb(const lv_font_t *font, lv_font_glyph_dsc_t *dsc, uint32_t unicode_letter,
                               uint32_t next) {
    FontEngine *fe = (FontEngine *) font->dsc;
    const font::GlyphData *gd = fe->get_glyph_data(unicode_letter);
    if (gd == nullptr)
      return false;
    dsc->adv_w = gd->offset_x + gd->width;
    dsc->ofs_x = gd->offset_x;
    dsc->ofs_y = fe->height_ - gd->height - gd->offset_y - fe->baseline_;
    dsc->box_w = gd->width;
    dsc->box_h = gd->height;
    dsc->is_placeholder = 0;
    dsc->bpp = fe->bpp_;
    return true;
  }

  static const uint8_t *get_glyph_bitmap(const lv_font_t *font, uint32_t unicode_letter) {
    FontEngine *fe = (FontEngine *) font->dsc;
    const font::GlyphData *gd = fe->get_glyph_data(unicode_letter);
    if (gd == nullptr)
      return nullptr;
    // esph_log_d(TAG, "Returning bitmap @  %X", (uint32_t)gd->data);

    return gd->data;
  }

 protected:
  font::Font *font_{};
  uint32_t last_letter_{};
  const font::GlyphData *last_data_{};
  lv_font_t lv_font_{};
  uint16_t baseline_{};
  uint16_t height_{};
  uint8_t bpp_{};

  const font::GlyphData *get_glyph_data(uint32_t unicode_letter) {
    if (unicode_letter == last_letter_)
      return this->last_data_;
    uint8_t unicode[5];
    memset(unicode, 0, sizeof unicode);
    if (unicode_letter > 0xFFFF) {
      unicode[0] = 0xF0 + ((unicode_letter >> 18) & 0x7);
      unicode[1] = 0x80 + ((unicode_letter >> 12) & 0x3F);
      unicode[2] = 0x80 + ((unicode_letter >> 6) & 0x3F);
      unicode[3] = 0x80 + (unicode_letter & 0x3F);
    } else if (unicode_letter > 0x7FF) {
      unicode[0] = 0xE0 + ((unicode_letter >> 12) & 0xF);
      unicode[1] = 0x80 + ((unicode_letter >> 6) & 0x3F);
      unicode[2] = 0x80 + (unicode_letter & 0x3F);
    } else if (unicode_letter > 0x7F) {
      unicode[0] = 0xC0 + ((unicode_letter >> 6) & 0x1F);
      unicode[1] = 0x80 + (unicode_letter & 0x3F);
    } else {
      unicode[0] = unicode_letter;
    }
    int match_length;
    int glyph_n = this->font_->match_next_glyph(unicode, &match_length);
    if (glyph_n < 0)
      return nullptr;
    this->last_data_ = this->font_->get_glyphs()[glyph_n].get_glyph_data();
    this->last_letter_ = unicode_letter;
    return this->last_data_;
  }
};
#endif  // LV_USE_FONT

#if LVGL_USES_IMAGE
static lv_img_dsc_t *lv_img_from(image::Image *src) {
  auto img = new lv_img_dsc_t();  // NOLINT
  img->header.always_zero = 0;
  img->header.reserved = 0;
  img->header.w = src->get_width();
  img->header.h = src->get_height();
  img->data = src->get_data_start();
  img->data_size = image::image_type_to_width_stride(img->header.w * img->header.h, src->get_type());
  switch (src->get_type()) {
    case image::IMAGE_TYPE_BINARY:
      img->header.cf = LV_IMG_CF_ALPHA_1BIT;
      break;

    case image::IMAGE_TYPE_GRAYSCALE:
      img->header.cf = LV_IMG_CF_ALPHA_8BIT;
      break;

    case image::IMAGE_TYPE_RGB24:
      img->header.cf = LV_IMG_CF_RGB888;
      break;

    case image::IMAGE_TYPE_RGB565:
#if LV_COLOR_DEPTH == 16
      img->header.cf = src->has_transparency() ? LV_IMG_CF_TRUE_COLOR_CHROMA_KEYED : LV_IMG_CF_TRUE_COLOR;
#else
      img->header.cf = LV_IMG_CF_RGB565;
#endif
      break;

    case image::IMAGE_TYPE_RGBA:
#if LV_COLOR_DEPTH == 32
      img->header.cf = LV_IMG_CF_TRUE_COLOR;
#else
      img->header.cf = LV_IMG_CF_RGBA8888;
#endif
      break;
  }
  return img;
}
#endif

class LvglComponent : public PollingComponent {
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

  void setup() override {
    esph_log_config(TAG, "LVGL Setup starts");
#if LV_USE_LOG
    lv_log_register_print_cb(log_cb);
#endif
    auto display = this->displays_[0];
    size_t buffer_pixels = display->get_width() * display->get_height() / this->buffer_frac_;
    auto buf_bytes = buffer_pixels * LV_COLOR_DEPTH / 8;
    auto buf = lv_custom_mem_alloc(buf_bytes);
    if (buf == nullptr) {
      esph_log_e(TAG, "Malloc failed to allocate %zu bytes", buf_bytes);
      this->mark_failed();
      return;
    }
    lv_disp_draw_buf_init(&this->draw_buf_, buf, nullptr, buffer_pixels);
    lv_disp_drv_init(&this->disp_drv_);
    this->disp_drv_.hor_res = display->get_width();
    this->disp_drv_.ver_res = display->get_height();
    this->disp_drv_.draw_buf = &this->draw_buf_;
    this->disp_drv_.user_data = this;
    this->disp_drv_.full_refresh = this->full_refresh_;
    this->disp_drv_.flush_cb = static_flush_cb;
    this->disp_drv_.rounder_cb = rounder_cb;
    switch (display->get_rotation()) {
      case display::DISPLAY_ROTATION_0_DEGREES:
        break;
      case display::DISPLAY_ROTATION_90_DEGREES:
        this->disp_drv_.sw_rotate = true;
        this->disp_drv_.rotated = LV_DISP_ROT_90;
        break;
      case display::DISPLAY_ROTATION_180_DEGREES:
        this->disp_drv_.sw_rotate = true;
        this->disp_drv_.rotated = LV_DISP_ROT_180;
        break;
      case display::DISPLAY_ROTATION_270_DEGREES:
        this->disp_drv_.sw_rotate = true;
        this->disp_drv_.rotated = LV_DISP_ROT_270;
        break;
    }
    esph_log_d(TAG, "sw_rotate = %d, rotated=%d", this->disp_drv_.sw_rotate, this->disp_drv_.rotated);
    this->disp_ = lv_disp_drv_register(&this->disp_drv_);
    this->custom_change_event_ = (lv_event_code_t) lv_event_register_id();
    for (auto v : this->init_lambdas_)
      v(this->disp_);
    if (!this->pages_.empty()) {
      auto page = this->pages_[0];
      esph_log_d(TAG, "loading page: size = %zu, index %zu, ptr %p", this->pages_.size(), page->index, page->page);
      if (page->page != nullptr)
        lv_scr_load(this->pages_[0]->page);
    }
    // this->display_->set_writer([](display::Display &d) { lv_timer_handler(); });
    lv_disp_trig_activity(this->disp_);
    esph_log_config(TAG, "LVGL Setup complete");
  }

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
        this->write_random();
      return;
    }
    lv_timer_handler_run_in_period(5);
  }

  void add_on_idle_callback(std::function<void(uint32_t)> &&callback) {
    this->idle_callbacks_.add(std::move(callback));
  }

  void add_display(display::Display *display) { this->displays_.push_back(display); }
  void add_init_lambda(std::function<void(lv_disp_t *)> lamb) { this->init_lambdas_.push_back(lamb); }
  void dump_config() override { esph_log_config(TAG, "LVGL:"); }
  lv_event_code_t get_custom_change_event() { return this->custom_change_event_; }
  void set_full_refresh(bool full_refresh) { this->full_refresh_ = full_refresh; }
  void set_paused(bool paused, bool show_snow) {
    this->paused_ = paused;
    this->show_snow_ = show_snow;
    this->snow_line_ = 0;
    if (!paused) {
      lv_disp_trig_activity(this->disp_);  // resets the inactivity time
      lv_obj_invalidate(lv_scr_act());
    }
  }
  bool is_paused() { return this->paused_; }
  void set_page_wrap(bool page_wrap) { this->page_wrap_ = page_wrap; }
  bool is_idle(uint32_t idle_ms) { return lv_disp_get_inactive_time(this->disp_) > idle_ms; }
  void set_buffer_frac(size_t frac) { this->buffer_frac_ = frac; }
  void add_page(LvPageType *page) { this->pages_.push_back(page); }
  void show_page(size_t index, lv_scr_load_anim_t anim, uint32_t time) {
    if (index >= this->pages_.size())
      return;
    this->page_index_ = index;
    lv_scr_load_anim(this->pages_[index]->page, anim, time, 0, false);
  }
  void show_next_page(bool reverse, lv_scr_load_anim_t anim, uint32_t time) {
    if (this->pages_.empty())
      return;
    int next = this->page_index_;
    do {
      if (reverse) {
        if (next == 0) {
          if (!this->page_wrap_)
            return;
          next = this->pages_.size();
        }
        next--;
      } else {
        if (++next == this->pages_.size()) {
          if (!this->page_wrap_)
            return;
          next = 0;
        }
      }
    } while (this->pages_[next]->skip && next != this->page_index_);
    this->show_page(next, anim, time);
  }

 protected:
  void write_random() {
    // length of 2 lines in 32 bit units
    // we write 2 lines for the benefit of displays that won't write one line at a time.
    size_t line_len = this->disp_drv_.hor_res * LV_COLOR_DEPTH / 8 / 4 * 2;
    for (size_t i = 0; i != line_len; i++) {
      ((uint32_t *) (this->draw_buf_.buf1))[i] = random_uint32();
    }
    lv_area_t area;
    area.x1 = 0;
    area.x2 = this->disp_drv_.hor_res - 1;
    if (this->snow_line_ == this->disp_drv_.ver_res / 2) {
      area.y1 = random_uint32() % (this->disp_drv_.ver_res / 2) * 2;
    } else {
      area.y1 = this->snow_line_++ * 2;
    }
    // write 2 lines
    area.y2 = area.y1 + 1;
    this->draw_buffer_(&area, (const uint8_t *) this->draw_buf_.buf1);
  }

  void draw_buffer_(const lv_area_t *area, const uint8_t *ptr) {
    for (auto display : this->displays_) {
      display->draw_pixels_at(area->x1, area->y1, lv_area_get_width(area), lv_area_get_height(area), ptr,
                              display::COLOR_ORDER_RGB, LV_BITNESS, LV_COLOR_16_SWAP);
    }
  }

  void flush_cb_(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    auto now = millis();
    this->draw_buffer_(area, (const uint8_t *) color_p);
    lv_disp_flush_ready(disp_drv);
    esph_log_v(TAG, "flush_cb, area=%d/%d, %d/%d took %dms", area->x1, area->y1, lv_area_get_width(area),
               lv_area_get_height(area), (int) (millis() - now));
  }

  std::vector<display::Display *> displays_{};
  lv_disp_draw_buf_t draw_buf_{};
  lv_disp_drv_t disp_drv_{};
  lv_disp_t *disp_{};
  lv_event_code_t custom_change_event_{};

  CallbackManager<void(uint32_t)> idle_callbacks_{};
  std::vector<std::function<void(lv_disp_t *)>> init_lambdas_;
  std::vector<LvPageType *> pages_{};
  bool page_wrap_{true};
  size_t page_index_{0};
  size_t buffer_frac_{1};
  bool paused_{};
  bool show_snow_{};
  uint32_t snow_line_{};
  bool full_refresh_{};
};

class IdleTrigger : public Trigger<> {
 public:
  explicit IdleTrigger(LvglComponent *parent, TemplatableValue<uint32_t> timeout) : timeout_(timeout) {
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
  void play(Ts... x) override { this->action_(this->parent_); }

  void set_action(std::function<void(LvglComponent *)> action) { this->action_ = action; }

 protected:
  std::function<void(LvglComponent *)> action_{};
};

template<typename... Ts> class LvglCondition : public Condition<Ts...>, public Parented<LvglComponent> {
 public:
  bool check(Ts... x) override { return this->condition_lambda_(this->parent_); }
  void set_condition_lambda(std::function<bool(LvglComponent *)> condition_lambda) {
    this->condition_lambda_ = condition_lambda;
  }

 protected:
  std::function<bool(LvglComponent *)> condition_lambda_{};
};

#if LV_USE_TOUCHSCREEN
class LVTouchListener : public touchscreen::TouchListener, public Parented<LvglComponent> {
 public:
  LVTouchListener(uint32_t long_press_time, uint32_t long_press_repeat_time) {
    lv_indev_drv_init(&this->drv);
    this->drv.long_press_repeat_time = long_press_repeat_time;
    this->drv.long_press_time = long_press_time;
    this->drv.type = LV_INDEV_TYPE_POINTER;
    this->drv.user_data = this;
    this->drv.read_cb = [](lv_indev_drv_t *d, lv_indev_data_t *data) {
      LVTouchListener *l = (LVTouchListener *) d->user_data;
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
  void touch_cb(lv_indev_data_t *data) {}

  lv_indev_drv_t drv{};

 protected:
  touchscreen::TouchPoint touch_point_{};
  bool touch_pressed_{};
};
#endif

#if LV_USE_ROTARY_ENCODER
class LVRotaryEncoderListener : public Parented<LvglComponent> {
 public:
  LVRotaryEncoderListener(uint16_t lpt, uint16_t lprt) {
    lv_indev_drv_init(&this->drv);
    this->drv.type = LV_INDEV_TYPE_ENCODER;
    this->drv.user_data = this;
    this->drv.long_press_time = lpt;
    this->drv.long_press_repeat_time = lprt;
    this->drv.read_cb = [](lv_indev_drv_t *d, lv_indev_data_t *data) {
      LVRotaryEncoderListener *l = (LVRotaryEncoderListener *) d->user_data;
      data->state = l->pressed_ ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
      data->continue_reading = false;
      data->enc_diff = l->count_ - l->last_count_;
      l->last_count_ = l->count_;
    };
  }

  void set_count(int32_t count) { this->count_ = count; }
  void set_pressed(bool pressed) { this->pressed_ = pressed && !this->parent_->is_paused(); }
  lv_indev_drv_t drv{};

 protected:
  bool pressed_{};
  int32_t count_{};
  int32_t last_count_{};
  binary_sensor::BinarySensor *binary_sensor_{};
  sensor::Sensor *sensor_{};
};
#endif

}  // namespace lvgl
}  // namespace esphome
