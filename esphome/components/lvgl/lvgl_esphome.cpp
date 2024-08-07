#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"
#include "lvgl_hal.h"
#include "lvgl_esphome.h"

namespace esphome {
namespace lvgl {
static const char *const TAG = "lvgl";

#if LV_USE_LOG
static void log_cb(const char *buf) {
  esp_log_printf_(ESPHOME_LOG_LEVEL_INFO, TAG, 0, "%.*s", (int) strlen(buf) - 1, buf);
}
#endif  // LV_USE_LOG

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

lv_event_code_t lv_custom_event;  // NOLINT
void LvglComponent::dump_config() { ESP_LOGCONFIG(TAG, "LVGL:"); }
void LvglComponent::set_paused(bool paused, bool show_snow) {
  this->paused_ = paused;
  this->show_snow_ = show_snow;
  this->snow_line_ = 0;
  if (!paused && lv_scr_act() != nullptr) {
    lv_disp_trig_activity(this->disp_);  // resets the inactivity time
    lv_obj_invalidate(lv_scr_act());
  }
}
void LvglComponent::add_event_cb(lv_obj_t *obj, event_callback_t callback, lv_event_code_t event) {
  lv_obj_add_event_cb(obj, callback, event, this);
  if (event == LV_EVENT_VALUE_CHANGED) {
    lv_obj_add_event_cb(obj, callback, lv_custom_event, this);
  }
}
void LvglComponent::add_event_cb(lv_obj_t *obj, event_callback_t callback, lv_event_code_t event1,
                                 lv_event_code_t event2) {
  this->add_event_cb(obj, callback, event1);
  this->add_event_cb(obj, callback, event2);
}
void LvglComponent::add_page(LvPageType *page) {
  this->pages_.push_back(page);
  page->setup(this->pages_.size() - 1);
}
void LvglComponent::show_page(size_t index, lv_scr_load_anim_t anim, uint32_t time) {
  if (index >= this->pages_.size())
    return;
  this->current_page_ = index;
  lv_scr_load_anim(this->pages_[this->current_page_]->obj, anim, time, 0, false);
}
void LvglComponent::show_next_page(lv_scr_load_anim_t anim, uint32_t time) {
  if (this->pages_.empty() || (this->current_page_ == this->pages_.size() - 1 && !this->page_wrap_))
    return;
  do {
    this->current_page_ = (this->current_page_ + 1) % this->pages_.size();
  } while (this->pages_[this->current_page_]->skip);  // skip empty pages()
  this->show_page(this->current_page_, anim, time);
}
void LvglComponent::show_prev_page(lv_scr_load_anim_t anim, uint32_t time) {
  if (this->pages_.empty() || (this->current_page_ == 0 && !this->page_wrap_))
    return;
  do {
    this->current_page_ = (this->current_page_ + this->pages_.size() - 1) % this->pages_.size();
  } while (this->pages_[this->current_page_]->skip);  // skip empty pages()
  this->show_page(this->current_page_, anim, time);
}
void LvglComponent::draw_buffer_(const lv_area_t *area, const uint8_t *ptr) {
  for (auto *display : this->displays_) {
    display->draw_pixels_at(area->x1, area->y1, lv_area_get_width(area), lv_area_get_height(area), ptr,
                            display::COLOR_ORDER_RGB, LV_BITNESS, LV_COLOR_16_SWAP);
  }
}

void LvglComponent::flush_cb_(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
  if (!this->paused_) {
    auto now = millis();
    this->draw_buffer_(area, (const uint8_t *) color_p);
    ESP_LOGV(TAG, "flush_cb, area=%d/%d, %d/%d took %dms", area->x1, area->y1, lv_area_get_width(area),
             lv_area_get_height(area), (int) (millis() - now));
  }
  lv_disp_flush_ready(disp_drv);
}
IdleTrigger::IdleTrigger(LvglComponent *parent, TemplatableValue<uint32_t> timeout) : timeout_(std::move(timeout)) {
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
LVTouchListener::LVTouchListener(uint16_t long_press_time, uint16_t long_press_repeat_time) {
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
void LVTouchListener::update(const touchscreen::TouchPoints_t &tpoints) {
  this->touch_pressed_ = !this->parent_->is_paused() && !tpoints.empty();
  if (this->touch_pressed_)
    this->touch_point_ = tpoints[0];
}
#endif  // USE_LVGL_TOUCHSCREEN

#ifdef USE_LVGL_KEY_LISTENER
LVEncoderListener::LVEncoderListener(lv_indev_type_t type, uint16_t lpt, uint16_t lprt) {
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
#endif  // USE_LVGL_KEY_LISTENER

#ifdef USE_LVGL_BUTTONMATRIX
void LvButtonMatrixType::set_obj(lv_obj_t *lv_obj) {
  LvCompound::set_obj(lv_obj);
  lv_obj_add_event_cb(
      lv_obj,
      [](lv_event_t *event) {
        auto *self = static_cast<LvButtonMatrixType *>(event->user_data);
        if (self->key_callback_.size() == 0)
          return;
        auto key_idx = lv_btnmatrix_get_selected_btn(self->obj);
        if (key_idx == LV_BTNMATRIX_BTN_NONE)
          return;
        if (self->key_map_.count(key_idx) != 0) {
          self->send_key_(self->key_map_[key_idx]);
          return;
        }
        const auto *str = lv_btnmatrix_get_btn_text(self->obj, key_idx);
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
        auto *self = static_cast<LvKeyboardType *>(event->user_data);
        if (self->key_callback_.size() == 0)
          return;

        auto key_idx = lv_btnmatrix_get_selected_btn(self->obj);
        if (key_idx == LV_BTNMATRIX_BTN_NONE)
          return;
        const char *txt = lv_btnmatrix_get_btn_text(self->obj, key_idx);
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

void LvglComponent::write_random_() {
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
    area.y1 = static_cast<lv_coord_t>(random_uint32() % (this->disp_drv_.ver_res / 2) * 2);
  } else {
    area.y1 = this->snow_line_++ * 2;
  }
  // write 2 lines
  area.y2 = area.y1 + 1;
  this->draw_buffer_(&area, (const uint8_t *) this->draw_buf_.buf1);
}

void LvglComponent::setup() {
  ESP_LOGCONFIG(TAG, "LVGL Setup starts");
#if LV_USE_LOG
  lv_log_register_print_cb(log_cb);
#endif
  lv_init();
  lv_custom_event = static_cast<lv_event_code_t>(lv_event_register_id());
  auto *display = this->displays_[0];
  size_t buffer_pixels = display->get_width() * display->get_height() / this->buffer_frac_;
  auto buf_bytes = buffer_pixels * LV_COLOR_DEPTH / 8;
  auto *buf = lv_custom_mem_alloc(buf_bytes);
  if (buf == nullptr) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_ERROR
    ESP_LOGE(TAG, "Malloc failed to allocate %zu bytes", buf_bytes);
#endif
    this->mark_failed();
    this->status_set_error("Memory allocation failure");
    return;
  }
  lv_disp_draw_buf_init(&this->draw_buf_, buf, nullptr, buffer_pixels);
  lv_disp_drv_init(&this->disp_drv_);
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
  display->set_rotation(display::DISPLAY_ROTATION_0_DEGREES);
  this->disp_drv_.hor_res = (lv_coord_t) display->get_width();
  this->disp_drv_.ver_res = (lv_coord_t) display->get_height();
  ESP_LOGV(TAG, "sw_rotate = %d, rotated=%d", this->disp_drv_.sw_rotate, this->disp_drv_.rotated);
  this->disp_ = lv_disp_drv_register(&this->disp_drv_);
  for (const auto &v : this->init_lambdas_)
    v(this);
  this->show_page(0, LV_SCR_LOAD_ANIM_NONE, 0);
  lv_disp_trig_activity(this->disp_);
  ESP_LOGCONFIG(TAG, "LVGL Setup complete");
}
void LvglComponent::update() {
  // update indicators
  if (this->paused_) {
    return;
  }
  this->idle_callbacks_.call(lv_disp_get_inactive_time(this->disp_));
}
void LvglComponent::loop() {
  if (this->paused_) {
    if (this->show_snow_)
      this->write_random_();
  }
  lv_timer_handler_run_in_period(5);
}

#ifdef USE_LVGL_IMAGE
lv_img_dsc_t *lv_img_from(image::Image *src, lv_img_dsc_t *img_dsc) {
  if (img_dsc == nullptr)
    img_dsc = new lv_img_dsc_t();  // NOLINT
  img_dsc->header.always_zero = 0;
  img_dsc->header.reserved = 0;
  img_dsc->header.w = src->get_width();
  img_dsc->header.h = src->get_height();
  img_dsc->data = src->get_data_start();
  img_dsc->data_size = image_type_to_width_stride(img_dsc->header.w * img_dsc->header.h, src->get_type());
  switch (src->get_type()) {
    case image::IMAGE_TYPE_BINARY:
      img_dsc->header.cf = LV_IMG_CF_ALPHA_1BIT;
      break;

    case image::IMAGE_TYPE_GRAYSCALE:
      img_dsc->header.cf = LV_IMG_CF_ALPHA_8BIT;
      break;

    case image::IMAGE_TYPE_RGB24:
      img_dsc->header.cf = LV_IMG_CF_RGB888;
      break;

    case image::IMAGE_TYPE_RGB565:
#if LV_COLOR_DEPTH == 16
      img_dsc->header.cf = src->has_transparency() ? LV_IMG_CF_TRUE_COLOR_CHROMA_KEYED : LV_IMG_CF_TRUE_COLOR;
#else
      img_dsc->header.cf = LV_IMG_CF_RGB565;
#endif
      break;

    case image::IMAGE_TYPE_RGBA:
#if LV_COLOR_DEPTH == 32
      img_dsc->header.cf = LV_IMG_CF_TRUE_COLOR;
#else
      img_dsc->header.cf = LV_IMG_CF_RGBA8888;
#endif
      break;
  }
  return img_dsc;
}
#endif  // USE_LVGL_IMAGE

#ifdef USE_LVGL_ANIMIMG
void lv_animimg_stop(lv_obj_t *obj) {
  auto *animg = (lv_animimg_t *) obj;
  int32_t duration = animg->anim.time;
  lv_animimg_set_duration(obj, 0);
  lv_animimg_start(obj);
  lv_animimg_set_duration(obj, duration);
}
#endif
void LvglComponent::static_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
  reinterpret_cast<LvglComponent *>(disp_drv->user_data)->flush_cb_(disp_drv, area, color_p);
}
}  // namespace lvgl
}  // namespace esphome

size_t lv_millis(void) { return esphome::millis(); }

#if defined(USE_HOST) || defined(USE_RP2040) || defined(USE_ESP8266)
void *lv_custom_mem_alloc(size_t size) {
  auto *ptr = malloc(size);  // NOLINT
  if (ptr == nullptr) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_ERROR
    esphome::ESP_LOGE(esphome::lvgl::TAG, "Failed to allocate %zu bytes", size);
#endif
  }
  return ptr;
}
void lv_custom_mem_free(void *ptr) { return free(ptr); }                            // NOLINT
void *lv_custom_mem_realloc(void *ptr, size_t size) { return realloc(ptr, size); }  // NOLINT
#else
static unsigned cap_bits = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;  // NOLINT

void *lv_custom_mem_alloc(size_t size) {
  void *ptr;
  ptr = heap_caps_malloc(size, cap_bits);
  if (ptr == nullptr) {
    cap_bits = MALLOC_CAP_8BIT;
    ptr = heap_caps_malloc(size, cap_bits);
  }
  if (ptr == nullptr) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_ERROR
    esphome::ESP_LOGE(esphome::lvgl::TAG, "Failed to allocate %zu bytes", size);
#endif
    return nullptr;
  }
#ifdef ESPHOME_LOG_HAS_VERBOSE
  esphome::ESP_LOGV(esphome::lvgl::TAG, "allocate %zu - > %p", size, ptr);
#endif
  return ptr;
}

void lv_custom_mem_free(void *ptr) {
#ifdef ESPHOME_LOG_HAS_VERBOSE
  esphome::ESP_LOGV(esphome::lvgl::TAG, "free %p", ptr);
#endif
  if (ptr == nullptr)
    return;
  heap_caps_free(ptr);
}

void *lv_custom_mem_realloc(void *ptr, size_t size) {
#ifdef ESPHOME_LOG_HAS_VERBOSE
  esphome::ESP_LOGV(esphome::lvgl::TAG, "realloc %p: %zu", ptr, size);
#endif
  return heap_caps_realloc(ptr, size, cap_bits);
}
#endif
