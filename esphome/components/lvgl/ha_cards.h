#pragma once

#include "esphome/core/defines.h"
#ifdef USE_LVGL

#include "lvgl_esphome.h"

namespace esphome {
namespace lvgl {

/**
 * Gauge Card - A circular arc gauge with optional value/name labels
 */
class LvGaugeCardType : public LvCompound {
 public:
  void set_obj(lv_obj_t *obj) {
    LvCompound::set_obj(obj);
  }

  void create_arc() {
    this->arc_ = lv_arc_create(this->obj);
    lv_obj_set_size(this->arc_, LV_PCT(100), LV_PCT(100));
    lv_obj_center(this->arc_);
  }

  lv_obj_t *get_arc() { return this->arc_; }

  void create_value_label() {
    this->value_label_ = lv_label_create(this->obj);
    lv_obj_center(this->value_label_);
  }

  lv_obj_t *get_value_label() { return this->value_label_; }

  void create_name_label() {
    this->name_label_ = lv_label_create(this->obj);
    lv_obj_align(this->name_label_, LV_ALIGN_BOTTOM_MID, 0, -10);
  }

  lv_obj_t *get_name_label() { return this->name_label_; }

  void set_value(float value) {
    this->value_ = value;
    if (this->arc_ != nullptr) {
      lv_arc_set_value(this->arc_, static_cast<int32_t>(value));
    }
  }

  float get_value() { return this->value_; }

  void update_value_label(float value, const char *format) {
    if (this->value_label_ != nullptr) {
      lv_label_set_text_fmt(this->value_label_, format, value);
    }
  }

 protected:
  lv_obj_t *arc_{nullptr};
  lv_obj_t *value_label_{nullptr};
  lv_obj_t *name_label_{nullptr};
  float value_{0};
};

/**
 * Button Card - A button with icon and label
 */
class LvButtonCardType : public LvCompound {
 public:
  void set_obj(lv_obj_t *obj) {
    LvCompound::set_obj(obj);
  }

  void create_icon() {
    this->icon_ = lv_img_create(this->obj);
  }

  lv_obj_t *get_icon() { return this->icon_; }

  void create_label() {
    this->label_ = lv_label_create(this->obj);
  }

  lv_obj_t *get_label() { return this->label_; }

  void create_state_label() {
    this->state_label_ = lv_label_create(this->obj);
  }

  lv_obj_t *get_state_label() { return this->state_label_; }

  bool is_on() { return this->state_; }

  void set_state(bool state) {
    this->state_ = state;
    if (state) {
      lv_obj_add_state(this->obj, LV_STATE_CHECKED);
    } else {
      lv_obj_clear_state(this->obj, LV_STATE_CHECKED);
    }
  }

 protected:
  lv_obj_t *icon_{nullptr};
  lv_obj_t *label_{nullptr};
  lv_obj_t *state_label_{nullptr};
  bool state_{false};
};

/**
 * Tile Card - A modern tile-style widget
 */
class LvTileCardType : public LvCompound {
 public:
  void set_obj(lv_obj_t *obj) {
    LvCompound::set_obj(obj);
  }

  void create_icon() {
    this->icon_ = lv_img_create(this->obj);
  }

  lv_obj_t *get_icon() { return this->icon_; }

  void create_name_label() {
    this->name_label_ = lv_label_create(this->obj);
  }

  lv_obj_t *get_name_label() { return this->name_label_; }

  void create_state_label() {
    this->state_label_ = lv_label_create(this->obj);
  }

  lv_obj_t *get_state_label() { return this->state_label_; }

  void set_state(bool state) {
    this->state_ = state;
    if (state) {
      lv_obj_add_state(this->obj, LV_STATE_CHECKED);
    } else {
      lv_obj_clear_state(this->obj, LV_STATE_CHECKED);
    }
  }

  bool is_on() { return this->state_; }

 protected:
  lv_obj_t *icon_{nullptr};
  lv_obj_t *name_label_{nullptr};
  lv_obj_t *state_label_{nullptr};
  bool state_{false};
};

/**
 * Entity Card - Simple entity state display
 */
class LvEntityCardType : public LvCompound {
 public:
  void set_obj(lv_obj_t *obj) {
    LvCompound::set_obj(obj);
  }

  void create_icon() {
    this->icon_ = lv_img_create(this->obj);
  }

  lv_obj_t *get_icon() { return this->icon_; }

  void create_name_label() {
    this->name_label_ = lv_label_create(this->obj);
  }

  lv_obj_t *get_name_label() { return this->name_label_; }

  void create_value_label() {
    this->value_label_ = lv_label_create(this->obj);
  }

  lv_obj_t *get_value_label() { return this->value_label_; }

  void set_value(const char *value) {
    if (this->value_label_ != nullptr) {
      lv_label_set_text(this->value_label_, value);
    }
  }

 protected:
  lv_obj_t *icon_{nullptr};
  lv_obj_t *name_label_{nullptr};
  lv_obj_t *value_label_{nullptr};
};

/**
 * Thermostat Card - Climate control interface
 */
class LvThermostatCardType : public LvCompound {
 public:
  void set_obj(lv_obj_t *obj) {
    LvCompound::set_obj(obj);
  }

  void create_temperature_arc() {
    this->temp_arc_ = lv_arc_create(this->obj);
  }

  lv_obj_t *get_temperature_arc() { return this->temp_arc_; }

  void create_temperature_label() {
    this->temp_label_ = lv_label_create(this->obj);
  }

  lv_obj_t *get_temperature_label() { return this->temp_label_; }

  void create_setpoint_label() {
    this->setpoint_label_ = lv_label_create(this->obj);
  }

  lv_obj_t *get_setpoint_label() { return this->setpoint_label_; }

  void create_mode_buttons() {
    this->mode_container_ = lv_obj_create(this->obj);
  }

  lv_obj_t *get_mode_container() { return this->mode_container_; }

  void create_up_button() {
    this->up_btn_ = lv_btn_create(this->obj);
    lv_obj_t *label = lv_label_create(this->up_btn_);
    lv_label_set_text(label, LV_SYMBOL_UP);
    lv_obj_center(label);
  }

  lv_obj_t *get_up_button() { return this->up_btn_; }

  void create_down_button() {
    this->down_btn_ = lv_btn_create(this->obj);
    lv_obj_t *label = lv_label_create(this->down_btn_);
    lv_label_set_text(label, LV_SYMBOL_DOWN);
    lv_obj_center(label);
  }

  lv_obj_t *get_down_button() { return this->down_btn_; }

  void set_current_temperature(float temp) {
    this->current_temp_ = temp;
    if (this->temp_label_ != nullptr) {
      lv_label_set_text_fmt(this->temp_label_, "%.1f°", temp);
    }
  }

  float get_current_temperature() { return this->current_temp_; }

  void set_target_temperature(float temp) {
    this->target_temp_ = temp;
    if (this->temp_arc_ != nullptr) {
      lv_arc_set_value(this->temp_arc_, static_cast<int32_t>(temp * 10));
    }
    if (this->setpoint_label_ != nullptr) {
      lv_label_set_text_fmt(this->setpoint_label_, "%.1f°", temp);
    }
  }

  float get_target_temperature() { return this->target_temp_; }

 protected:
  lv_obj_t *temp_arc_{nullptr};
  lv_obj_t *temp_label_{nullptr};
  lv_obj_t *setpoint_label_{nullptr};
  lv_obj_t *mode_container_{nullptr};
  lv_obj_t *up_btn_{nullptr};
  lv_obj_t *down_btn_{nullptr};
  float current_temp_{0};
  float target_temp_{20};
};

}  // namespace lvgl
}  // namespace esphome

#endif  // USE_LVGL
