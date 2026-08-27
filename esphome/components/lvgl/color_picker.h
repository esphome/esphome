#pragma once

#include "lvgl_esphome.h"
#include "esphome/core/color.h"

#include <algorithm>
#include <array>
#include <cmath>

#ifdef USE_LVGL_COLOR_PICKER

namespace esphome::lvgl {

// The hue bar runs through all six primaries and back to red, so it needs seven stops in the
// fixed-size array LVGL gives a gradient. `gradient.py` raises the limit to that when a colour
// picker is in use; this makes an under-sized one a build error rather than a stray write.
static_assert(LV_GRADIENT_MAX_STOPS >= 7, "color_picker needs LV_GRADIENT_MAX_STOPS >= 7");

class LvColorPickerType : public LvCompound {
 public:
  // The sliders the widget can be built from, in the order they appear in `sliders_`.
  enum SliderIndex : size_t {
    SLIDER_HUE,
    SLIDER_SATURATION,
    SLIDER_BRIGHTNESS,
    SLIDER_RED,
    SLIDER_GREEN,
    SLIDER_BLUE,
    SLIDER_COUNT,
  };

  // One bit per slider, for choosing which of them a widget is given.
  enum SliderFlag : uint8_t {
    SLIDER_FLAG_HUE = 1 << SLIDER_HUE,
    SLIDER_FLAG_SATURATION = 1 << SLIDER_SATURATION,
    SLIDER_FLAG_BRIGHTNESS = 1 << SLIDER_BRIGHTNESS,
    SLIDER_FLAG_RED = 1 << SLIDER_RED,
    SLIDER_FLAG_GREEN = 1 << SLIDER_GREEN,
    SLIDER_FLAG_BLUE = 1 << SLIDER_BLUE,
  };

  // The sliders to build, and whether to use a colour wheel in place of the hue and
  // saturation ones, are fixed for the life of the widget, since the layout is worked out
  // from them, so they are given here rather than through a setter.
  explicit LvColorPickerType(uint8_t sliders, bool use_wheel) : sliders_mask_(sliders), use_wheel_(use_wheel) {}

  // The widget has no knob or bar of its own, so styles configured for its `items` and
  // `knob` parts are applied to each slider in turn instead. Returns null for a slider the
  // widget was not built with. Index must be less than SLIDER_COUNT.
  lv_obj_t *get_slider(size_t index) const { return this->sliders_[index]; }

  // Stops the knobs being tinted with the colour their slider currently shows, for when a
  // background colour has been configured for them instead.
  void set_tint_knobs(bool tint_knobs) { this->tint_knobs_ = tint_knobs; }

  void set_color(lv_color_t color) {
    lv_color32_t c32 = lv_color_to_32(color, LV_OPA_COVER);
    this->state_ = Color(c32.red, c32.green, c32.blue);
    if (this->obj != nullptr)
      this->update_color_();
  }
  Color get_color() const { return this->state_; }
  void set_obj(lv_obj_t *outer) override {
    LvCompound::set_obj(outer);
    init_shared();
    this->saturation_bar_.dir = LV_GRAD_DIR_HOR;
    this->saturation_bar_.stops_count = 2;
    this->saturation_bar_.stops[0].color = lv_color_hex(0xFFFFFF);
    this->saturation_bar_.stops[0].opa = LV_OPA_COVER;
    this->saturation_bar_.stops[0].frac = 0;
    this->saturation_bar_.stops[1].color = lv_color_hex(0xFF0000);
    this->saturation_bar_.stops[1].opa = LV_OPA_COVER;
    this->saturation_bar_.stops[1].frac = 255;
    // The far end tracks the current hue too, the same as the saturation bar above, so the
    // brightness bar always shows a fade from black to the colour actually being adjusted -
    // in slider mode as much as with the wheel, hence this is not behind `use_wheel_`.
    this->brightness_bar_.dir = LV_GRAD_DIR_HOR;
    this->brightness_bar_.stops_count = 2;
    this->brightness_bar_.stops[0].color = lv_color_hex(0x00);
    this->brightness_bar_.stops[0].opa = LV_OPA_COVER;
    this->brightness_bar_.stops[0].frac = 0;
    this->brightness_bar_.stops[1].color = lv_color_hex(0xFFFFFF);
    this->brightness_bar_.stops[1].opa = LV_OPA_COVER;
    this->brightness_bar_.stops[1].frac = 255;

    // How much room each part of the layout gets depends on which sliders were asked for: the
    // hue bar takes a column down the left, the saturation and brightness bars a row each,
    // and the red, green and blue bars share the space beside the colour swatch.
    auto rows = static_cast<int32_t>(this->enabled_(SLIDER_SATURATION)) +
                static_cast<int32_t>(this->enabled_(SLIDER_BRIGHTNESS));
    auto channels = static_cast<int32_t>(this->enabled_(SLIDER_RED)) +
                    static_cast<int32_t>(this->enabled_(SLIDER_GREEN)) +
                    static_cast<int32_t>(this->enabled_(SLIDER_BLUE));

    // The outer object deliberately has no layout: every child is sized as a percentage of
    // it, and LVGL only ignores such children when working out a LV_SIZE_CONTENT parent's
    // size if that parent is not laid out. Combined with the LV_EVENT_GET_SELF_SIZE handler
    // below, that lets the widget report its own preferred size like any other widget does.
    lv_obj_add_event_cb(outer, self_size_cb, LV_EVENT_GET_SELF_SIZE, nullptr);
    // Inset the contents so that a slider knob at the end of its travel does not sit hard
    // against the widget border.
    lv_obj_set_style_pad_all(outer, KNOB_OVERHANG, LV_PART_MAIN);

    if (this->enabled_(SLIDER_HUE)) {
      auto *hue_container = this->create_slider_(outer, SLIDER_HUE, "Hue", true, &color_bar, 360);
      lv_obj_set_style_align(hue_container, LV_ALIGN_LEFT_MID, LV_PART_MAIN);
      lv_obj_set_style_height(hue_container, lv_pct(100), LV_PART_MAIN);
      lv_obj_set_style_width(hue_container, lv_pct(HUE_WIDTH_PCT), LV_PART_MAIN);
    } else if (this->use_wheel_) {
      auto *wheel_container = this->create_wheel_(outer);
      lv_obj_set_style_align(wheel_container, LV_ALIGN_LEFT_MID, LV_PART_MAIN);
      lv_obj_set_style_height(wheel_container, lv_pct(WHEEL_WIDTH_PCT), LV_PART_MAIN);
      lv_obj_set_style_width(wheel_container, lv_pct(WHEEL_WIDTH_PCT), LV_PART_MAIN);
    }

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
    auto left_column_width = this->enabled_(SLIDER_HUE) ? HUE_WIDTH_PCT : (this->use_wheel_ ? WHEEL_WIDTH_PCT : 0);
    lv_obj_set_style_width(middle, lv_pct(100 - left_column_width), LV_PART_MAIN);

    if (this->enabled_(SLIDER_BRIGHTNESS)) {
      auto *brightness_container =
          this->create_slider_(middle, SLIDER_BRIGHTNESS, "Brightness", false, &this->brightness_bar_, 100);
      lv_obj_set_style_height(brightness_container, lv_pct(ROW_HEIGHT_PCT), LV_PART_MAIN);
      lv_obj_set_style_width(brightness_container, lv_pct(100), LV_PART_MAIN);
      // A dark bar needs light text, unlike every other value label.
      this->add_value_label_(SLIDER_BRIGHTNESS, lv_color_white());
    }

    auto *inner = lv_obj_create(middle);
    lv_obj_set_layout(inner, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(inner, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(inner, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_border_width(inner, 0, LV_PART_MAIN);
    lv_obj_set_style_height(inner, lv_pct(CONTENT_HEIGHT_PCT - ROW_HEIGHT_PCT * rows), LV_PART_MAIN);
    lv_obj_set_style_pad_all(inner, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(inner, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(inner, 0, LV_PART_MAIN);
    lv_obj_set_style_width(inner, lv_pct(100), LV_PART_MAIN);

    // The channel bars keep a fixed width each, so dropping one widens the swatch beside them.
    auto channels_width = CHANNEL_WIDTH_PCT * channels;

    auto *indicator_container = lv_obj_create(inner);
    lv_obj_set_style_border_width(indicator_container, 0, LV_PART_MAIN);
    lv_obj_set_style_height(indicator_container, lv_pct(100), LV_PART_MAIN);
    lv_obj_set_style_pad_all(indicator_container, 0, LV_PART_MAIN);
    lv_obj_set_style_width(indicator_container, lv_pct(CONTENT_WIDTH_PCT - channels_width), LV_PART_MAIN);

    this->color_text_ = lv_label_create(indicator_container);
    lv_obj_set_style_text_align(this->color_text_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    if (this->use_wheel_) {
      // The wheel already shows the colour, so there is no swatch for the label to sit
      // above; centring it uses the space the swatch would otherwise have left empty.
      lv_obj_set_style_align(this->color_text_, LV_ALIGN_CENTER, LV_PART_MAIN);
    } else {
      lv_obj_set_style_align(this->color_text_, LV_ALIGN_TOP_MID, LV_PART_MAIN);
      lv_obj_set_style_y(this->color_text_, lv_pct(10), LV_PART_MAIN);

      this->color_indicator_ = lv_obj_create(indicator_container);
      lv_obj_set_style_align(this->color_indicator_, LV_ALIGN_CENTER, LV_PART_MAIN);
      lv_obj_set_style_border_color(this->color_indicator_, lv_color_hex(0x808080), LV_PART_MAIN);
      lv_obj_set_style_border_width(this->color_indicator_, 1, LV_PART_MAIN);
      lv_obj_set_style_height(this->color_indicator_, lv_pct(50), LV_PART_MAIN);
      lv_obj_set_style_radius(this->color_indicator_, 0, LV_PART_MAIN);
      lv_obj_set_style_width(this->color_indicator_, lv_pct(90), LV_PART_MAIN);
    }

    if (channels != 0) {
      auto *rgb_container = lv_obj_create(inner);
      lv_obj_set_layout(rgb_container, LV_LAYOUT_FLEX);
      lv_obj_set_flex_flow(rgb_container, LV_FLEX_FLOW_ROW);
      lv_obj_set_flex_align(rgb_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
      lv_obj_set_style_border_width(rgb_container, 0, LV_PART_MAIN);
      lv_obj_set_style_height(rgb_container, lv_pct(100), LV_PART_MAIN);
      lv_obj_set_style_pad_all(rgb_container, 0, LV_PART_MAIN);
      lv_obj_set_style_pad_column(rgb_container, 0, LV_PART_MAIN);
      lv_obj_set_style_pad_row(rgb_container, 0, LV_PART_MAIN);
      lv_obj_set_style_width(rgb_container, lv_pct(channels_width), LV_PART_MAIN);

      struct ChannelInfo {
        SliderIndex index;
        const char *name;
        const lv_grad_dsc_t *grad;
      };
      const std::array<ChannelInfo, 3> channel_info{{
          {SLIDER_RED, "R", &red_bar},
          {SLIDER_GREEN, "G", &green_bar},
          {SLIDER_BLUE, "B", &blue_bar},
      }};
      for (const auto &channel : channel_info) {
        if (!this->enabled_(channel.index))
          continue;
        auto *container = this->create_slider_(rgb_container, channel.index, channel.name, true, channel.grad, 255);
        lv_obj_set_style_height(container, lv_pct(100), LV_PART_MAIN);
        lv_obj_set_style_width(container, lv_pct(100 / channels), LV_PART_MAIN);
        this->add_value_label_(channel.index, lv_color_black());
      }
    }

    if (this->enabled_(SLIDER_SATURATION)) {
      auto *saturation_container =
          this->create_slider_(middle, SLIDER_SATURATION, "Saturation", false, &this->saturation_bar_, 100);
      lv_obj_set_style_height(saturation_container, lv_pct(ROW_HEIGHT_PCT), LV_PART_MAIN);
      lv_obj_set_style_width(saturation_container, lv_pct(100), LV_PART_MAIN);
      this->add_value_label_(SLIDER_SATURATION, lv_color_black());
    }

    this->update_color_();

    lv_event_cb_t hsv_cb = [](lv_event_t *event) {
      auto *self = static_cast<LvColorPickerType *>(lv_event_get_user_data(event));
      self->update_hsl_();
    };
    lv_event_cb_t rgb_cb = [](lv_event_t *event) {
      auto *self = static_cast<LvColorPickerType *>(lv_event_get_user_data(event));
      self->update_rgb_();
    };
    for (size_t index = 0; index < SLIDER_COUNT; index++) {
      if (this->sliders_[index] == nullptr)
        continue;
      lv_obj_add_event_cb(this->sliders_[index], index <= SLIDER_BRIGHTNESS ? hsv_cb : rgb_cb, LV_EVENT_VALUE_CHANGED,
                          this);
    }

    bubble_events(outer);
  }

 protected:
  // Fills in everything that is the same for every colour picker. LVGL keeps only a pointer
  // to a gradient or a style, so one copy serves them all however many are on the screen.
  // Called on the first widget to be built and does nothing after that.
  static void init_shared() {
    if (shared_ready)
      return;
    shared_ready = true;

    color_bar.dir = LV_GRAD_DIR_VER;
    color_bar.stops_count = 7;
    color_bar.stops[0].color = lv_color_hex(0xFF0000);
    color_bar.stops[0].opa = LV_OPA_COVER;
    color_bar.stops[0].frac = 0;
    color_bar.stops[1].color = lv_color_hex(0xFF00FF);
    color_bar.stops[1].opa = LV_OPA_COVER;
    color_bar.stops[1].frac = 42;
    color_bar.stops[2].color = lv_color_hex(0xFF);
    color_bar.stops[2].opa = LV_OPA_COVER;
    color_bar.stops[2].frac = 84;
    color_bar.stops[3].color = lv_color_hex(0xFFFF);
    color_bar.stops[3].opa = LV_OPA_COVER;
    color_bar.stops[3].frac = 127;
    color_bar.stops[4].color = lv_color_hex(0xFF00);
    color_bar.stops[4].opa = LV_OPA_COVER;
    color_bar.stops[4].frac = 169;
    color_bar.stops[5].color = lv_color_hex(0xFFFF00);
    color_bar.stops[5].opa = LV_OPA_COVER;
    color_bar.stops[5].frac = 212;
    color_bar.stops[6].color = lv_color_hex(0xFF0000);
    color_bar.stops[6].opa = LV_OPA_COVER;
    color_bar.stops[6].frac = 255;
    blue_bar.dir = LV_GRAD_DIR_VER;
    blue_bar.stops_count = 2;
    blue_bar.stops[0].color = lv_color_hex(0xFF);
    blue_bar.stops[0].opa = LV_OPA_COVER;
    blue_bar.stops[0].frac = 0;
    blue_bar.stops[1].color = lv_color_hex(0xFFFFFF);
    blue_bar.stops[1].opa = LV_OPA_COVER;
    blue_bar.stops[1].frac = 255;
    green_bar.dir = LV_GRAD_DIR_VER;
    green_bar.stops_count = 2;
    green_bar.stops[0].color = lv_color_hex(0xFF00);
    green_bar.stops[0].opa = LV_OPA_COVER;
    green_bar.stops[0].frac = 0;
    green_bar.stops[1].color = lv_color_hex(0xFFFFFF);
    green_bar.stops[1].opa = LV_OPA_COVER;
    green_bar.stops[1].frac = 255;
    red_bar.dir = LV_GRAD_DIR_VER;
    red_bar.stops_count = 2;
    red_bar.stops[0].color = lv_color_hex(0xFF0000);
    red_bar.stops[0].opa = LV_OPA_COVER;
    red_bar.stops[0].frac = 0;
    red_bar.stops[1].color = lv_color_hex(0xFFFFFF);
    red_bar.stops[1].opa = LV_OPA_COVER;
    red_bar.stops[1].frac = 255;

    // The wheel's hue runs clockwise from red at the 3 o'clock position, matching how
    // lv_color_hsv_to_rgb() reads the angle a touch on it is converted to.
    lv_grad_conical_init(&wheel_hue_grad, lv_pct(50), lv_pct(50), 0, 360, LV_GRAD_EXTEND_PAD);
    wheel_hue_grad.stops_count = 7;
    wheel_hue_grad.stops[0].color = lv_color_hex(0xFF0000);
    wheel_hue_grad.stops[0].opa = LV_OPA_COVER;
    wheel_hue_grad.stops[0].frac = 0;
    wheel_hue_grad.stops[1].color = lv_color_hex(0xFFFF00);
    wheel_hue_grad.stops[1].opa = LV_OPA_COVER;
    wheel_hue_grad.stops[1].frac = 42;
    wheel_hue_grad.stops[2].color = lv_color_hex(0xFF00);
    wheel_hue_grad.stops[2].opa = LV_OPA_COVER;
    wheel_hue_grad.stops[2].frac = 84;
    wheel_hue_grad.stops[3].color = lv_color_hex(0xFFFF);
    wheel_hue_grad.stops[3].opa = LV_OPA_COVER;
    wheel_hue_grad.stops[3].frac = 127;
    wheel_hue_grad.stops[4].color = lv_color_hex(0xFF);
    wheel_hue_grad.stops[4].opa = LV_OPA_COVER;
    wheel_hue_grad.stops[4].frac = 169;
    wheel_hue_grad.stops[5].color = lv_color_hex(0xFF00FF);
    wheel_hue_grad.stops[5].opa = LV_OPA_COVER;
    wheel_hue_grad.stops[5].frac = 212;
    wheel_hue_grad.stops[6].color = lv_color_hex(0xFF0000);
    wheel_hue_grad.stops[6].opa = LV_OPA_COVER;
    wheel_hue_grad.stops[6].frac = 255;
    // Fades from opaque white at the centre to fully transparent at the edge, so the hue
    // wheel underneath shows through more with distance from the centre: white at the
    // centre, full saturation at the rim, the way saturation is drawn on a colour wheel.
    lv_grad_radial_init(&wheel_white_grad, lv_pct(50), lv_pct(50), lv_pct(100), lv_pct(50), LV_GRAD_EXTEND_PAD);
    wheel_white_grad.stops_count = 2;
    wheel_white_grad.stops[0].color = lv_color_white();
    wheel_white_grad.stops[0].opa = LV_OPA_COVER;
    wheel_white_grad.stops[0].frac = 0;
    wheel_white_grad.stops[1].color = lv_color_white();
    wheel_white_grad.stops[1].opa = LV_OPA_TRANSP;
    wheel_white_grad.stops[1].frac = 255;

    lv_style_init(&slider_vert);
    lv_style_set_align(&slider_vert, LV_ALIGN_CENTER);
    lv_style_set_bg_opa(&slider_vert, LV_OPA_COVER);
    lv_style_set_height(&slider_vert, lv_pct(90));
    lv_style_set_pad_all(&slider_vert, 0);
    lv_style_set_radius(&slider_vert, 0);
    lv_style_set_width(&slider_vert, lv_pct(50));
    lv_style_init(&slider_vert_knob);
    lv_style_set_bg_color(&slider_vert_knob, lv_color_hex(0xFFFFFF));
    lv_style_set_border_width(&slider_vert_knob, 1);
    lv_style_set_outline_color(&slider_vert_knob, lv_color_hex(0x00));
    lv_style_set_pad_bottom(&slider_vert_knob, -KNOB_TRIM);
    lv_style_set_pad_top(&slider_vert_knob, -KNOB_TRIM);
    lv_style_set_pad_left(&slider_vert_knob, KNOB_OVERHANG);
    lv_style_set_pad_right(&slider_vert_knob, KNOB_OVERHANG);
    lv_style_set_radius(&slider_vert_knob, 0);
    lv_style_init(&slider_horz);
    lv_style_set_align(&slider_horz, LV_ALIGN_CENTER);
    lv_style_set_bg_opa(&slider_horz, LV_OPA_COVER);
    // LVGL sizes a slider's knob from the thickness of its bar, so the horizontal bars are
    // given the same thickness as the vertical ones to keep every knob the same size. The
    // vertical bars are half the width of a container that is HUE_WIDTH_PCT of the widget,
    // and a horizontal bar's container is ROW_HEIGHT_PCT of its height.
    lv_style_set_height(&slider_horz, lv_pct(45));
    lv_style_set_pad_all(&slider_horz, 0);
    lv_style_set_radius(&slider_horz, 0);
    lv_style_set_width(&slider_horz, lv_pct(90));
    lv_style_init(&slider_horz_knob);
    lv_style_set_bg_color(&slider_horz_knob, lv_color_hex(0xFFFFFF));
    lv_style_set_border_width(&slider_horz_knob, 1);
    lv_style_set_outline_color(&slider_horz_knob, lv_color_hex(0x00));
    lv_style_set_pad_left(&slider_horz_knob, -KNOB_TRIM);
    lv_style_set_pad_right(&slider_horz_knob, -KNOB_TRIM);
    lv_style_set_pad_top(&slider_horz_knob, KNOB_OVERHANG);
    lv_style_set_pad_bottom(&slider_horz_knob, KNOB_OVERHANG);
    lv_style_set_radius(&slider_horz_knob, 0);
  }

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

  // Proportions of the layout, as percentages. The hue bar takes a column of the widget; the
  // saturation and brightness bars a row each of what is left, with the rest going to the
  // colour swatch and the channel bars beside it. The totals fall short of 100 so that the
  // rows are spaced apart rather than meeting edge to edge.
  static constexpr int32_t HUE_WIDTH_PCT = 15;
  static constexpr int32_t ROW_HEIGHT_PCT = 18;
  static constexpr int32_t CONTENT_HEIGHT_PCT = 94;
  static constexpr int32_t CHANNEL_WIDTH_PCT = 19;
  static constexpr int32_t CONTENT_WIDTH_PCT = 99;
  // The wheel replaces the hue bar's narrow column with a square one, wide enough to read as
  // a circle: the container it goes in is itself square, so equal width and height percentages
  // keep it that way whatever the widget's overall size.
  static constexpr int32_t WHEEL_WIDTH_PCT = 45;
  // Diameter of the ring that marks the currently chosen point on the wheel.
  static constexpr int32_t WHEEL_HANDLE_SIZE = 14;
  // Placing the handle and reading an angle back from a touch both need degrees-to-radians,
  // and <cmath> has no standard constant for it.
  static constexpr double PI = 3.14159265358979323846;

  bool enabled_(SliderIndex index) const { return (this->sliders_mask_ & (1 << index)) != 0; }
  bool has_(SliderIndex index) const { return this->sliders_[index] != nullptr; }

  // Reads a slider, falling back to the value held internally where the widget was built
  // without that slider, so an omitted one simply keeps whatever it was last set to.
  int32_t slider_value_(SliderIndex index, int32_t fallback) const {
    return this->has_(index) ? lv_slider_get_value(this->sliders_[index]) : fallback;
  }

  void set_slider_value_(SliderIndex index, int32_t value) {
    if (this->has_(index))
      lv_slider_set_value(this->sliders_[index], value, LV_ANIM_OFF);
  }

  // Builds one slider in a container of its own, holding a name label above the bar, and
  // returns the container so the caller can size and place it.
  lv_obj_t *create_slider_(lv_obj_t *parent, SliderIndex index, const char *name, bool vertical,
                           const lv_grad_dsc_t *grad, int32_t max) {
    auto *container = lv_obj_create(parent);
    this->init_slider_container_(container, name, vertical);

    auto *slider = lv_slider_create(container);
    this->sliders_[index] = slider;
    lv_obj_add_style(slider, vertical ? &slider_vert : &slider_horz, LV_PART_MAIN);
    lv_obj_set_style_bg_grad(slider, grad, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_add_style(slider, vertical ? &slider_vert_knob : &slider_horz_knob, LV_PART_KNOB);
    if (vertical)
      lv_obj_set_flex_grow(slider, 1);
    lv_slider_set_range(slider, 0, max);
    lv_slider_set_mode(slider, LV_SLIDER_MODE_NORMAL);
    return container;
  }

  // Builds the colour wheel that replaces the hue and saturation sliders: a circular hue
  // gradient with a white-to-transparent overlay for saturation, and a ring marking the
  // point currently chosen. Returns the container so the caller can size and place it.
  lv_obj_t *create_wheel_(lv_obj_t *parent) {
    auto *container = lv_obj_create(parent);
    lv_obj_set_style_border_width(container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(container, 0, LV_PART_MAIN);

    this->wheel_ = lv_obj_create(container);
    lv_obj_set_size(this->wheel_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_align(this->wheel_, LV_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_radius(this->wheel_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(this->wheel_, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(this->wheel_, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(this->wheel_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_grad(this->wheel_, &wheel_hue_grad, LV_PART_MAIN);
    lv_obj_remove_flag(this->wheel_, LV_OBJ_FLAG_SCROLLABLE);

    // Neither the overlay nor the handle should ever be the touch target: with
    // LV_OBJ_FLAG_CLICKABLE off, LVGL's hit testing skips them and falls through to the
    // wheel underneath, which is what a touch on either of them is meant to affect.
    this->wheel_overlay_ = lv_obj_create(this->wheel_);
    lv_obj_set_size(this->wheel_overlay_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_align(this->wheel_overlay_, LV_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_radius(this->wheel_overlay_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(this->wheel_overlay_, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(this->wheel_overlay_, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(this->wheel_overlay_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_grad(this->wheel_overlay_, &wheel_white_grad, LV_PART_MAIN);
    lv_obj_remove_flag(this->wheel_overlay_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(this->wheel_overlay_, LV_OBJ_FLAG_SCROLLABLE);

    this->wheel_handle_ = lv_obj_create(this->wheel_);
    lv_obj_set_size(this->wheel_handle_, WHEEL_HANDLE_SIZE, WHEEL_HANDLE_SIZE);
    lv_obj_set_style_align(this->wheel_handle_, LV_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_radius(this->wheel_handle_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(this->wheel_handle_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(this->wheel_handle_, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(this->wheel_handle_, lv_color_white(), LV_PART_MAIN);
    lv_obj_remove_flag(this->wheel_handle_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(this->wheel_handle_, LV_OBJ_FLAG_SCROLLABLE);

    // PRESSED as well as PRESSING, so tapping a point sets it immediately rather than only
    // once the finger moves.
    lv_obj_add_event_cb(this->wheel_, wheel_touch_cb, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(this->wheel_, wheel_touch_cb, LV_EVENT_PRESSING, this);
    // The wheel's radius, needed to place the handle, is not known until it has been laid
    // out, which has not happened yet here.
    lv_obj_add_event_cb(this->wheel_, wheel_size_cb, LV_EVENT_SIZE_CHANGED, this);
    return container;
  }

  // Adds the label that shows a slider's current value, drawn over the middle of its bar.
  void add_value_label_(SliderIndex index, lv_color_t color) {
    auto *slider = this->sliders_[index];
    lv_obj_add_flag(slider, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    auto *label = lv_label_create(slider);
    this->values_[index] = label;
    lv_obj_set_style_align(label, LV_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
  }

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

  static void wheel_touch_cb(lv_event_t *event) {
    static_cast<LvColorPickerType *>(lv_event_get_user_data(event))->update_from_wheel_touch_();
  }

  static void wheel_size_cb(lv_event_t *event) {
    static_cast<LvColorPickerType *>(lv_event_get_user_data(event))->update_wheel_handle_();
  }

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
    snprintf(buf, sizeof(buf), "#%02X%02X%02X", static_cast<unsigned>(this->state_.r),
             static_cast<unsigned>(this->state_.g), static_cast<unsigned>(this->state_.b));
    lv_label_set_text(this->color_text_, buf);
  }

  void update_saturation_bar_() {
    if (!this->has_(SLIDER_SATURATION))
      return;
    this->saturation_bar_.stops[1].color = lv_color_hsv_to_rgb(this->hue_, 100, 100);
    lv_obj_invalidate(this->sliders_[SLIDER_SATURATION]);
  }

  // Fades from black to the fully saturated current hue, so the bar shows what varying
  // brightness does to the hue being adjusted rather than a plain black-to-white fade.
  void update_brightness_bar_() {
    if (!this->has_(SLIDER_BRIGHTNESS))
      return;
    this->brightness_bar_.stops[1].color = lv_color_hsv_to_rgb(this->hue_, 100, 100);
    lv_obj_invalidate(this->sliders_[SLIDER_BRIGHTNESS]);
  }

  void update_values_() const {
    for (size_t index = 0; index < SLIDER_COUNT; index++) {
      if (this->values_[index] == nullptr)
        continue;
      // max 4 bytes: up to 3 digits (0..255) + null
      char buf[4];
      snprintf(buf, sizeof(buf), "%d", static_cast<int>(lv_slider_get_value(this->sliders_[index])));
      lv_label_set_text(this->values_[index], buf);
    }
  }

  void tint_knob_(SliderIndex index, lv_color_t color) const {
    if (this->has_(index))
      lv_obj_set_style_bg_color(this->sliders_[index], color, LV_PART_KNOB);
  }

  // Tints each knob with the colour its own bar shows at the current value, so a knob reads
  // as a sample of the gradient it sits on rather than a plain white block. The red, green
  // and blue bars each run from white up to their full primary, so their knob fades that one
  // channel in while the other two drop away. The brightness knob is the exception: its bar
  // varies only the hue's brightness, but the knob shows the colour actually selected,
  // saturation included, so it reads as a sample of the real result rather than of the bar.
  void update_knobs_() const {
    if (!this->tint_knobs_)
      return;
    this->tint_knob_(SLIDER_HUE, lv_color_hsv_to_rgb(this->hue_, 100, 100));
    this->tint_knob_(SLIDER_SATURATION, lv_color_hsv_to_rgb(this->hue_, this->saturation_, 100));
    this->tint_knob_(SLIDER_BRIGHTNESS, this->state_);
    this->tint_knob_(SLIDER_RED, lv_color_make(255, 255 - this->state_.r, 255 - this->state_.r));
    this->tint_knob_(SLIDER_GREEN, lv_color_make(255 - this->state_.g, 255, 255 - this->state_.g));
    this->tint_knob_(SLIDER_BLUE, lv_color_make(255 - this->state_.b, 255 - this->state_.b, 255));
  }

  // Moves the handle ring to the point on the wheel that matches the current hue and
  // saturation. Does nothing without a wheel, and before the wheel has been laid out, since
  // its radius is not known until then - update_wheel_handle_() is also called from
  // wheel_size_cb() once it has been, which puts the handle right on the first draw too.
  void update_wheel_handle_() {
    if (this->wheel_handle_ == nullptr)
      return;
    lv_area_t area;
    lv_obj_get_coords(this->wheel_, &area);
    auto radius = static_cast<double>(area.x2 - area.x1) / 2.0;
    if (radius <= 0)
      return;
    auto angle_rad = static_cast<double>(this->hue_) * (PI / 180.0);
    auto distance = (static_cast<double>(this->saturation_) / 100.0) * radius;
    lv_obj_set_style_translate_x(this->wheel_handle_, static_cast<int32_t>(std::lround(distance * std::cos(angle_rad))),
                                 LV_PART_MAIN);
    lv_obj_set_style_translate_y(this->wheel_handle_, static_cast<int32_t>(std::lround(distance * std::sin(angle_rad))),
                                 LV_PART_MAIN);
  }

  // Reads where the wheel was touched and sets the hue and saturation it represents there:
  // the angle from the centre is the hue, clockwise from red at the 3 o'clock position to
  // match wheel_hue_grad, and the distance from the centre is the saturation, capped at the
  // wheel's radius. Brightness is left alone - the wheel does not touch it.
  void update_from_wheel_touch_() {
    lv_indev_t *indev = lv_indev_active();
    if (indev == nullptr)
      return;
    lv_point_t point;
    lv_indev_get_point(indev, &point);
    lv_area_t area;
    lv_obj_get_coords(this->wheel_, &area);
    auto radius = static_cast<double>(area.x2 - area.x1) / 2.0;
    if (radius <= 0)
      return;
    auto dx = static_cast<double>(point.x - (area.x1 + area.x2) / 2);
    auto dy = static_cast<double>(point.y - (area.y1 + area.y2) / 2);
    auto angle = std::atan2(dy, dx) * (180.0 / PI);
    if (angle < 0)
      angle += 360.0;
    this->hue_ = static_cast<uint16_t>(std::lround(angle)) % 360;
    auto distance = std::sqrt(dx * dx + dy * dy);
    this->saturation_ = static_cast<uint8_t>(std::lround(std::min(100.0, distance * 100.0 / radius)));
    this->brightness_ = static_cast<uint8_t>(this->slider_value_(SLIDER_BRIGHTNESS, this->brightness_));
    lv_color32_t c32 =
        lv_color_to_32(lv_color_hsv_to_rgb(this->hue_, this->saturation_, this->brightness_), LV_OPA_COVER);
    this->state_ = Color(c32.red, c32.green, c32.blue);
    this->set_slider_value_(SLIDER_RED, c32.red);
    this->set_slider_value_(SLIDER_GREEN, c32.green);
    this->set_slider_value_(SLIDER_BLUE, c32.blue);
    this->refresh_();
    // Nothing but a slider sends this on its own; sending it here is what makes on_value and
    // on_update fire for the wheel the way they already do for every other kind of touch.
    lv_obj_send_event(this->wheel_, LV_EVENT_VALUE_CHANGED, nullptr);
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

  // Redraws everything that follows from the current colour.
  void refresh_() {
    if (this->color_indicator_ != nullptr)
      lv_obj_set_style_bg_color(this->color_indicator_, this->state_, LV_PART_MAIN);
    this->update_saturation_bar_();
    this->update_brightness_bar_();
    this->update_text_();
    this->update_values_();
    this->update_knobs_();
    this->update_wheel_handle_();
  }

  void update_hsl_() {
    this->hue_ = static_cast<uint16_t>(this->slider_value_(SLIDER_HUE, this->hue_));
    this->saturation_ = static_cast<uint8_t>(this->slider_value_(SLIDER_SATURATION, this->saturation_));
    this->brightness_ = static_cast<uint8_t>(this->slider_value_(SLIDER_BRIGHTNESS, this->brightness_));
    lv_color32_t c32 =
        lv_color_to_32(lv_color_hsv_to_rgb(this->hue_, this->saturation_, this->brightness_), LV_OPA_COVER);
    this->state_ = Color(c32.red, c32.green, c32.blue);
    this->set_slider_value_(SLIDER_RED, c32.red);
    this->set_slider_value_(SLIDER_GREEN, c32.green);
    this->set_slider_value_(SLIDER_BLUE, c32.blue);
    this->refresh_();
  }

  void update_rgb_() {
    this->state_ = Color(static_cast<uint8_t>(this->slider_value_(SLIDER_RED, this->state_.r)),
                         static_cast<uint8_t>(this->slider_value_(SLIDER_GREEN, this->state_.g)),
                         static_cast<uint8_t>(this->slider_value_(SLIDER_BLUE, this->state_.b)));
    auto hsv = lv_color_rgb_to_hsv(this->state_.r, this->state_.g, this->state_.b);
    this->hue_ = hsv.h;
    this->saturation_ = hsv.s;
    this->brightness_ = hsv.v;
    this->set_slider_value_(SLIDER_HUE, hsv.h);
    this->set_slider_value_(SLIDER_SATURATION, hsv.s);
    this->set_slider_value_(SLIDER_BRIGHTNESS, hsv.v);
    this->refresh_();
  }

  void update_color_() {
    this->set_slider_value_(SLIDER_RED, this->state_.r);
    this->set_slider_value_(SLIDER_GREEN, this->state_.g);
    this->set_slider_value_(SLIDER_BLUE, this->state_.b);
    this->update_rgb_();
    lv_obj_invalidate(this->obj);
  }

  // Shared between every colour picker, set up once by init_shared(). The saturation and
  // brightness bars differ from one widget to the next, since their far end follows the
  // chosen hue, so those two are instance fields further down instead.
  inline static bool shared_ready{false};
  inline static lv_grad_dsc_t color_bar{};
  inline static lv_grad_dsc_t blue_bar{};
  inline static lv_grad_dsc_t green_bar{};
  inline static lv_grad_dsc_t red_bar{};
  inline static lv_grad_dsc_t wheel_hue_grad{};
  inline static lv_grad_dsc_t wheel_white_grad{};
  inline static lv_style_t slider_vert{};
  inline static lv_style_t slider_vert_knob{};
  inline static lv_style_t slider_horz{};
  inline static lv_style_t slider_horz_knob{};

  lv_grad_dsc_t saturation_bar_{};
  lv_grad_dsc_t brightness_bar_{};
  // The colour currently shown, kept up to date as the sliders move. Converts to lv_color_t
  // on its own, so it can be handed straight to LVGL calls as well as read component-wise.
  Color state_{0x80, 0x80, 0x80};
  std::array<lv_obj_t *, SLIDER_COUNT> sliders_{};
  // The label drawn over each slider, or null where that slider has none.
  std::array<lv_obj_t *, SLIDER_COUNT> values_{};
  lv_obj_t *color_indicator_{};
  lv_obj_t *color_text_{};
  // The colour in the form the hue, saturation and brightness sliders take, kept alongside
  // `state_` so that a slider left out of the widget still has a value to contribute.
  uint16_t hue_{0};
  uint8_t saturation_{0};
  uint8_t brightness_{50};
  const uint8_t sliders_mask_;
  const bool use_wheel_;
  bool tint_knobs_{true};
  // The circular touch area itself, the white-to-transparent overlay drawn over it, and the
  // ring marking the currently chosen point. Null unless `use_wheel_` is set.
  lv_obj_t *wheel_{};
  lv_obj_t *wheel_overlay_{};
  lv_obj_t *wheel_handle_{};
};

}  // namespace esphome::lvgl

#endif  // USE_LVGL_COLOR_PICKER
