#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "lvgl_esphome.h"

#include "core/lv_global.h"
#include "core/lv_obj_class_private.h"

#include <numeric>

static void *lv_alloc_draw_buf(size_t size, bool internal);
static void *draw_buf_alloc_cb(size_t size, lv_color_format_t color_format) { return lv_alloc_draw_buf(size, false); };

namespace esphome::lvgl {
static const char *const TAG = "lvgl";

static const size_t MIN_BUFFER_FRAC = 8;     // buffer must be at least 1/8 of the display size
static const size_t MIN_BUFFER_SIZE = 2048;  // Sensible minimum buffer size

static const char *const EVENT_NAMES[] = {
    "NONE",
    "PRESSED",
    "PRESSING",
    "PRESS_LOST",
    "SHORT_CLICKED",
    "LONG_PRESSED",
    "LONG_PRESSED_REPEAT",
    "CLICKED",
    "RELEASED",
    "SCROLL_BEGIN",
    "SCROLL_END",
    "SCROLL",
    "GESTURE",
    "KEY",
    "FOCUSED",
    "DEFOCUSED",
    "LEAVE",
    "HIT_TEST",
    "COVER_CHECK",
    "REFR_EXT_DRAW_SIZE",
    "DRAW_MAIN_BEGIN",
    "DRAW_MAIN",
    "DRAW_MAIN_END",
    "DRAW_POST_BEGIN",
    "DRAW_POST",
    "DRAW_POST_END",
    "DRAW_PART_BEGIN",
    "DRAW_PART_END",
    "VALUE_CHANGED",
    "INSERT",
    "REFRESH",
    "READY",
    "CANCEL",
    "DELETE",
    "CHILD_CHANGED",
    "CHILD_CREATED",
    "CHILD_DELETED",
    "SCREEN_UNLOAD_START",
    "SCREEN_LOAD_START",
    "SCREEN_LOADED",
    "SCREEN_UNLOADED",
    "SIZE_CHANGED",
    "STYLE_CHANGED",
    "LAYOUT_CHANGED",
    "GET_SELF_SIZE",
};

static const unsigned LOG_LEVEL_MAP[] = {
    ESPHOME_LOG_LEVEL_DEBUG, ESPHOME_LOG_LEVEL_INFO,  ESPHOME_LOG_LEVEL_WARN,
    ESPHOME_LOG_LEVEL_ERROR, ESPHOME_LOG_LEVEL_ERROR, ESPHOME_LOG_LEVEL_NONE,

};

std::string lv_event_code_name_for(lv_event_t *event) {
  auto event_code = lv_event_get_code(event);
  if (event_code < sizeof(EVENT_NAMES) / sizeof(EVENT_NAMES[0])) {
    return EVENT_NAMES[event_code];
  }
  // max 4 bytes: "%u" with uint8_t (max 255, 3 digits) + null
  char buf[4];
  snprintf(buf, sizeof(buf), "%u", event_code);
  return buf;
}

void LvglComponent::set_rotation(display::DisplayRotation rotation) {
  if (this->rotation_type_ == RotationType::ROTATION_UNUSED) {
    ESP_LOGW(TAG, "Display rotation cannot be changed unless rotation was enabled during setup.");
    return;
  }
  this->rotation_ = rotation;
  if (this->is_ready()) {
    this->set_resolution_();
    lv_obj_update_layout(this->get_screen_active());
    lv_obj_invalidate(this->get_screen_active());
  }
}

void LvglComponent::rotate_coordinates(int32_t &x, int32_t &y) const {
  switch (this->rotation_) {
    default:
      break;

    case display::DISPLAY_ROTATION_180_DEGREES: {
      x = this->width_ - x - 1;
      y = this->height_ - y - 1;
      break;
    }
    case display::DISPLAY_ROTATION_270_DEGREES: {
      auto tmp = x;
      x = this->height_ - y - 1;
      y = tmp;
      break;
    }
    case display::DISPLAY_ROTATION_90_DEGREES: {
      auto tmp = y;
      y = this->width_ - x - 1;
      x = tmp;
      break;
    }
  }
}

static void rounder_cb(lv_event_t *event) {
  auto *comp = static_cast<LvglComponent *>(lv_event_get_user_data(event));
  auto *area = static_cast<lv_area_t *>(lv_event_get_param(event));
  // cater for display driver chips with special requirements for bounds of partial
  // draw areas. Extend the draw area to satisfy:
  // * Coordinates must be a multiple of draw_rounding
  auto draw_rounding = comp->draw_rounding;
  // round down the start coordinates
  area->x1 = area->x1 / draw_rounding * draw_rounding;
  area->y1 = area->y1 / draw_rounding * draw_rounding;
  // round up the end coordinates
  area->x2 = (area->x2 + draw_rounding) / draw_rounding * draw_rounding - 1;
  area->y2 = (area->y2 + draw_rounding) / draw_rounding * draw_rounding - 1;
}

void LvglComponent::render_end_cb(lv_event_t *event) {
  auto *comp = static_cast<LvglComponent *>(lv_event_get_user_data(event));
  comp->draw_end_();
}

void LvglComponent::render_start_cb(lv_event_t *event) {
  ESP_LOGVV(TAG, "Draw start");
  auto *comp = static_cast<LvglComponent *>(lv_event_get_user_data(event));
  comp->draw_start_();
}

lv_event_code_t lv_update_event;  // NOLINT
void LvglComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "LVGL:\n"
                "  Display width/height: %d x %d\n"
                "  Buffer size: %zu%%\n"
                "  Rotation: %d\n"
                "  Draw rounding: %d",
                this->width_, this->height_, 100 / this->buffer_frac_, this->rotation_, (int) this->draw_rounding);
  if (this->rotation_type_ != ROTATION_UNUSED) {
    const char *rot_type = "hardware via display driver";
    if (this->rotation_type_ == RotationType::ROTATION_SOFTWARE) {
#ifdef USE_ESP32_VARIANT_ESP32P4
      rot_type = this->ppa_client_ != nullptr ? "software (PPA accelerated)" : "software";
#else
      rot_type = "software";
#endif
    }
    ESP_LOGCONFIG(TAG, "  Rotation type: %s", rot_type);
  }
}

void LvglComponent::set_paused(bool paused, bool show_snow) {
  this->paused_ = paused;
  this->show_snow_ = show_snow;
  if (!paused && lv_screen_active() != nullptr) {
    lv_display_trigger_activity(this->disp_);  // resets the inactivity time
    lv_obj_invalidate(lv_screen_active());
  }
  if (paused && this->pause_callback_ != nullptr)
    this->pause_callback_->trigger();
  if (!paused && this->resume_callback_ != nullptr)
    this->resume_callback_->trigger();
}

void LvglComponent::esphome_lvgl_init() {
  lv_init();
  // override draw buf alloc to ensure proper alignment for PPA
  LV_GLOBAL_DEFAULT()->draw_buf_handlers.buf_malloc_cb = draw_buf_alloc_cb;
  LV_GLOBAL_DEFAULT()->draw_buf_handlers.buf_free_cb = lv_free_core;
  LV_GLOBAL_DEFAULT()->image_cache_draw_buf_handlers.buf_malloc_cb = draw_buf_alloc_cb;
  LV_GLOBAL_DEFAULT()->image_cache_draw_buf_handlers.buf_free_cb = lv_free_core;
  LV_GLOBAL_DEFAULT()->font_draw_buf_handlers.buf_malloc_cb = draw_buf_alloc_cb;
  LV_GLOBAL_DEFAULT()->font_draw_buf_handlers.buf_free_cb = lv_free_core;
  lv_tick_set_cb([] { return millis(); });
  lv_update_event = static_cast<lv_event_code_t>(lv_event_register_id());
}

void LvglComponent::add_event_cb(lv_obj_t *obj, event_callback_t callback, lv_event_code_t event) {
  lv_obj_add_event_cb(obj, callback, event, nullptr);
}

void LvglComponent::add_event_cb(lv_obj_t *obj, event_callback_t callback, lv_event_code_t event1,
                                 lv_event_code_t event2) {
  add_event_cb(obj, callback, event1);
  add_event_cb(obj, callback, event2);
}

void LvglComponent::add_event_cb(lv_obj_t *obj, event_callback_t callback, lv_event_code_t event1,
                                 lv_event_code_t event2, lv_event_code_t event3) {
  add_event_cb(obj, callback, event1);
  add_event_cb(obj, callback, event2);
  add_event_cb(obj, callback, event3);
}

void LvglComponent::add_page(LvPageType *page) {
  this->pages_.push_back(page);
  page->set_parent(this);
  lv_display_set_default(this->disp_);
  page->setup(this->pages_.size() - 1);
}

void LvglComponent::show_page(size_t index, lv_screen_load_anim_t anim, uint32_t time) {
  if (index >= this->pages_.size())
    return;
  this->current_page_ = index;
  if (anim == LV_SCREEN_LOAD_ANIM_NONE) {
    lv_screen_load(this->pages_[this->current_page_]->obj);
  } else {
    lv_screen_load_anim(this->pages_[this->current_page_]->obj, anim, time, 0, false);
  }
}

void LvglComponent::show_next_page(lv_screen_load_anim_t anim, uint32_t time) {
  if (this->pages_.empty() || (this->current_page_ == this->pages_.size() - 1 && !this->page_wrap_))
    return;
  size_t start = this->current_page_;
  do {
    this->current_page_ = (this->current_page_ + 1) % this->pages_.size();
    if (this->current_page_ == start)
      return;  // all pages have skip=true (guaranteed not to happen by YAML validation)
  } while (this->pages_[this->current_page_]->skip);  // skip empty pages()
  this->show_page(this->current_page_, anim, time);
}

void LvglComponent::show_prev_page(lv_screen_load_anim_t anim, uint32_t time) {
  if (this->pages_.empty() || (this->current_page_ == 0 && !this->page_wrap_))
    return;
  size_t start = this->current_page_;
  do {
    this->current_page_ = (this->current_page_ + this->pages_.size() - 1) % this->pages_.size();
    if (this->current_page_ == start)
      return;  // all pages have skip=true (guaranteed not to happen by YAML validation)
  } while (this->pages_[this->current_page_]->skip);  // skip empty pages()
  this->show_page(this->current_page_, anim, time);
}

size_t LvglComponent::get_current_page() const { return this->current_page_; }
bool LvPageType::is_showing() const { return this->parent_->get_current_page() == this->index; }

#ifdef USE_ESP32_VARIANT_ESP32P4
bool LvglComponent::ppa_rotate_(const lv_color_data *src, lv_color_data *dst, uint16_t width, uint16_t height,
                                uint32_t height_rounded) {
  ppa_srm_rotation_angle_t angle;
  uint16_t out_w, out_h;

  // Map ESPHome clockwise display rotation to PPA counter-clockwise angles
  switch (this->rotation_) {
    case display::DISPLAY_ROTATION_90_DEGREES:
      angle = PPA_SRM_ROTATION_ANGLE_270;  // 270° CCW = 90° CW
      out_w = height_rounded;
      out_h = width;
      break;
    case display::DISPLAY_ROTATION_180_DEGREES:
      angle = PPA_SRM_ROTATION_ANGLE_180;
      out_w = width;
      out_h = height;
      break;
    case display::DISPLAY_ROTATION_270_DEGREES:
      angle = PPA_SRM_ROTATION_ANGLE_90;  // 90° CCW = 270° CW
      out_w = height_rounded;
      out_h = width;
      break;
    default:
      return false;  // No rotation needed
  }

  // Align buffer size to cache line (LV_DRAW_BUF_ALIGN) as required by PPA DMA
  // the underlying buffer will be large enough as the size is also padded when allocating.
  size_t out_buf_size = out_w * out_h * sizeof(lv_color_data);
  out_buf_size = LV_ROUND_UP(out_buf_size, LV_DRAW_BUF_ALIGN);

  ppa_srm_oper_config_t srm_config{};
  srm_config.in.buffer = src;
  srm_config.in.pic_w = width;
  srm_config.in.pic_h = height;
  srm_config.in.block_w = width;
  srm_config.in.block_h = height;
#if LV_COLOR_DEPTH == 16
  srm_config.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
#elif LV_COLOR_DEPTH == 32
  srm_config.in.srm_cm = PPA_SRM_COLOR_MODE_ARGB8888;
#endif
  srm_config.out.buffer = dst;
  srm_config.out.buffer_size = out_buf_size;
  srm_config.out.pic_w = out_w;
  srm_config.out.pic_h = out_h;
#if LV_COLOR_DEPTH == 16
  srm_config.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
#elif LV_COLOR_DEPTH == 32
  srm_config.out.srm_cm = PPA_SRM_COLOR_MODE_ARGB8888;
#endif
  srm_config.rotation_angle = angle;
  srm_config.scale_x = 1.0f;
  srm_config.scale_y = 1.0f;
  srm_config.mode = PPA_TRANS_MODE_BLOCKING;

  esp_err_t ret = ppa_do_scale_rotate_mirror(this->ppa_client_, &srm_config);
  if (ret != ESP_OK) {
    ESP_LOGW(TAG, "PPA rotation failed: %s", esp_err_to_name(ret));
    ESP_LOGW(TAG, "PPA SRM: in=%ux%u src=%p, out=%ux%u dst=%p size=%zu, angle=%d", width, height, src, out_w, out_h,
             dst, out_buf_size, (int) angle);
    return false;
  }
  return true;
}
#endif  // USE_ESP32_VARIANT_ESP32P4

void LvglComponent::draw_buffer_(const lv_area_t *area, lv_color_data *ptr) {
  auto width = lv_area_get_width(area);
  auto height = lv_area_get_height(area);
  auto height_rounded = (height + this->draw_rounding - 1) / this->draw_rounding * this->draw_rounding;
  auto x1 = area->x1;
  auto y1 = area->y1;
  if (this->rotation_type_ == ROTATION_SOFTWARE) {
    lv_color_data *dst = reinterpret_cast<lv_color_data *>(this->rotate_buf_);
#ifdef USE_ESP32_VARIANT_ESP32P4
    bool ppa_done = this->ppa_client_ != nullptr && this->ppa_rotate_(ptr, dst, width, height, height_rounded);
    if (!ppa_done)
#endif
    {
      switch (this->rotation_) {
        case display::DISPLAY_ROTATION_90_DEGREES:
          for (lv_coord_t x = height; x-- != 0;) {
            for (lv_coord_t y = 0; y != width; y++) {
              dst[y * height_rounded + x] = *ptr++;
            }
          }
          break;

        case display::DISPLAY_ROTATION_180_DEGREES:
          for (lv_coord_t y = height; y-- != 0;) {
            for (lv_coord_t x = width; x-- != 0;) {
              dst[y * width + x] = *ptr++;
            }
          }
          break;

        case display::DISPLAY_ROTATION_270_DEGREES:
          for (lv_coord_t x = 0; x != height; x++) {
            for (lv_coord_t y = width; y-- != 0;) {
              dst[y * height_rounded + x] = *ptr++;
            }
          }
          break;

        default:
          dst = ptr;
          break;
      }
    }
    // Coordinate adjustments apply regardless of PPA or SW rotation
    switch (this->rotation_) {
      case display::DISPLAY_ROTATION_90_DEGREES:
        y1 = x1;
        x1 = this->width_ - area->y1 - height;
        height = width;
        width = height_rounded;
        break;

      case display::DISPLAY_ROTATION_180_DEGREES:
        x1 = this->width_ - x1 - width;
        y1 = this->height_ - y1 - height;
        break;

      case display::DISPLAY_ROTATION_270_DEGREES:
        x1 = y1;
        y1 = this->height_ - area->x1 - width;
        height = width;
        width = height_rounded;
        break;

      default:
        break;
    }
    ptr = dst;
  }
  for (auto *display : this->displays_) {
    display->draw_pixels_at(x1, y1, width, height, (const uint8_t *) ptr, display::COLOR_ORDER_RGB, LV_BITNESS,
                            this->big_endian_);
  }
}

void LvglComponent::flush_cb_(lv_display_t *disp_drv, const lv_area_t *area, uint8_t *color_p) {
  if (!this->is_paused()) {
    auto now = millis();
    this->draw_buffer_(area, reinterpret_cast<lv_color_data *>(color_p));
    ESP_LOGV(TAG, "flush_cb, area=%d/%d, %d/%d took %dms", (int) area->x1, (int) area->y1,
             (int) lv_area_get_width(area), (int) lv_area_get_height(area), (int) (millis() - now));
  }
  lv_display_flush_ready(disp_drv);
}

IdleTrigger::IdleTrigger(LvglComponent *parent, TemplatableFn<uint32_t> timeout) : timeout_(timeout) {
  parent->add_on_idle_callback([this](uint32_t idle_time) {
    if (!this->is_idle_ && idle_time > this->timeout_.value()) {
      this->is_idle_ = true;
      this->trigger();
    } else if (this->is_idle_ && idle_time < this->timeout_.value()) {
      this->is_idle_ = false;
    }
  });
}

#ifdef USE_LVGL_TOUCHSCREEN
LVTouchListener::LVTouchListener(uint16_t long_press_time, uint16_t long_press_repeat_time, LvglComponent *parent) {
  this->set_parent(parent);
  this->drv_ = lv_indev_create();
  lv_indev_set_type(this->drv_, LV_INDEV_TYPE_POINTER);
  lv_indev_set_disp(this->drv_, parent->get_disp());
  lv_indev_set_long_press_time(this->drv_, long_press_time);
  // long press repeat time TBD
  lv_indev_set_user_data(this->drv_, this);
  lv_indev_set_read_cb(this->drv_, [](lv_indev_t *d, lv_indev_data_t *data) {
    auto *l = static_cast<LVTouchListener *>(lv_indev_get_user_data(d));
    if (l->touch_pressed_) {
      data->point.x = l->touch_point_.x;
      data->point.y = l->touch_point_.y;
      l->parent_->rotate_coordinates(data->point.x, data->point.y);
      data->state = LV_INDEV_STATE_PRESSED;
    } else {
      data->state = LV_INDEV_STATE_RELEASED;
    }
  });
}

void LVTouchListener::update(const touchscreen::TouchPoints_t &tpoints) {
  this->touch_pressed_ = !this->parent_->is_paused() && !tpoints.empty();
  if (this->touch_pressed_)
    this->touch_point_ = tpoints[0];
}
#endif  // USE_LVGL_TOUCHSCREEN

#ifdef USE_LVGL_METER

int16_t lv_get_needle_angle_for_value(lv_obj_t *obj, int32_t value) {
  auto *scale = lv_obj_get_parent(obj);
  auto min_value = lv_scale_get_range_min_value(scale);
  auto max_value = lv_scale_get_range_max_value(scale);
  value = clamp(value, min_value, max_value);
  return ((value - min_value) * lv_scale_get_angle_range(scale) / (max_value - min_value) +
          lv_scale_get_rotation((scale))) %
         360;
}

void IndicatorLine::set_obj(lv_obj_t *lv_obj) {
  LvCompound::set_obj(lv_obj);
  lv_line_set_points(lv_obj, this->points_, 2);
  lv_obj_add_event_cb(
      lv_obj_get_parent(obj),
      [](lv_event_t *e) {
        auto *indicator = static_cast<IndicatorLine *>(lv_event_get_user_data(e));
        indicator->update_length_();
        ESP_LOGV(TAG, "Updated length, value = %d", indicator->angle_);
      },
      LV_EVENT_SIZE_CHANGED, this);
}

void IndicatorLine::set_value(int value) {
  auto angle = lv_get_needle_angle_for_value(this->obj, value);
  if (angle != this->angle_) {
    this->angle_ = angle;
    this->update_length_();
  }
}

void IndicatorLine::update_length_() {
  auto cx = lv_obj_get_width(lv_obj_get_parent(this->obj)) / 2;
  auto cy = lv_obj_get_height(lv_obj_get_parent(this->obj)) / 2;
  auto radius = clamp_at_most(cx, cy);
  auto length = lv_obj_get_style_length(this->obj, LV_PART_MAIN);
  auto radial_offset = lv_obj_get_style_radial_offset(this->obj, LV_PART_MAIN);
  if (LV_COORD_IS_PCT(radial_offset)) {
    radial_offset = radius * LV_COORD_GET_PCT(radial_offset) / 100;
  }
  if (LV_COORD_IS_PCT(length)) {
    length = radius * LV_COORD_GET_PCT(length) / 100;
  } else if (length < 0) {
    length += radius;
  }
  auto x = lv_trigo_cos(this->angle_) / 32768.0f;
  auto y = lv_trigo_sin(this->angle_) / 32768.0f;
  // radius here also represents the offset of the scale center from top left
  this->points_[0].x = radius + radial_offset * x;
  this->points_[0].y = radius + radial_offset * y;
  this->points_[1].x = radius + x * (radial_offset + length);
  this->points_[1].y = radius + y * (radial_offset + length);
  lv_obj_refresh_self_size(this->obj);
  lv_obj_invalidate(this->obj);
}
#endif

#ifdef USE_LVGL_KEY_LISTENER
LVEncoderListener::LVEncoderListener(lv_indev_type_t type, uint16_t long_press_time, uint16_t long_press_repeat_time) {
  this->drv_ = lv_indev_create();
  lv_indev_set_type(this->drv_, type);
  lv_indev_set_long_press_time(this->drv_, long_press_time);
  lv_indev_set_long_press_repeat_time(this->drv_, long_press_repeat_time);
  lv_indev_set_user_data(this->drv_, this);
  lv_indev_set_read_cb(this->drv_, [](lv_indev_t *d, lv_indev_data_t *data) {
    auto *l = static_cast<LVEncoderListener *>(lv_indev_get_user_data(d));
    data->state = l->pressed_ ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->key = l->key_;
    data->enc_diff = (int16_t) (l->count_ - l->last_count_);
    l->last_count_ = l->count_;
    data->continue_reading = false;
  });
}
#endif  // USE_LVGL_KEY_LISTENER

#if defined(USE_LVGL_DROPDOWN) || defined(LV_USE_ROLLER)
std::string LvSelectable::get_selected_text() {
  auto selected = this->get_selected_index();
  if (selected >= this->options_.size())
    return "";
  return this->options_[selected];
}

static std::string join_string(std::vector<std::string> options) {
  return std::accumulate(
      options.begin(), options.end(), std::string(),
      [](const std::string &a, const std::string &b) -> std::string { return a + (!a.empty() ? "\n" : "") + b; });
}

void LvSelectable::set_selected_text(const std::string &text, lv_anim_enable_t anim) {
  auto index = std::find(this->options_.begin(), this->options_.end(), text);
  if (index != this->options_.end()) {
    this->set_selected_index(index - this->options_.begin(), anim);
    lv_obj_send_event(this->obj, lv_update_event, nullptr);
  }
}

void LvSelectable::set_options(std::vector<std::string> options) {
  auto index = this->get_selected_index();
  if (index >= options.size())
    index = options.size() - 1;
  this->options_ = std::move(options);
  this->set_option_string(join_string(this->options_).c_str());
  lv_obj_send_event(this->obj, LV_EVENT_REFRESH, nullptr);
  this->set_selected_index(index, LV_ANIM_OFF);
}
#endif  // USE_LVGL_DROPDOWN || LV_USE_ROLLER

#ifdef USE_LVGL_BUTTONMATRIX
void LvButtonMatrixType::set_obj(lv_obj_t *lv_obj) {
  LvCompound::set_obj(lv_obj);
  lv_obj_add_event_cb(
      lv_obj,
      [](lv_event_t *event) {
        auto *self = static_cast<LvButtonMatrixType *>(lv_event_get_user_data(event));
        if (self->key_callback_.size() == 0)
          return;
        auto key_idx = lv_buttonmatrix_get_selected_button(self->obj);
        if (key_idx == LV_BUTTONMATRIX_BUTTON_NONE)
          return;
        if (self->key_map_.count(key_idx) != 0) {
          self->send_key_(self->key_map_[key_idx]);
          return;
        }
        const auto *str = lv_buttonmatrix_get_button_text(self->obj, key_idx);
        auto len = strlen(str);
        while (len--)
          self->send_key_(*str++);
      },
      LV_EVENT_PRESSED, this);
}
#endif  // USE_LVGL_BUTTONMATRIX

#ifdef USE_LVGL_KEYBOARD
static const char *const KB_SPECIAL_KEYS[] = {
    "abc", "ABC", "1#",
    // maybe add other special keys here
};

void LvKeyboardType::set_obj(lv_obj_t *lv_obj) {
  LvCompound::set_obj(lv_obj);
  lv_obj_add_event_cb(
      lv_obj,
      [](lv_event_t *event) {
        auto *self = static_cast<LvKeyboardType *>(lv_event_get_user_data(event));
        if (self->key_callback_.size() == 0)
          return;

        auto key_idx = lv_buttonmatrix_get_selected_button(self->obj);
        if (key_idx == LV_BUTTONMATRIX_BUTTON_NONE)
          return;
        const char *txt = lv_buttonmatrix_get_button_text(self->obj, key_idx);
        if (txt == nullptr)
          return;
        for (const auto *kb_special_key : KB_SPECIAL_KEYS) {
          if (strcmp(txt, kb_special_key) == 0)
            return;
        }
        while (*txt != 0)
          self->send_key_(*txt++);
      },
      LV_EVENT_PRESSED, this);
}
#endif  // USE_LVGL_KEYBOARD

void LvglComponent::draw_end_() {
  if (this->draw_end_callback_ != nullptr)
    this->draw_end_callback_->trigger();
  if (this->update_when_display_idle_) {
    for (auto *disp : this->displays_)
      disp->update();
  }
}

bool LvglComponent::is_paused() const {
  if (this->paused_)
    return true;
  if (this->update_when_display_idle_) {
    for (auto *disp : this->displays_) {
      if (!disp->is_idle())
        return true;
    }
  }
  return false;
}

void LvglComponent::write_random_() {
  int iterations = 6 - lv_display_get_inactive_time(this->disp_) / 60000;
  if (iterations <= 0)
    iterations = 1;
  int16_t width = lv_display_get_horizontal_resolution(this->disp_);
  int16_t height = lv_display_get_vertical_resolution(this->disp_);
  while (iterations-- != 0) {
    int32_t col = random_uint32() % width;
    col = col / this->draw_rounding * this->draw_rounding;
    int32_t row = random_uint32() % height;
    row = row / this->draw_rounding * this->draw_rounding;
    // size will be between 8 and 32, and a multiple of draw_rounding
    int32_t size = (random_uint32() % 25 + 8) / this->draw_rounding * this->draw_rounding;
    lv_area_t area{.x1 = col, .y1 = row, .x2 = col + size - 1, .y2 = row + size - 1};
    // clip to display bounds just in case
    if (area.x2 >= width)
      area.x2 = width - 1;
    if (area.y2 >= height)
      area.y2 = height - 1;

    // line_len can't exceed 1024, and minimum buffer size is 2048, so this won't overflow the buffer
    size_t line_len = lv_area_get_width(&area) * lv_area_get_height(&area) / 2;
    for (size_t i = 0; i != line_len; i++) {
      reinterpret_cast<uint32_t *>(this->draw_buf_)[i] = random_uint32();
    }
    this->draw_buffer_(&area, reinterpret_cast<lv_color_data *>(this->draw_buf_));
  }
}

/**
 * @class LvglComponent
 * @brief Component for rendering LVGL.
 *
 * This component renders LVGL widgets on a display. Some initialisation must be done in the constructor
 * since LVGL needs to be initialised before any widgets can be created.
 *
 * @param displays a list of displays to render onto. All displays must have the same
 *                 resolution.
 * @param buffer_frac the fraction of the display resolution to use for the LVGL
 *                    draw buffer. A higher value will make animations smoother but
 *                    also increase memory usage.
 * @param full_refresh if true, the display will be fully refreshed on every frame.
 *                     If false, only changed areas will be updated.
 * @param draw_rounding the rounding to use when drawing. A value of 1 will draw
 *                      without any rounding, a value of 2 will round to the nearest
 *                      multiple of 2, and so on.
 * @param resume_on_input if true, this component will resume rendering when the user
 *                         presses a key or clicks on the screen.
 * @param rotation_type What rotation type to use, if any
 */
LvglComponent::LvglComponent(std::vector<display::Display *> displays, float buffer_frac, bool full_refresh,
                             int draw_rounding, bool resume_on_input, bool update_when_display_idle,
                             RotationType rotation_type)
    : draw_rounding(draw_rounding),
      displays_(std::move(displays)),
      buffer_frac_(buffer_frac),
      full_refresh_(full_refresh),
      resume_on_input_(resume_on_input),
      update_when_display_idle_(update_when_display_idle),
      rotation_type_(rotation_type) {
  this->disp_ = lv_display_create(240, 240);
}

void LvglComponent::set_resolution_() const {
  int32_t width = this->width_;
  int32_t height = this->height_;
  if (this->rotation_ == display::DISPLAY_ROTATION_90_DEGREES ||
      this->rotation_ == display::DISPLAY_ROTATION_270_DEGREES) {
    std::swap(width, height);
  }
  ESP_LOGD(TAG, "Setting resolution to %u x %u (rotation %d)", (unsigned) width, (unsigned) height,
           (int) this->rotation_);
  if (this->rotation_type_ == RotationType::ROTATION_HARDWARE) {
    for (auto *display : this->displays_)
      display->set_rotation(this->rotation_);
  }
  lv_display_set_resolution(this->disp_, width, height);
}
void LvglComponent::setup() {
  auto *display = this->displays_[0];
  auto rounding = this->draw_rounding;
  this->width_ = display->get_native_width();
  this->height_ = display->get_native_height();
  // cater for displays with dimensions that don't divide by the required rounding
  auto width = (this->width_ + rounding - 1) / rounding * rounding;
  auto height = (this->height_ + rounding - 1) / rounding * rounding;
  auto frac = this->buffer_frac_;
  if (frac == 0)
    frac = 1;
  auto buf_bytes = clamp_at_least(width * height / frac * LV_COLOR_DEPTH / 8, MIN_BUFFER_SIZE);
  void *buffer = nullptr;
  // for small buffers, try to allocate in internal memory first to improve performance
  if (this->buffer_frac_ >= MIN_BUFFER_FRAC / 2)
    buffer = lv_alloc_draw_buf(buf_bytes, true);  // NOLINT
  if (buffer == nullptr)
    buffer = lv_alloc_draw_buf(buf_bytes, false);  // NOLINT
  // if specific buffer size not set and can't get 100%, try for a smaller one
  if (buffer == nullptr && this->buffer_frac_ == 0) {
    frac = MIN_BUFFER_FRAC;
    buf_bytes /= MIN_BUFFER_FRAC;
    buffer = lv_alloc_draw_buf(buf_bytes, false);  // NOLINT
  }
  this->buffer_frac_ = frac;
  if (buffer == nullptr) {
    this->status_set_error(LOG_STR("Memory allocation failure"));
    this->mark_failed();
    return;
  }
  this->draw_buf_ = static_cast<uint8_t *>(buffer);
  this->set_resolution_();
  lv_display_set_color_format(this->disp_, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(this->disp_, static_flush_cb);
  lv_display_set_user_data(this->disp_, this);
  lv_display_add_event_cb(this->disp_, rounder_cb, LV_EVENT_INVALIDATE_AREA, this);
  lv_display_set_buffers(this->disp_, this->draw_buf_, nullptr, buf_bytes,
                         this->full_refresh_ ? LV_DISPLAY_RENDER_MODE_FULL : LV_DISPLAY_RENDER_MODE_PARTIAL);
  if (this->rotation_type_ == RotationType::ROTATION_SOFTWARE) {
    this->rotate_buf_ = static_cast<lv_color_t *>(lv_alloc_draw_buf(buf_bytes, false));  // NOLINT
    if (this->rotate_buf_ == nullptr) {
      this->status_set_error(LOG_STR("Memory allocation failure"));
      this->mark_failed();
      return;
    }
#ifdef USE_ESP32_VARIANT_ESP32P4
    ppa_client_config_t ppa_config{};
    ppa_config.oper_type = PPA_OPERATION_SRM;
    ppa_config.max_pending_trans_num = 1;
    if (ppa_register_client(&ppa_config, &this->ppa_client_) != ESP_OK) {
      ESP_LOGW(TAG, "PPA client registration failed, using software rotation");
      this->ppa_client_ = nullptr;
    }
#endif
  }
  if (this->draw_start_callback_ != nullptr) {
    lv_display_add_event_cb(this->disp_, render_start_cb, LV_EVENT_RENDER_START, this);
  }
  if (this->draw_end_callback_ != nullptr || this->update_when_display_idle_) {
    lv_display_add_event_cb(this->disp_, render_end_cb, LV_EVENT_REFR_READY, this);
  }
#if LV_USE_LOG
  lv_log_register_print_cb([](lv_log_level_t level, const char *buf) {
    auto next = strchr(buf, ')');
    if (next != nullptr)
      buf = next + 1;
    while (isspace(*buf))
      buf++;
    if (level >= sizeof(LOG_LEVEL_MAP) / sizeof(LOG_LEVEL_MAP[0]))
      level = sizeof(LOG_LEVEL_MAP) / sizeof(LOG_LEVEL_MAP[0]) - 1;
    esp_log_printf_(LOG_LEVEL_MAP[level], TAG, 0, "%.*s", (int) strlen(buf) - 1, buf);
  });
#endif
  this->show_page(0, LV_SCREEN_LOAD_ANIM_NONE, 0);
  lv_display_trigger_activity(this->disp_);
}

void LvglComponent::update() {
  // update indicators
  if (this->is_paused()) {
    return;
  }
  this->idle_callbacks_.call(lv_display_get_inactive_time(this->disp_));
}

void LvglComponent::loop() {
  if (this->is_paused()) {
    if (this->paused_ && this->show_snow_)
      this->write_random_();
  } else {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
    auto now = millis();
    lv_timer_handler();
    auto elapsed = millis() - now;
    if (elapsed > 15) {
      ESP_LOGV(TAG, "lv_timer_handler took %dms", (int) (millis() - now));
    }
#else
    lv_timer_handler();
#endif
  }
}

#ifdef USE_LVGL_ANIMIMG
void lv_animimg_stop(lv_obj_t *obj) {
  int32_t duration = lv_animimg_get_duration(obj);
  lv_animimg_set_duration(obj, 0);
  lv_animimg_start(obj);
  lv_animimg_set_duration(obj, duration);
}
#endif
void LvglComponent::static_flush_cb(lv_display_t *disp_drv, const lv_area_t *area, uint8_t *color_p) {
  reinterpret_cast<LvglComponent *>(lv_display_get_user_data(disp_drv))->flush_cb_(disp_drv, area, color_p);
}

#ifdef USE_LVGL_SCALE
/**
 * Function to apply colors to ticks based on position
 * @param e The event data
 * @param color_start The color to apply to the first tick
 * @param color_end  The color to apply to the last tick
 * @param width
 */
void lv_scale_draw_event_cb(lv_event_t *e, int16_t range_start, int16_t range_end, lv_color_t color_start,
                            lv_color_t color_end, int width, bool local) {
  auto *scale = static_cast<lv_obj_t *>(lv_event_get_target(e));
  lv_draw_task_t *task = lv_event_get_draw_task(e);

  if (lv_draw_task_get_type(task) == LV_DRAW_TASK_TYPE_LINE) {
    auto *line_dsc = static_cast<lv_draw_line_dsc_t *>(lv_draw_task_get_draw_dsc(task));
    int tick = line_dsc->base.id2;
    if (tick >= range_start && tick <= range_end) {
      int ratio;
      if (local) {
        int range = range_end - range_start;
        tick -= range_start;
        ratio = range == 0 ? 0 : (tick * 255) / range;
      } else {
        // total tick count is guaranteed to be at least 2.
        ratio = (line_dsc->base.id1 * 255) / (lv_scale_get_total_tick_count(scale) - 1);
      }
      line_dsc->color = lv_color_mix(color_end, color_start, ratio);
      line_dsc->width += width;
    }
  }
}
#endif  // USE_LVGL_SCALE

#ifdef USE_LVGL_GRADIENT
/**
 *
 * @param dsc The gradient descriptor containing the color stops
 * @param pos The current position to calculate the color for
 * @return The color for the given position
 */

lv_color_t lv_grad_calculate_color(const lv_grad_dsc_t *dsc, int32_t pos) {
  if (dsc->stops_count == 0)
    return lv_color_black();
  if (dsc->stops_count == 1 || pos <= dsc->stops[0].frac)
    return dsc->stops[0].color;
  if (pos >= dsc->stops[dsc->stops_count - 1].frac)
    return dsc->stops[dsc->stops_count - 1].color;
  int i = 1;
  while (i < dsc->stops_count && dsc->stops[i].frac < pos)
    i++;
  auto *stop1 = &dsc->stops[i - 1];
  auto *stop2 = &dsc->stops[i];
  int32_t range = stop2->frac - stop1->frac;
  int32_t offset = pos - stop1->frac;
  return lv_color_mix(stop2->color, stop1->color, range == 0 ? 0 : (offset * 255) / range);
}
#endif  // USE_LVGL_GRADIENT

lv_point_t LvglComponent::get_touch_relative_to_obj(lv_obj_t *obj) {
  auto *indev = lv_indev_get_act();
  if (indev == nullptr) {
    return {INT32_MAX, INT32_MAX};
  }
  lv_point_t point;
  lv_indev_get_point(indev, &point);
  lv_area_t coords;
  lv_obj_get_coords(obj, &coords);
  point.x -= coords.x1;
  point.y -= coords.y1;
  return point;
}

static void lv_container_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj) {
  LV_TRACE_OBJ_CREATE("begin");
  LV_UNUSED(class_p);
}

// Container class. Name is based on LVGL naming convention but upper case to keep ESPHome clang-tidy happy
const lv_obj_class_t LV_CONTAINER_CLASS = {
    .base_class = &lv_obj_class,
    .constructor_cb = lv_container_constructor,
    .name = "lv_container",
};

lv_obj_t *lv_container_create(lv_obj_t *parent) {
  lv_obj_t *obj = lv_obj_class_create_obj(&LV_CONTAINER_CLASS, parent);
  lv_obj_class_init_obj(obj);
  return obj;
}
}  // namespace esphome::lvgl

lv_result_t lv_mem_test_core() { return LV_RESULT_OK; }

void lv_mem_init() {}

void lv_mem_deinit() {}

#if defined(USE_HOST) || defined(USE_RP2040) || defined(USE_ESP8266)
void *lv_malloc_core(size_t size) {
  auto *ptr = malloc(size);  // NOLINT
  if (ptr == nullptr) {
    ESP_LOGE(esphome::lvgl::TAG, "Failed to allocate %zu bytes", size);
  }
  return ptr;
}
void lv_free_core(void *ptr) { return free(ptr); }                            // NOLINT
void *lv_realloc_core(void *ptr, size_t size) { return realloc(ptr, size); }  // NOLINT

void lv_mem_monitor_core(lv_mem_monitor_t *mon_p) { memset(mon_p, 0, sizeof(lv_mem_monitor_t)); }
static void *lv_alloc_draw_buf(size_t size, bool internal) {
  return malloc(size);  // NOLINT
}

#elif defined(USE_ESP32)
static unsigned cap_bits = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;  // NOLINT

static void *lv_alloc_draw_buf(size_t size, bool internal) {
  void *buffer;
  size = LV_ROUND_UP(size, LV_DRAW_BUF_ALIGN);
  buffer = heap_caps_aligned_alloc(LV_DRAW_BUF_ALIGN, size, internal ? MALLOC_CAP_8BIT : cap_bits);  // NOLINT
  if (buffer == nullptr)
    ESP_LOGW(esphome::lvgl::TAG, "Failed to allocate %zu bytes for %sdraw buffer", size, internal ? "internal " : "");
  return buffer;
}

void lv_mem_monitor_core(lv_mem_monitor_t *mon_p) {
  multi_heap_info_t heap_info;
  heap_caps_get_info(&heap_info, cap_bits);
  mon_p->total_size = heap_info.total_allocated_bytes + heap_info.total_free_bytes;
  mon_p->free_size = heap_info.total_free_bytes;
  mon_p->max_used = heap_info.total_allocated_bytes;
  mon_p->free_biggest_size = heap_info.largest_free_block;
  mon_p->used_cnt = heap_info.allocated_blocks;
  mon_p->free_cnt = heap_info.free_blocks;
  mon_p->used_pct = heap_info.allocated_blocks * 100 / (heap_info.allocated_blocks + heap_info.free_blocks);
  mon_p->frag_pct = 0;
}

void *lv_malloc_core(size_t size) {
  void *ptr;
  ptr = heap_caps_malloc(size, cap_bits);
  if (ptr == nullptr) {
    cap_bits = MALLOC_CAP_8BIT;
    ptr = heap_caps_malloc(size, cap_bits);
  }
  if (ptr == nullptr) {
    ESP_LOGE(esphome::lvgl::TAG, "Failed to allocate %zu bytes", size);
    return nullptr;
  }
  ESP_LOGV(esphome::lvgl::TAG, "allocate %zu - > %p", size, ptr);
  return ptr;
}

void lv_free_core(void *ptr) {
  ESP_LOGV(esphome::lvgl::TAG, "free %p", ptr);
  if (ptr == nullptr)
    return;
  heap_caps_free(ptr);
}

void *lv_realloc_core(void *ptr, size_t size) {
  ESP_LOGV(esphome::lvgl::TAG, "realloc %p: %zu", ptr, size);
  return heap_caps_realloc(ptr, size, cap_bits);
}
#endif
