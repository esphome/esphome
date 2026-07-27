#pragma once

#include "lvgl_esphome.h"
#include "esphome/core/color.h"

#ifdef USE_LVGL_COLOR_PICKER

namespace esphome::lvgl {

class LvColorPickerType : public LvCompound {
  constexpr static const char *const TAG = "lvgl.color_picker";

 public:
  void set_color(lv_color_t color) {
    this->color_ = color;
    if (this->obj != nullptr)
      this->update_color_();
  }
  Color get_color() {
    return {static_cast<uint8_t>(lv_slider_get_value(this->red_slider_)),
            static_cast<uint8_t>(lv_slider_get_value(this->green_slider_)),
            static_cast<uint8_t>(lv_slider_get_value(this->blue_slider_))};
  }
  void set_obj(lv_obj_t *outer) override {
    LvCompound::set_obj(outer);
    this->color_bar_.dir = LV_GRAD_DIR_VER;
    this->color_bar_.stops_count = 7;
    this->color_bar_.stops[0].color = lv_color_hex(0xFF0000);
    this->color_bar_.stops[0].opa = LV_OPA_COVER;
    this->color_bar_.stops[0].frac = 0;
    this->color_bar_.stops[1].color = lv_color_hex(0xFF00FF);
    this->color_bar_.stops[1].opa = LV_OPA_COVER;
    this->color_bar_.stops[1].frac = 42;
    this->color_bar_.stops[2].color = lv_color_hex(0xFF);
    this->color_bar_.stops[2].opa = LV_OPA_COVER;
    this->color_bar_.stops[2].frac = 84;
    this->color_bar_.stops[3].color = lv_color_hex(0xFFFF);
    this->color_bar_.stops[3].opa = LV_OPA_COVER;
    this->color_bar_.stops[3].frac = 127;
    this->color_bar_.stops[4].color = lv_color_hex(0xFF00);
    this->color_bar_.stops[4].opa = LV_OPA_COVER;
    this->color_bar_.stops[4].frac = 169;
    this->color_bar_.stops[5].color = lv_color_hex(0xFFFF00);
    this->color_bar_.stops[5].opa = LV_OPA_COVER;
    this->color_bar_.stops[5].frac = 212;
    this->color_bar_.stops[6].color = lv_color_hex(0xFF0000);
    this->color_bar_.stops[6].opa = LV_OPA_COVER;
    this->color_bar_.stops[6].frac = 255;
    this->brightness_bar_.dir = LV_GRAD_DIR_HOR;
    this->brightness_bar_.stops_count = 2;
    this->brightness_bar_.stops[0].color = lv_color_hex(0x00);
    this->brightness_bar_.stops[0].opa = LV_OPA_COVER;
    this->brightness_bar_.stops[0].frac = 0;
    this->brightness_bar_.stops[1].color = lv_color_hex(0xFFFFFF);
    this->brightness_bar_.stops[1].opa = LV_OPA_COVER;
    this->brightness_bar_.stops[1].frac = 255;
    this->saturation_bar_.dir = LV_GRAD_DIR_HOR;
    this->saturation_bar_.stops_count = 2;
    this->saturation_bar_.stops[0].color = lv_color_hex(0xFFFFFF);
    this->saturation_bar_.stops[0].opa = LV_OPA_COVER;
    this->saturation_bar_.stops[0].frac = 0;
    this->saturation_bar_.stops[1].color = lv_color_hex(0xFF0000);
    this->saturation_bar_.stops[1].opa = LV_OPA_COVER;
    this->saturation_bar_.stops[1].frac = 255;
    this->blue_bar_.dir = LV_GRAD_DIR_VER;
    this->blue_bar_.stops_count = 2;
    this->blue_bar_.stops[0].color = lv_color_hex(0xFF);
    this->blue_bar_.stops[0].opa = LV_OPA_COVER;
    this->blue_bar_.stops[0].frac = 0;
    this->blue_bar_.stops[1].color = lv_color_hex(0xFFFFFF);
    this->blue_bar_.stops[1].opa = LV_OPA_COVER;
    this->blue_bar_.stops[1].frac = 255;
    this->green_bar_.dir = LV_GRAD_DIR_VER;
    this->green_bar_.stops_count = 2;
    this->green_bar_.stops[0].color = lv_color_hex(0xFF00);
    this->green_bar_.stops[0].opa = LV_OPA_COVER;
    this->green_bar_.stops[0].frac = 0;
    this->green_bar_.stops[1].color = lv_color_hex(0xFFFFFF);
    this->green_bar_.stops[1].opa = LV_OPA_COVER;
    this->green_bar_.stops[1].frac = 255;
    this->red_bar_.dir = LV_GRAD_DIR_VER;
    this->red_bar_.stops_count = 2;
    this->red_bar_.stops[0].color = lv_color_hex(0xFF0000);
    this->red_bar_.stops[0].opa = LV_OPA_COVER;
    this->red_bar_.stops[0].frac = 0;
    this->red_bar_.stops[1].color = lv_color_hex(0xFFFFFF);
    this->red_bar_.stops[1].opa = LV_OPA_COVER;
    this->red_bar_.stops[1].frac = 255;

    lv_style_init(&this->slider_vert_);
    lv_style_set_align(&this->slider_vert_, LV_ALIGN_CENTER);
    lv_style_set_bg_opa(&this->slider_vert_, LV_OPA_COVER);
    lv_style_set_height(&this->slider_vert_, lv_pct(90));
    lv_style_set_pad_all(&this->slider_vert_, 0);
    lv_style_set_radius(&this->slider_vert_, 0);
    lv_style_set_width(&this->slider_vert_, lv_pct(33));
    lv_style_init(&this->slider_vert_knob_);
    lv_style_set_bg_color(&this->slider_vert_knob_, lv_color_hex(0xFFFFFF));
    lv_style_set_border_width(&this->slider_vert_knob_, 1);
    lv_style_set_outline_color(&this->slider_vert_knob_, lv_color_hex(0x00));
    lv_style_set_pad_bottom(&this->slider_vert_knob_, -4);
    lv_style_set_pad_top(&this->slider_vert_knob_, -4);
    lv_style_set_radius(&this->slider_vert_knob_, 0);
    lv_style_init(&this->slider_horz_);
    lv_style_set_align(&this->slider_horz_, LV_ALIGN_CENTER);
    lv_style_set_bg_opa(&this->slider_horz_, LV_OPA_COVER);
    lv_style_set_height(&this->slider_horz_, lv_pct(33));
    lv_style_set_pad_all(&this->slider_horz_, 0);
    lv_style_set_radius(&this->slider_horz_, 0);
    lv_style_set_width(&this->slider_horz_, lv_pct(90));
    lv_style_init(&this->slider_horz_knob_);
    lv_style_set_bg_color(&this->slider_horz_knob_, lv_color_hex(0xFFFFFF));
    lv_style_set_border_width(&this->slider_horz_knob_, 1);
    lv_style_set_outline_color(&this->slider_horz_knob_, lv_color_hex(0x00));
    lv_style_set_pad_left(&this->slider_horz_knob_, -4);
    lv_style_set_pad_right(&this->slider_horz_knob_, -4);
    lv_style_set_radius(&this->slider_horz_knob_, 0);

    lv_obj_set_layout(outer, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(outer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(outer, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(outer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(outer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(outer, 0, LV_PART_MAIN);
    lv_obj_set_style_width(outer, 400, LV_PART_MAIN);

    auto *hue_container = lv_obj_create(outer);
    lv_obj_set_style_border_width(hue_container, 0, LV_PART_MAIN);
    lv_obj_set_style_height(hue_container, lv_pct(100), LV_PART_MAIN);
    lv_obj_set_style_pad_all(hue_container, 0, LV_PART_MAIN);
    lv_obj_set_style_width(hue_container, lv_pct(15), LV_PART_MAIN);

    auto *hue_name = lv_label_create(hue_container);
    lv_obj_set_style_align(hue_name, LV_ALIGN_TOP_MID, LV_PART_MAIN);
    lv_label_set_text(hue_name, "H");

    this->hue_slider_ = lv_slider_create(hue_container);
    lv_obj_add_style(this->hue_slider_, &this->slider_vert_, LV_PART_MAIN);
    lv_obj_set_style_align(this->hue_slider_, LV_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_bg_grad(this->hue_slider_, &this->color_bar_, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(this->hue_slider_, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_add_style(this->hue_slider_, &this->slider_vert_knob_, LV_PART_KNOB);
    lv_slider_set_range(this->hue_slider_, 0, 360);
    lv_slider_set_mode(this->hue_slider_, LV_SLIDER_MODE_NORMAL);

    lv_obj_add_flag(this->hue_slider_, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    this->hue_value_ = lv_label_create(this->hue_slider_);
    lv_obj_set_style_align(this->hue_value_, LV_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(this->hue_value_, lv_color_black(), LV_PART_MAIN);
    auto *middle = lv_obj_create(outer);
    lv_obj_set_layout(middle, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(middle, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(middle, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_border_width(middle, 0, LV_PART_MAIN);
    lv_obj_set_style_height(middle, lv_pct(100), LV_PART_MAIN);
    lv_obj_set_style_pad_all(middle, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(middle, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(middle, 0, LV_PART_MAIN);
    lv_obj_set_style_width(middle, lv_pct(85), LV_PART_MAIN);

    auto *brightness_container = lv_obj_create(middle);
    lv_obj_set_style_border_width(brightness_container, 0, LV_PART_MAIN);
    lv_obj_set_style_height(brightness_container, lv_pct(15), LV_PART_MAIN);
    lv_obj_set_style_pad_all(brightness_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(brightness_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(brightness_container, 0, LV_PART_MAIN);
    lv_obj_set_style_width(brightness_container, lv_pct(100), LV_PART_MAIN);

    auto *brightness_name = lv_label_create(brightness_container);
    lv_obj_set_style_align(brightness_name, LV_ALIGN_TOP_MID, LV_PART_MAIN);
    lv_label_set_text(brightness_name, "V");

    this->brightness_slider_ = lv_slider_create(brightness_container);
    lv_obj_add_style(this->brightness_slider_, &this->slider_horz_, LV_PART_MAIN);
    lv_obj_set_style_bg_grad(this->brightness_slider_, &this->brightness_bar_, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(this->brightness_slider_, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_add_style(this->brightness_slider_, &this->slider_horz_knob_, LV_PART_KNOB);
    lv_slider_set_range(this->brightness_slider_, 0, 100);
    lv_slider_set_mode(this->brightness_slider_, LV_SLIDER_MODE_NORMAL);

    lv_obj_add_flag(this->brightness_slider_, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    this->brightness_value_ = lv_label_create(this->brightness_slider_);
    lv_obj_set_style_align(this->brightness_value_, LV_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(this->brightness_value_, lv_color_white(), LV_PART_MAIN);

    auto *inner = lv_obj_create(middle);
    lv_obj_set_layout(inner, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(inner, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(inner, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_border_width(inner, 0, LV_PART_MAIN);
    lv_obj_set_style_height(inner, lv_pct(70), LV_PART_MAIN);
    lv_obj_set_style_pad_all(inner, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(inner, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(inner, 0, LV_PART_MAIN);
    lv_obj_set_style_width(inner, lv_pct(100), LV_PART_MAIN);

    auto *indicator_container = lv_obj_create(inner);
    lv_obj_set_style_border_width(indicator_container, 0, LV_PART_MAIN);
    lv_obj_set_style_height(indicator_container, lv_pct(100), LV_PART_MAIN);
    lv_obj_set_style_pad_all(indicator_container, 0, LV_PART_MAIN);
    lv_obj_set_style_width(indicator_container, lv_pct(42), LV_PART_MAIN);

    this->color_text_ = lv_label_create(indicator_container);
    lv_obj_set_style_align(this->color_text_, LV_ALIGN_TOP_MID, LV_PART_MAIN);
    lv_obj_set_style_text_align(this->color_text_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_y(this->color_text_, lv_pct(10), LV_PART_MAIN);
    lv_label_set_text(this->color_text_, "#working");

    this->color_indicator_ = lv_obj_create(indicator_container);
    lv_obj_set_style_align(this->color_indicator_, LV_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(this->color_indicator_, lv_color_hex(0xFFFF00), LV_PART_MAIN);
    lv_obj_set_style_border_color(this->color_indicator_, lv_color_hex(0x808080), LV_PART_MAIN);
    lv_obj_set_style_border_width(this->color_indicator_, 1, LV_PART_MAIN);
    lv_obj_set_style_height(this->color_indicator_, lv_pct(50), LV_PART_MAIN);
    lv_obj_set_style_radius(this->color_indicator_, 0, LV_PART_MAIN);
    lv_obj_set_style_width(this->color_indicator_, lv_pct(90), LV_PART_MAIN);

    auto *rgb_container = lv_obj_create(inner);
    lv_obj_set_layout(rgb_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(rgb_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rgb_container, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_border_width(rgb_container, 0, LV_PART_MAIN);
    lv_obj_set_style_height(rgb_container, lv_pct(100), LV_PART_MAIN);
    lv_obj_set_style_pad_all(rgb_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(rgb_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(rgb_container, 0, LV_PART_MAIN);
    lv_obj_set_style_width(rgb_container, lv_pct(57), LV_PART_MAIN);

    auto *red_container = lv_obj_create(rgb_container);
    lv_obj_set_style_border_width(red_container, 0, LV_PART_MAIN);
    lv_obj_set_style_height(red_container, lv_pct(100), LV_PART_MAIN);
    lv_obj_set_style_pad_all(red_container, 0, LV_PART_MAIN);
    lv_obj_set_style_width(red_container, lv_pct(33), LV_PART_MAIN);

    auto *red_name = lv_label_create(red_container);
    lv_obj_set_style_align(red_name, LV_ALIGN_TOP_MID, LV_PART_MAIN);
    lv_label_set_text(red_name, "R");

    this->red_slider_ = lv_slider_create(red_container);
    lv_obj_add_style(this->red_slider_, &this->slider_vert_, LV_PART_MAIN);
    lv_obj_set_style_bg_grad(this->red_slider_, &this->red_bar_, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(this->red_slider_, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_add_style(this->red_slider_, &this->slider_vert_knob_, LV_PART_KNOB);
    lv_slider_set_range(this->red_slider_, 0, 255);
    lv_slider_set_mode(this->red_slider_, LV_SLIDER_MODE_NORMAL);

    lv_obj_add_flag(this->red_slider_, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    this->red_value_ = lv_label_create(this->red_slider_);
    lv_obj_set_style_align(this->red_value_, LV_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(this->red_value_, lv_color_black(), LV_PART_MAIN);

    auto *green_container = lv_obj_create(rgb_container);
    lv_obj_set_style_border_width(green_container, 0, LV_PART_MAIN);
    lv_obj_set_style_height(green_container, lv_pct(100), LV_PART_MAIN);
    lv_obj_set_style_pad_all(green_container, 0, LV_PART_MAIN);
    lv_obj_set_style_width(green_container, lv_pct(33), LV_PART_MAIN);

    auto *green_name = lv_label_create(green_container);
    lv_obj_set_style_align(green_name, LV_ALIGN_TOP_MID, LV_PART_MAIN);
    lv_label_set_text(green_name, "G");

    this->green_slider_ = lv_slider_create(green_container);
    lv_obj_add_style(this->green_slider_, &this->slider_vert_, LV_PART_MAIN);
    lv_obj_set_style_bg_grad(this->green_slider_, &this->green_bar_, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(this->green_slider_, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_add_style(this->green_slider_, &this->slider_vert_knob_, LV_PART_KNOB);
    lv_slider_set_range(this->green_slider_, 0, 255);
    lv_slider_set_mode(this->green_slider_, LV_SLIDER_MODE_NORMAL);

    lv_obj_add_flag(this->green_slider_, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    this->green_value_ = lv_label_create(this->green_slider_);
    lv_obj_set_style_align(this->green_value_, LV_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(this->green_value_, lv_color_black(), LV_PART_MAIN);

    auto *blue_container = lv_obj_create(rgb_container);
    lv_obj_set_style_border_width(blue_container, 0, LV_PART_MAIN);
    lv_obj_set_style_height(blue_container, lv_pct(100), LV_PART_MAIN);
    lv_obj_set_style_pad_all(blue_container, 0, LV_PART_MAIN);
    lv_obj_set_style_width(blue_container, lv_pct(33), LV_PART_MAIN);

    auto *blue_name = lv_label_create(blue_container);
    lv_obj_set_style_align(blue_name, LV_ALIGN_TOP_MID, LV_PART_MAIN);
    lv_label_set_text(blue_name, "B");

    this->blue_slider_ = lv_slider_create(blue_container);
    lv_obj_add_style(this->blue_slider_, &this->slider_vert_, LV_PART_MAIN);
    lv_obj_set_style_bg_grad(this->blue_slider_, &this->blue_bar_, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(this->blue_slider_, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_add_style(this->blue_slider_, &this->slider_vert_knob_, LV_PART_KNOB);
    lv_slider_set_range(this->blue_slider_, 0, 255);
    lv_slider_set_mode(this->blue_slider_, LV_SLIDER_MODE_NORMAL);

    lv_obj_add_flag(this->blue_slider_, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    this->blue_value_ = lv_label_create(this->blue_slider_);
    lv_obj_set_style_align(this->blue_value_, LV_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(this->blue_value_, lv_color_black(), LV_PART_MAIN);

    auto *saturation_container = lv_obj_create(middle);
    lv_obj_set_style_border_width(saturation_container, 0, LV_PART_MAIN);
    lv_obj_set_style_height(saturation_container, lv_pct(15), LV_PART_MAIN);
    lv_obj_set_style_pad_all(saturation_container, 0, LV_PART_MAIN);
    lv_obj_set_style_width(saturation_container, lv_pct(100), LV_PART_MAIN);

    auto *saturation_name = lv_label_create(saturation_container);
    lv_obj_set_style_align(saturation_name, LV_ALIGN_TOP_MID, LV_PART_MAIN);
    lv_label_set_text(saturation_name, "S");

    this->saturation_slider_ = lv_slider_create(saturation_container);
    lv_obj_add_style(this->saturation_slider_, &this->slider_horz_, LV_PART_MAIN);
    lv_obj_set_style_bg_grad(this->saturation_slider_, &this->saturation_bar_, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(this->saturation_slider_, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_add_style(this->saturation_slider_, &this->slider_horz_knob_, LV_PART_KNOB);
    lv_slider_set_range(this->saturation_slider_, 0, 100);
    lv_slider_set_mode(this->saturation_slider_, LV_SLIDER_MODE_NORMAL);

    lv_obj_add_flag(this->saturation_slider_, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    this->saturation_value_ = lv_label_create(this->saturation_slider_);
    lv_obj_set_style_align(this->saturation_value_, LV_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(this->saturation_value_, lv_color_black(), LV_PART_MAIN);
    this->update_color_();

    auto rgb_lambda = [](lv_event_t *event) {
      auto *self = static_cast<LvColorPickerType *>(lv_event_get_user_data(event));
      self->update_rgb_();
    };
    lv_obj_add_event_cb(this->red_slider_, rgb_lambda, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(this->green_slider_, rgb_lambda, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(this->blue_slider_, rgb_lambda, LV_EVENT_VALUE_CHANGED, this);

    auto hsl_lambda = [](lv_event_t *event) {
      auto *self = static_cast<LvColorPickerType *>(lv_event_get_user_data(event));
      self->update_hsl_();
    };
    lv_obj_add_event_cb(this->hue_slider_, hsl_lambda, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(this->saturation_slider_, hsl_lambda, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(this->brightness_slider_, hsl_lambda, LV_EVENT_VALUE_CHANGED, this);
  }

 protected:
  void update_text_() const {
    // max 8 bytes: "#" + 3x "%02X" (2 hex digits each) + null
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X", lv_slider_get_value(this->red_slider_),
             lv_slider_get_value(this->green_slider_), lv_slider_get_value(this->blue_slider_));
    lv_label_set_text(this->color_text_, buf);
  }

  void update_saturation_bar_(uint16_t hue) {
    this->saturation_bar_.stops[1].color = lv_color_hsv_to_rgb(hue, 100, 100);
    lv_obj_invalidate(this->saturation_slider_);
  }

  void update_values_() const {
    // max 4 bytes: up to 3 digits (0..360) + null
    char buf[4];
    snprintf(buf, sizeof(buf), "%d", lv_slider_get_value(this->hue_slider_));
    lv_label_set_text(this->hue_value_, buf);
    snprintf(buf, sizeof(buf), "%d", lv_slider_get_value(this->saturation_slider_));
    lv_label_set_text(this->saturation_value_, buf);
    snprintf(buf, sizeof(buf), "%d", lv_slider_get_value(this->brightness_slider_));
    lv_label_set_text(this->brightness_value_, buf);
    snprintf(buf, sizeof(buf), "%d", lv_slider_get_value(this->red_slider_));
    lv_label_set_text(this->red_value_, buf);
    snprintf(buf, sizeof(buf), "%d", lv_slider_get_value(this->green_slider_));
    lv_label_set_text(this->green_value_, buf);
    snprintf(buf, sizeof(buf), "%d", lv_slider_get_value(this->blue_slider_));
    lv_label_set_text(this->blue_value_, buf);
  }

  void update_hsl_() {
    auto hue = lv_slider_get_value(this->hue_slider_);
    auto brightness = lv_slider_get_value(this->brightness_slider_);
    auto saturation = lv_slider_get_value(this->saturation_slider_);
    this->color_ = lv_color_hsv_to_rgb(hue, saturation, brightness);
    lv_obj_set_style_bg_color(this->color_indicator_, this->color_, LV_PART_MAIN);
    lv_color32_t c32;
    c32 = lv_color_to_32(this->color_, LV_OPA_COVER);
    lv_slider_set_value(this->red_slider_, c32.red, LV_ANIM_OFF);
    lv_slider_set_value(this->green_slider_, c32.green, LV_ANIM_OFF);
    lv_slider_set_value(this->blue_slider_, c32.blue, LV_ANIM_OFF);
    this->update_saturation_bar_(hue);
    this->update_text_();
    this->update_values_();
  }

  void update_rgb_() {
    auto red = lv_slider_get_value(this->red_slider_);
    auto green = lv_slider_get_value(this->green_slider_);
    auto blue = lv_slider_get_value(this->blue_slider_);
    this->color_ = lv_color_make(red, green, blue);
    lv_obj_set_style_bg_color(this->color_indicator_, this->color_, LV_PART_MAIN);
    auto hsv = lv_color_rgb_to_hsv(red, green, blue);
    lv_slider_set_value(this->hue_slider_, hsv.h, LV_ANIM_OFF);
    lv_slider_set_value(this->saturation_slider_, hsv.s, LV_ANIM_OFF);
    lv_slider_set_value(this->brightness_slider_, hsv.v, LV_ANIM_OFF);
    this->update_saturation_bar_(hsv.h);
    this->update_text_();
    this->update_values_();
  }

  void update_color_() {
    lv_color32_t c32;
    c32 = lv_color_to_32(this->color_, LV_OPA_COVER);
    lv_slider_set_value(this->red_slider_, c32.red, LV_ANIM_OFF);
    lv_slider_set_value(this->green_slider_, c32.green, LV_ANIM_OFF);
    lv_slider_set_value(this->blue_slider_, c32.blue, LV_ANIM_OFF);
    this->update_rgb_();
    lv_obj_invalidate(this->obj);
  }

  lv_grad_dsc_t color_bar_{};
  lv_grad_dsc_t brightness_bar_{};
  lv_grad_dsc_t saturation_bar_{};
  lv_grad_dsc_t blue_bar_{};
  lv_grad_dsc_t green_bar_{};
  lv_grad_dsc_t red_bar_{};
  lv_style_t slider_vert_{};
  lv_style_t slider_vert_knob_{};
  lv_style_t slider_horz_{};
  lv_style_t slider_horz_knob_{};
  lv_obj_t *hue_slider_{};
  lv_obj_t *brightness_slider_{};
  lv_obj_t *saturation_slider_{};
  lv_obj_t *red_slider_{};
  lv_obj_t *blue_slider_{};
  lv_obj_t *green_slider_{};
  lv_obj_t *hue_value_{};
  lv_obj_t *brightness_value_{};
  lv_obj_t *saturation_value_{};
  lv_obj_t *red_value_{};
  lv_obj_t *blue_value_{};
  lv_obj_t *green_value_{};
  lv_obj_t *color_indicator_{};
  lv_obj_t *color_text_{};
  lv_color_t color_{lv_color_hex(0x808080)};
};

}  // namespace esphome::lvgl

#endif  // USE_LVGL_COLOR_PICKER
