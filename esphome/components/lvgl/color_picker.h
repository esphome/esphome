#pragma once

#include "lvgl_esphome.h"
#include "esphome/core/color.h"

#include <array>

#ifdef USE_LVGL_COLOR_PICKER

namespace esphome::lvgl {

class LvColorPickerType : public LvCompound {
  constexpr static const char *const TAG = "lvgl.color_picker";

 public:
  // The sliders the widget is built from, in the order they appear in `sliders_`.
  enum SliderIndex : size_t {
    SLIDER_HUE,
    SLIDER_SATURATION,
    SLIDER_BRIGHTNESS,
    SLIDER_RED,
    SLIDER_GREEN,
    SLIDER_BLUE,
    SLIDER_COUNT,
  };

  // The colour currently shown, kept up to date as the sliders move. Converts to lv_color_t
  // on its own, so it can be handed straight to LVGL calls as well as read component-wise.
  Color state{0x80, 0x80, 0x80};

  // The widget has no knob or bar of its own, so styles configured for its `items` and
  // `knob` parts are applied to each slider in turn instead. Index must be less than
  // SLIDER_COUNT.
  lv_obj_t *get_slider(size_t index) const { return this->sliders_[index]; }

  // Stops the knobs being tinted with the colour their slider currently shows, for when a
  // background colour has been configured for them instead.
  void set_tint_knobs(bool tint_knobs) { this->tint_knobs_ = tint_knobs; }

  void set_color(lv_color_t color) {
    lv_color32_t c32 = lv_color_to_32(color, LV_OPA_COVER);
    this->state = Color(c32.red, c32.green, c32.blue);
    if (this->obj != nullptr)
      this->update_color_();
  }
  Color get_color() const { return this->state; }
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
    lv_style_set_width(&this->slider_vert_, lv_pct(50));
    lv_style_init(&this->slider_vert_knob_);
    lv_style_set_bg_color(&this->slider_vert_knob_, lv_color_hex(0xFFFFFF));
    lv_style_set_border_width(&this->slider_vert_knob_, 1);
    lv_style_set_outline_color(&this->slider_vert_knob_, lv_color_hex(0x00));
    lv_style_set_pad_bottom(&this->slider_vert_knob_, -KNOB_TRIM);
    lv_style_set_pad_top(&this->slider_vert_knob_, -KNOB_TRIM);
    lv_style_set_pad_left(&this->slider_vert_knob_, KNOB_OVERHANG);
    lv_style_set_pad_right(&this->slider_vert_knob_, KNOB_OVERHANG);
    lv_style_set_radius(&this->slider_vert_knob_, 0);
    lv_style_init(&this->slider_horz_);
    lv_style_set_align(&this->slider_horz_, LV_ALIGN_CENTER);
    lv_style_set_bg_opa(&this->slider_horz_, LV_OPA_COVER);
    // LVGL sizes a slider's knob from the thickness of its bar, so the horizontal bars are
    // given the same thickness as the vertical ones to keep every knob the same size. The
    // vertical bars are half the width of a container that is 15% of the widget, and this
    // container is 18% of its height, hence the proportion here.
    lv_style_set_height(&this->slider_horz_, lv_pct(45));
    lv_style_set_pad_all(&this->slider_horz_, 0);
    lv_style_set_radius(&this->slider_horz_, 0);
    lv_style_set_width(&this->slider_horz_, lv_pct(90));
    lv_style_init(&this->slider_horz_knob_);
    lv_style_set_bg_color(&this->slider_horz_knob_, lv_color_hex(0xFFFFFF));
    lv_style_set_border_width(&this->slider_horz_knob_, 1);
    lv_style_set_outline_color(&this->slider_horz_knob_, lv_color_hex(0x00));
    lv_style_set_pad_left(&this->slider_horz_knob_, -KNOB_TRIM);
    lv_style_set_pad_right(&this->slider_horz_knob_, -KNOB_TRIM);
    lv_style_set_pad_top(&this->slider_horz_knob_, KNOB_OVERHANG);
    lv_style_set_pad_bottom(&this->slider_horz_knob_, KNOB_OVERHANG);
    lv_style_set_radius(&this->slider_horz_knob_, 0);

    // The outer object deliberately has no layout: every child is sized as a percentage of
    // it, and LVGL only ignores such children when working out a LV_SIZE_CONTENT parent's
    // size if that parent is not laid out. Combined with the LV_EVENT_GET_SELF_SIZE handler
    // below, that lets the widget report its own preferred size like any other widget does.
    lv_obj_add_event_cb(outer, self_size_cb, LV_EVENT_GET_SELF_SIZE, nullptr);
    // Inset the contents so that a slider knob at the end of its travel does not sit hard
    // against the widget border.
    lv_obj_set_style_pad_all(outer, KNOB_OVERHANG, LV_PART_MAIN);

    auto *hue_container = lv_obj_create(outer);
    this->init_slider_container_(hue_container, "Hue", true);
    lv_obj_set_style_align(hue_container, LV_ALIGN_LEFT_MID, LV_PART_MAIN);
    lv_obj_set_style_height(hue_container, lv_pct(100), LV_PART_MAIN);
    lv_obj_set_style_width(hue_container, lv_pct(15), LV_PART_MAIN);

    this->sliders_[SLIDER_HUE] = lv_slider_create(hue_container);
    lv_obj_add_style(this->sliders_[SLIDER_HUE], &this->slider_vert_, LV_PART_MAIN);
    lv_obj_set_style_bg_grad(this->sliders_[SLIDER_HUE], &this->color_bar_, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(this->sliders_[SLIDER_HUE], LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_add_style(this->sliders_[SLIDER_HUE], &this->slider_vert_knob_, LV_PART_KNOB);
    lv_obj_set_flex_grow(this->sliders_[SLIDER_HUE], 1);
    lv_slider_set_range(this->sliders_[SLIDER_HUE], 0, 360);
    lv_slider_set_mode(this->sliders_[SLIDER_HUE], LV_SLIDER_MODE_NORMAL);

    auto *middle = lv_obj_create(outer);
    lv_obj_set_style_align(middle, LV_ALIGN_RIGHT_MID, LV_PART_MAIN);
    lv_obj_set_layout(middle, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(middle, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(middle, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_border_width(middle, 0, LV_PART_MAIN);
    lv_obj_set_style_height(middle, lv_pct(100), LV_PART_MAIN);
    lv_obj_set_style_pad_all(middle, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(middle, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(middle, 0, LV_PART_MAIN);
    lv_obj_set_style_width(middle, lv_pct(85), LV_PART_MAIN);

    auto *brightness_container = lv_obj_create(middle);
    this->init_slider_container_(brightness_container, "Brightness", false);
    lv_obj_set_style_height(brightness_container, lv_pct(18), LV_PART_MAIN);
    lv_obj_set_style_width(brightness_container, lv_pct(100), LV_PART_MAIN);

    this->sliders_[SLIDER_BRIGHTNESS] = lv_slider_create(brightness_container);
    lv_obj_add_style(this->sliders_[SLIDER_BRIGHTNESS], &this->slider_horz_, LV_PART_MAIN);
    lv_obj_set_style_bg_grad(this->sliders_[SLIDER_BRIGHTNESS], &this->brightness_bar_, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(this->sliders_[SLIDER_BRIGHTNESS], LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_add_style(this->sliders_[SLIDER_BRIGHTNESS], &this->slider_horz_knob_, LV_PART_KNOB);
    lv_slider_set_range(this->sliders_[SLIDER_BRIGHTNESS], 0, 100);
    lv_slider_set_mode(this->sliders_[SLIDER_BRIGHTNESS], LV_SLIDER_MODE_NORMAL);

    lv_obj_add_flag(this->sliders_[SLIDER_BRIGHTNESS], LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    this->brightness_value_ = lv_label_create(this->sliders_[SLIDER_BRIGHTNESS]);
    lv_obj_set_style_align(this->brightness_value_, LV_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(this->brightness_value_, lv_color_white(), LV_PART_MAIN);

    auto *inner = lv_obj_create(middle);
    lv_obj_set_layout(inner, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(inner, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(inner, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_border_width(inner, 0, LV_PART_MAIN);
    lv_obj_set_style_height(inner, lv_pct(58), LV_PART_MAIN);
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
    lv_obj_set_flex_align(rgb_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_border_width(rgb_container, 0, LV_PART_MAIN);
    lv_obj_set_style_height(rgb_container, lv_pct(100), LV_PART_MAIN);
    lv_obj_set_style_pad_all(rgb_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(rgb_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(rgb_container, 0, LV_PART_MAIN);
    lv_obj_set_style_width(rgb_container, lv_pct(57), LV_PART_MAIN);

    auto *red_container = lv_obj_create(rgb_container);
    this->init_slider_container_(red_container, "R", true);
    lv_obj_set_style_height(red_container, lv_pct(100), LV_PART_MAIN);
    lv_obj_set_style_width(red_container, lv_pct(33), LV_PART_MAIN);

    this->sliders_[SLIDER_RED] = lv_slider_create(red_container);
    lv_obj_add_style(this->sliders_[SLIDER_RED], &this->slider_vert_, LV_PART_MAIN);
    lv_obj_set_style_bg_grad(this->sliders_[SLIDER_RED], &this->red_bar_, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(this->sliders_[SLIDER_RED], LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_add_style(this->sliders_[SLIDER_RED], &this->slider_vert_knob_, LV_PART_KNOB);
    lv_obj_set_flex_grow(this->sliders_[SLIDER_RED], 1);
    lv_slider_set_range(this->sliders_[SLIDER_RED], 0, 255);
    lv_slider_set_mode(this->sliders_[SLIDER_RED], LV_SLIDER_MODE_NORMAL);

    lv_obj_add_flag(this->sliders_[SLIDER_RED], LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    this->red_value_ = lv_label_create(this->sliders_[SLIDER_RED]);
    lv_obj_set_style_align(this->red_value_, LV_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(this->red_value_, lv_color_black(), LV_PART_MAIN);

    auto *green_container = lv_obj_create(rgb_container);
    this->init_slider_container_(green_container, "G", true);
    lv_obj_set_style_height(green_container, lv_pct(100), LV_PART_MAIN);
    lv_obj_set_style_width(green_container, lv_pct(33), LV_PART_MAIN);

    this->sliders_[SLIDER_GREEN] = lv_slider_create(green_container);
    lv_obj_add_style(this->sliders_[SLIDER_GREEN], &this->slider_vert_, LV_PART_MAIN);
    lv_obj_set_style_bg_grad(this->sliders_[SLIDER_GREEN], &this->green_bar_, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(this->sliders_[SLIDER_GREEN], LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_add_style(this->sliders_[SLIDER_GREEN], &this->slider_vert_knob_, LV_PART_KNOB);
    lv_obj_set_flex_grow(this->sliders_[SLIDER_GREEN], 1);
    lv_slider_set_range(this->sliders_[SLIDER_GREEN], 0, 255);
    lv_slider_set_mode(this->sliders_[SLIDER_GREEN], LV_SLIDER_MODE_NORMAL);

    lv_obj_add_flag(this->sliders_[SLIDER_GREEN], LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    this->green_value_ = lv_label_create(this->sliders_[SLIDER_GREEN]);
    lv_obj_set_style_align(this->green_value_, LV_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(this->green_value_, lv_color_black(), LV_PART_MAIN);

    auto *blue_container = lv_obj_create(rgb_container);
    this->init_slider_container_(blue_container, "B", true);
    lv_obj_set_style_height(blue_container, lv_pct(100), LV_PART_MAIN);
    lv_obj_set_style_width(blue_container, lv_pct(33), LV_PART_MAIN);

    this->sliders_[SLIDER_BLUE] = lv_slider_create(blue_container);
    lv_obj_add_style(this->sliders_[SLIDER_BLUE], &this->slider_vert_, LV_PART_MAIN);
    lv_obj_set_style_bg_grad(this->sliders_[SLIDER_BLUE], &this->blue_bar_, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(this->sliders_[SLIDER_BLUE], LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_add_style(this->sliders_[SLIDER_BLUE], &this->slider_vert_knob_, LV_PART_KNOB);
    lv_obj_set_flex_grow(this->sliders_[SLIDER_BLUE], 1);
    lv_slider_set_range(this->sliders_[SLIDER_BLUE], 0, 255);
    lv_slider_set_mode(this->sliders_[SLIDER_BLUE], LV_SLIDER_MODE_NORMAL);

    lv_obj_add_flag(this->sliders_[SLIDER_BLUE], LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    this->blue_value_ = lv_label_create(this->sliders_[SLIDER_BLUE]);
    lv_obj_set_style_align(this->blue_value_, LV_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(this->blue_value_, lv_color_black(), LV_PART_MAIN);

    auto *saturation_container = lv_obj_create(middle);
    this->init_slider_container_(saturation_container, "Saturation", false);
    lv_obj_set_style_height(saturation_container, lv_pct(18), LV_PART_MAIN);
    lv_obj_set_style_width(saturation_container, lv_pct(100), LV_PART_MAIN);

    this->sliders_[SLIDER_SATURATION] = lv_slider_create(saturation_container);
    lv_obj_add_style(this->sliders_[SLIDER_SATURATION], &this->slider_horz_, LV_PART_MAIN);
    lv_obj_set_style_bg_grad(this->sliders_[SLIDER_SATURATION], &this->saturation_bar_, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(this->sliders_[SLIDER_SATURATION], LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_add_style(this->sliders_[SLIDER_SATURATION], &this->slider_horz_knob_, LV_PART_KNOB);
    lv_slider_set_range(this->sliders_[SLIDER_SATURATION], 0, 100);
    lv_slider_set_mode(this->sliders_[SLIDER_SATURATION], LV_SLIDER_MODE_NORMAL);

    lv_obj_add_flag(this->sliders_[SLIDER_SATURATION], LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    this->saturation_value_ = lv_label_create(this->sliders_[SLIDER_SATURATION]);
    lv_obj_set_style_align(this->saturation_value_, LV_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(this->saturation_value_, lv_color_black(), LV_PART_MAIN);
    this->update_color_();

    auto rgb_lambda = [](lv_event_t *event) {
      auto *self = static_cast<LvColorPickerType *>(lv_event_get_user_data(event));
      self->update_rgb_();
    };
    lv_obj_add_event_cb(this->sliders_[SLIDER_RED], rgb_lambda, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(this->sliders_[SLIDER_GREEN], rgb_lambda, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(this->sliders_[SLIDER_BLUE], rgb_lambda, LV_EVENT_VALUE_CHANGED, this);

    auto hsl_lambda = [](lv_event_t *event) {
      auto *self = static_cast<LvColorPickerType *>(lv_event_get_user_data(event));
      self->update_hsl_();
    };
    lv_obj_add_event_cb(this->sliders_[SLIDER_HUE], hsl_lambda, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(this->sliders_[SLIDER_SATURATION], hsl_lambda, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(this->sliders_[SLIDER_BRIGHTNESS], hsl_lambda, LV_EVENT_VALUE_CHANGED, this);

    bubble_events(outer);
  }

 protected:
  // The widget needs room for a gradient bar, its knob and a label either side of it, so its
  // preferred size scales with how tall a line of the current text font is. It is square, so
  // the same figure serves for both width and height.
  static constexpr int SELF_SIZE_FONT_MULTIPLE = 16;

  // A slider knob is centred on the current value and is as thick as the bar is wide, so at
  // either end of its travel half of it lies outside the bar. KNOB_TRIM pulls it back a
  // little along that direction, and KNOB_OVERHANG makes it stand out across the bar so it
  // is easy to see. Both axes are set explicitly, otherwise the theme supplies whatever the
  // style leaves out and the knob ends up a size the containers have not reserved room for.
  static constexpr int KNOB_TRIM = 4;
  static constexpr int KNOB_OVERHANG = 4;

  // Prepares a container holding a name label above a slider. A column layout is used so
  // that the label always keeps its own row and the slider fills whatever is left, rather
  // than the two overlapping when the text font is large relative to the widget.
  void init_slider_container_(lv_obj_t *container, const char *name, bool vertical) {
    lv_obj_set_layout(container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(container, 0, LV_PART_MAIN);
    if (vertical) {
      // The knob travels the length of the bar, so how far it reaches past either end
      // depends on the bar's width, which is only known once the widget has been laid out.
      update_knob_clearance(container);
      lv_obj_add_event_cb(container, knob_clearance_cb, LV_EVENT_STYLE_CHANGED, nullptr);
    } else {
      // The knob spans the bar's height and stands out by a fixed amount above and below it.
      lv_obj_set_style_pad_bottom(container, KNOB_OVERHANG, LV_PART_MAIN);
      lv_obj_set_style_pad_row(container, KNOB_OVERHANG, LV_PART_MAIN);
    }

    auto *label = lv_label_create(container);
    lv_label_set_text(label, name);
  }

  // Reserves room above and below a vertical slider for the half of the knob that hangs off
  // the end of the bar. Half a line of text covers that at the proportions this widget uses,
  // and re-deriving it on a style change keeps it right when the font is changed later.
  static void update_knob_clearance(lv_obj_t *container) {
    const auto *font = lv_obj_get_style_text_font(container, LV_PART_MAIN);
    auto clearance = static_cast<int32_t>(lv_font_get_line_height(font)) / 2;
    if (clearance == lv_obj_get_style_pad_bottom(container, LV_PART_MAIN))
      return;
    lv_obj_set_style_pad_bottom(container, clearance, LV_PART_MAIN);
    lv_obj_set_style_pad_row(container, clearance, LV_PART_MAIN);
  }

  static void knob_clearance_cb(lv_event_t *event) { update_knob_clearance(lv_event_get_current_target_obj(event)); }

  // Answers LV_SIZE_CONTENT on behalf of the widget. LVGL asks again whenever the font
  // changes, since the text font is flagged as affecting layout.
  static void self_size_cb(lv_event_t *event) {
    auto *obj = lv_event_get_current_target_obj(event);
    const auto *font = lv_obj_get_style_text_font(obj, LV_PART_MAIN);
    auto self_size = static_cast<int32_t>(lv_font_get_line_height(font)) * SELF_SIZE_FONT_MULTIPLE;
    auto *point = static_cast<lv_point_t *>(lv_event_get_param(event));
    point->x = self_size;
    point->y = self_size;
  }

  void update_text_() const {
    // max 8 bytes: "#" + 3x "%02X" (2 hex digits each) + null
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X", static_cast<unsigned>(lv_slider_get_value(this->sliders_[SLIDER_RED])),
             static_cast<unsigned>(lv_slider_get_value(this->sliders_[SLIDER_GREEN])),
             static_cast<unsigned>(lv_slider_get_value(this->sliders_[SLIDER_BLUE])));
    lv_label_set_text(this->color_text_, buf);
  }

  void update_saturation_bar_(uint16_t hue) {
    this->saturation_bar_.stops[1].color = lv_color_hsv_to_rgb(hue, 100, 100);
    lv_obj_invalidate(this->sliders_[SLIDER_SATURATION]);
  }

  void update_values_() const {
    // max 4 bytes: up to 3 digits (0..255) + null
    char buf[4];
    snprintf(buf, sizeof(buf), "%d", (int) lv_slider_get_value(this->sliders_[SLIDER_SATURATION]));
    lv_label_set_text(this->saturation_value_, buf);
    snprintf(buf, sizeof(buf), "%d", (int) lv_slider_get_value(this->sliders_[SLIDER_BRIGHTNESS]));
    lv_label_set_text(this->brightness_value_, buf);
    snprintf(buf, sizeof(buf), "%d", (int) lv_slider_get_value(this->sliders_[SLIDER_RED]));
    lv_label_set_text(this->red_value_, buf);
    snprintf(buf, sizeof(buf), "%d", (int) lv_slider_get_value(this->sliders_[SLIDER_GREEN]));
    lv_label_set_text(this->green_value_, buf);
    snprintf(buf, sizeof(buf), "%d", (int) lv_slider_get_value(this->sliders_[SLIDER_BLUE]));
    lv_label_set_text(this->blue_value_, buf);
  }

  // Tints each knob with the colour its own bar shows at the current value, so a knob reads
  // as a sample of the gradient it sits on rather than a plain white block. The red, green
  // and blue bars each run from white up to their full primary, so their knob fades that one
  // channel in while the other two drop away.
  void update_knobs_() const {
    if (!this->tint_knobs_)
      return;
    auto hue = static_cast<uint16_t>(lv_slider_get_value(this->sliders_[SLIDER_HUE]));
    auto saturation = static_cast<uint8_t>(lv_slider_get_value(this->sliders_[SLIDER_SATURATION]));
    auto brightness = static_cast<uint8_t>(lv_slider_get_value(this->sliders_[SLIDER_BRIGHTNESS]));
    auto red = static_cast<uint8_t>(lv_slider_get_value(this->sliders_[SLIDER_RED]));
    auto green = static_cast<uint8_t>(lv_slider_get_value(this->sliders_[SLIDER_GREEN]));
    auto blue = static_cast<uint8_t>(lv_slider_get_value(this->sliders_[SLIDER_BLUE]));
    auto grey = static_cast<uint8_t>(brightness * 255 / 100);
    lv_obj_set_style_bg_color(this->sliders_[SLIDER_HUE], lv_color_hsv_to_rgb(hue, 100, 100), LV_PART_KNOB);
    lv_obj_set_style_bg_color(this->sliders_[SLIDER_SATURATION], lv_color_hsv_to_rgb(hue, saturation, 100),
                              LV_PART_KNOB);
    lv_obj_set_style_bg_color(this->sliders_[SLIDER_BRIGHTNESS], lv_color_make(grey, grey, grey), LV_PART_KNOB);
    lv_obj_set_style_bg_color(this->sliders_[SLIDER_RED], lv_color_make(255, 255 - red, 255 - red), LV_PART_KNOB);
    lv_obj_set_style_bg_color(this->sliders_[SLIDER_GREEN], lv_color_make(255 - green, 255, 255 - green), LV_PART_KNOB);
    lv_obj_set_style_bg_color(this->sliders_[SLIDER_BLUE], lv_color_make(255 - blue, 255 - blue, 255), LV_PART_KNOB);
  }

  // The sliders are internal to the widget, so touching one is an event on the slider rather
  // than on the widget, and nothing outside would see it. Passing events up the tree makes
  // them arrive at the widget, where `on_value`, `on_release` and the rest are listening.
  // LVGL only passes an event on if every object between the two carries the flag, so it
  // goes on all of them. The widget itself is left without it, so events stop there instead
  // of carrying on to whatever contains it.
  static void bubble_events(lv_obj_t *obj) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) {
      auto *child = lv_obj_get_child(obj, i);
      lv_obj_add_flag(child, LV_OBJ_FLAG_EVENT_BUBBLE);
      bubble_events(child);
    }
  }

  void update_hsl_() {
    auto hue = lv_slider_get_value(this->sliders_[SLIDER_HUE]);
    auto brightness = lv_slider_get_value(this->sliders_[SLIDER_BRIGHTNESS]);
    auto saturation = lv_slider_get_value(this->sliders_[SLIDER_SATURATION]);
    lv_color32_t c32 = lv_color_to_32(lv_color_hsv_to_rgb(hue, saturation, brightness), LV_OPA_COVER);
    this->state = Color(c32.red, c32.green, c32.blue);
    lv_obj_set_style_bg_color(this->color_indicator_, this->state, LV_PART_MAIN);
    lv_slider_set_value(this->sliders_[SLIDER_RED], c32.red, LV_ANIM_OFF);
    lv_slider_set_value(this->sliders_[SLIDER_GREEN], c32.green, LV_ANIM_OFF);
    lv_slider_set_value(this->sliders_[SLIDER_BLUE], c32.blue, LV_ANIM_OFF);
    this->update_saturation_bar_(hue);
    this->update_text_();
    this->update_values_();
    this->update_knobs_();
  }

  void update_rgb_() {
    auto red = lv_slider_get_value(this->sliders_[SLIDER_RED]);
    auto green = lv_slider_get_value(this->sliders_[SLIDER_GREEN]);
    auto blue = lv_slider_get_value(this->sliders_[SLIDER_BLUE]);
    this->state = Color(static_cast<uint8_t>(red), static_cast<uint8_t>(green), static_cast<uint8_t>(blue));
    lv_obj_set_style_bg_color(this->color_indicator_, this->state, LV_PART_MAIN);
    auto hsv = lv_color_rgb_to_hsv(red, green, blue);
    lv_slider_set_value(this->sliders_[SLIDER_HUE], hsv.h, LV_ANIM_OFF);
    lv_slider_set_value(this->sliders_[SLIDER_SATURATION], hsv.s, LV_ANIM_OFF);
    lv_slider_set_value(this->sliders_[SLIDER_BRIGHTNESS], hsv.v, LV_ANIM_OFF);
    this->update_saturation_bar_(hsv.h);
    this->update_text_();
    this->update_values_();
    this->update_knobs_();
  }

  void update_color_() {
    lv_slider_set_value(this->sliders_[SLIDER_RED], this->state.r, LV_ANIM_OFF);
    lv_slider_set_value(this->sliders_[SLIDER_GREEN], this->state.g, LV_ANIM_OFF);
    lv_slider_set_value(this->sliders_[SLIDER_BLUE], this->state.b, LV_ANIM_OFF);
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
  std::array<lv_obj_t *, SLIDER_COUNT> sliders_{};
  lv_obj_t *brightness_value_{};
  lv_obj_t *saturation_value_{};
  lv_obj_t *red_value_{};
  lv_obj_t *blue_value_{};
  lv_obj_t *green_value_{};
  lv_obj_t *color_indicator_{};
  lv_obj_t *color_text_{};
  bool tint_knobs_{true};
};

}  // namespace esphome::lvgl

#endif  // USE_LVGL_COLOR_PICKER
