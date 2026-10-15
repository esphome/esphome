#pragma once

#include "lvgl_esphome.h"
#include "esphome/core/color.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <numbers>

#ifdef USE_LVGL_COLOR_PICKER

namespace esphome::lvgl {

// The hue gradients need seven stops; gradient.py raises the limit when a colour picker is in use.
static_assert(LV_GRADIENT_MAX_STOPS >= 7, "color_picker needs LV_GRADIENT_MAX_STOPS >= 7");

class LvColorPickerType : public LvCompound {
 public:
  enum SliderIndex : size_t {
    SLIDER_HUE,
    SLIDER_SATURATION,
    SLIDER_BRIGHTNESS,
    SLIDER_RED,
    SLIDER_GREEN,
    SLIDER_BLUE,
    SLIDER_COUNT,
  };

  enum SliderFlag : uint8_t {
    SLIDER_FLAG_HUE = 1 << SLIDER_HUE,
    SLIDER_FLAG_SATURATION = 1 << SLIDER_SATURATION,
    SLIDER_FLAG_BRIGHTNESS = 1 << SLIDER_BRIGHTNESS,
    SLIDER_FLAG_RED = 1 << SLIDER_RED,
    SLIDER_FLAG_GREEN = 1 << SLIDER_GREEN,
    SLIDER_FLAG_BLUE = 1 << SLIDER_BLUE,
  };

  explicit LvColorPickerType(uint8_t sliders, bool use_wheel) : sliders_mask_(sliders), use_wheel_(use_wheel) {}

  // Returns null for a slider the widget was not built with.
  lv_obj_t *get_slider(size_t index) const { return this->sliders_[index]; }

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
    // The far end of these two follows the current hue.
    init_bar(&this->saturation_bar_, LV_GRAD_DIR_HOR, lv_color_white(), lv_color_hex(0xFF0000));
    init_bar(&this->brightness_bar_, LV_GRAD_DIR_HOR, lv_color_black(), lv_color_white());

    auto channels = static_cast<int32_t>(this->enabled_(SLIDER_RED)) +
                    static_cast<int32_t>(this->enabled_(SLIDER_GREEN)) +
                    static_cast<int32_t>(this->enabled_(SLIDER_BLUE));

    // No layout on the outer object, so LV_SIZE_CONTENT uses the size from self_size_cb.
    lv_obj_add_event_cb(outer, self_size_cb, LV_EVENT_GET_SELF_SIZE, nullptr);
    lv_obj_add_event_cb(outer, size_changed_cb, LV_EVENT_SIZE_CHANGED, this);
    lv_obj_set_style_pad_all(outer, KNOB_OVERHANG, LV_PART_MAIN);

    int32_t left_width = 0;
    lv_obj_t *left = nullptr;
    if (this->enabled_(SLIDER_HUE)) {
      left_width = HUE_WIDTH_PCT;
      left = this->create_slider_(outer, SLIDER_HUE, "Hue", true, &hue_bar, 360);
      lv_obj_set_size(left, lv_pct(HUE_WIDTH_PCT), lv_pct(100));
    }
#ifdef USE_LVGL_COLOR_PICKER_WHEEL
    else if (this->use_wheel_) {
      left_width = WHEEL_WIDTH_PCT;
      left = this->create_wheel_(outer);
      lv_obj_set_size(left, lv_pct(WHEEL_WIDTH_PCT), lv_pct(WHEEL_WIDTH_PCT));
    }
#endif
    if (left != nullptr) {
      lv_obj_set_style_align(left, LV_ALIGN_LEFT_MID, LV_PART_MAIN);
      // A narrow hue column may be slimmer than its label.
      lv_obj_set_flag(left, LV_OBJ_FLAG_OVERFLOW_VISIBLE, true);
    }

    // The text rows keep their natural height and the bars between them take what is left,
    // so a small widget gets thinner bars rather than clipped text.
    auto *middle = create_flex(outer, LV_FLEX_FLOW_COLUMN, LV_FLEX_ALIGN_START);
    lv_obj_set_style_align(middle, LV_ALIGN_RIGHT_MID, LV_PART_MAIN);
    lv_obj_set_style_pad_row(middle, KNOB_OVERHANG, LV_PART_MAIN);
    lv_obj_set_size(middle, lv_pct(100 - left_width), lv_pct(100));

    // A dark bar needs light text, unlike every other value label.
    if (this->enabled_(SLIDER_BRIGHTNESS))
      this->create_row_slider_(middle, SLIDER_BRIGHTNESS, "Brightness", &this->brightness_bar_, lv_color_white());

    auto *inner = create_flex(middle, LV_FLEX_FLOW_ROW, LV_FLEX_ALIGN_START);
    lv_obj_set_width(inner, lv_pct(100));
    lv_obj_set_flex_grow(inner, 1);

    // The channel bars keep a fixed width each, so dropping one widens the swatch.
    auto channels_width = CHANNEL_WIDTH_PCT * channels;
    auto *indicator_container = create_flex(inner, LV_FLEX_FLOW_COLUMN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_size(indicator_container, lv_pct(CONTENT_WIDTH_PCT - channels_width), lv_pct(100));
    lv_obj_set_style_pad_ver(indicator_container, KNOB_OVERHANG, LV_PART_MAIN);
    lv_obj_set_style_pad_row(indicator_container, KNOB_OVERHANG, LV_PART_MAIN);

    this->color_text_ = lv_label_create(indicator_container);
    this->color_indicator_ = lv_obj_create(indicator_container);
    lv_obj_set_style_border_color(this->color_indicator_, lv_color_hex(0x808080), LV_PART_MAIN);
    lv_obj_set_style_border_width(this->color_indicator_, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(this->color_indicator_, 0, LV_PART_MAIN);
    lv_obj_set_width(this->color_indicator_, lv_pct(90));
    lv_obj_set_flex_grow(this->color_indicator_, 1);

    if (channels != 0) {
      auto *rgb_container = create_flex(inner, LV_FLEX_FLOW_ROW, LV_FLEX_ALIGN_SPACE_BETWEEN);
      lv_obj_set_size(rgb_container, lv_pct(channels_width), lv_pct(100));
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
        lv_obj_set_size(container, lv_pct(100 / channels), lv_pct(100));
        this->add_value_label_(channel.index, lv_color_black());
      }
    }

    if (this->enabled_(SLIDER_SATURATION))
      this->create_row_slider_(middle, SLIDER_SATURATION, "Saturation", &this->saturation_bar_, lv_color_black());

    this->update_color_();

    for (size_t index = 0; index < SLIDER_COUNT; index++) {
      if (this->sliders_[index] == nullptr)
        continue;
      lv_obj_add_event_cb(this->sliders_[index], index <= SLIDER_BRIGHTNESS ? hsv_changed_cb : rgb_changed_cb,
                          LV_EVENT_VALUE_CHANGED, this);
    }

    bubble_events(outer);
  }

 protected:
  // LVGL keeps only pointers to gradients and styles, so one copy serves every colour picker.
  static void init_shared() {
    if (shared_ready)
      return;
    shared_ready = true;

    // Clockwise from red at 3 o'clock, matching how lv_color_hsv_to_rgb() reads the angle.
    std::array<lv_color_t, 7> hues{lv_color_hex(0xFF0000), lv_color_hex(0xFFFF00), lv_color_hex(0x00FF00),
                                   lv_color_hex(0x00FFFF), lv_color_hex(0x0000FF), lv_color_hex(0xFF00FF),
                                   lv_color_hex(0xFF0000)};
#ifdef USE_LVGL_COLOR_PICKER_WHEEL
    lv_grad_conical_init(&wheel_hue_grad, lv_pct(50), lv_pct(50), 0, 360, LV_GRAD_EXTEND_PAD);
    lv_grad_init_stops(&wheel_hue_grad, hues.data(), nullptr, nullptr, static_cast<int>(hues.size()));
    // White at the centre fading out at the rim, giving saturation over the hue wheel.
    const lv_color_t whites[] = {lv_color_white(), lv_color_white()};
    const lv_opa_t white_opas[] = {LV_OPA_COVER, LV_OPA_TRANSP};
    lv_grad_radial_init(&wheel_white_grad, lv_pct(50), lv_pct(50), lv_pct(100), lv_pct(50), LV_GRAD_EXTEND_PAD);
    lv_grad_init_stops(&wheel_white_grad, whites, white_opas, nullptr, 2);
#endif
    // A vertical slider has its minimum at the bottom, so the hue bar runs the other way.
    std::reverse(hues.begin(), hues.end());
    lv_grad_vertical_init(&hue_bar);
    lv_grad_init_stops(&hue_bar, hues.data(), nullptr, nullptr, static_cast<int>(hues.size()));

    init_bar(&red_bar, LV_GRAD_DIR_VER, lv_color_hex(0xFF0000), lv_color_white());
    init_bar(&green_bar, LV_GRAD_DIR_VER, lv_color_hex(0x00FF00), lv_color_white());
    init_bar(&blue_bar, LV_GRAD_DIR_VER, lv_color_hex(0x0000FF), lv_color_white());

    lv_style_init(&plain_box);
    lv_style_set_bg_opa(&plain_box, LV_OPA_TRANSP);
    lv_style_set_border_width(&plain_box, 0);
    lv_style_set_pad_all(&plain_box, 0);
    lv_style_set_pad_row(&plain_box, 0);
    lv_style_set_pad_column(&plain_box, 0);

    init_slider_styles(&slider_vert, &slider_vert_knob, true);
    init_slider_styles(&slider_horz, &slider_horz_knob, false);
  }

  static void init_bar(lv_grad_dsc_t *grad, lv_grad_dir_t dir, lv_color_t from, lv_color_t to) {
    const lv_color_t colors[] = {from, to};
    grad->dir = dir;
    lv_grad_init_stops(grad, colors, nullptr, nullptr, 2);
  }

  static void init_slider_styles(lv_style_t *bar, lv_style_t *knob, bool vertical) {
    lv_style_init(bar);
    lv_style_set_align(bar, LV_ALIGN_CENTER);
    lv_style_set_bg_opa(bar, LV_OPA_COVER);
    lv_style_set_pad_all(bar, 0);
    lv_style_set_radius(bar, 0);
    // A horizontal bar's thickness is set by update_bar_thickness_().
    if (vertical) {
      lv_style_set_width(bar, lv_pct(50));
      lv_style_set_height(bar, lv_pct(90));
    } else {
      lv_style_set_width(bar, lv_pct(90));
    }

    lv_style_init(knob);
    lv_style_set_bg_color(knob, lv_color_white());
    lv_style_set_border_width(knob, 1);
    lv_style_set_outline_color(knob, lv_color_black());
    lv_style_set_radius(knob, 0);
    // Both axes are set, or the theme picks a knob size the layout has not left room for.
    int32_t along = -KNOB_TRIM;
    int32_t across = KNOB_OVERHANG;
    lv_style_set_pad_top(knob, vertical ? along : across);
    lv_style_set_pad_bottom(knob, vertical ? along : across);
    lv_style_set_pad_left(knob, vertical ? across : along);
    lv_style_set_pad_right(knob, vertical ? across : along);
  }

  // A borderless, unpadded container.
  static lv_obj_t *create_box(lv_obj_t *parent) {
    auto *obj = lv_obj_create(parent);
    lv_obj_add_style(obj, &plain_box, LV_PART_MAIN);
    lv_obj_set_flag(obj, LV_OBJ_FLAG_SCROLLABLE, false);
    return obj;
  }

  static lv_obj_t *create_flex(lv_obj_t *parent, lv_flex_flow_t flow, lv_flex_align_t main_place,
                               lv_flex_align_t track_place = LV_FLEX_ALIGN_START) {
    auto *obj = create_box(parent);
    lv_obj_set_layout(obj, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(obj, flow);
    lv_obj_set_flex_align(obj, main_place, LV_FLEX_ALIGN_CENTER, track_place);
    return obj;
  }

  // Preferred size, in lines of the text font. The widget is square.
  static constexpr int SELF_SIZE_FONT_MULTIPLE = 16;

  // At either end of its travel half the knob lies outside the bar. KNOB_TRIM pulls it back
  // along the bar, and KNOB_OVERHANG makes it stand out across the bar.
  static constexpr int KNOB_TRIM = 4;
  static constexpr int KNOB_OVERHANG = 4;

  // Layout proportions, as percentages.
  static constexpr int32_t HUE_WIDTH_PCT = 15;
  static constexpr int32_t CHANNEL_WIDTH_PCT = 19;
  static constexpr int32_t CONTENT_WIDTH_PCT = 99;
  static constexpr int32_t WHEEL_WIDTH_PCT = 45;
  static constexpr int32_t WHEEL_HANDLE_SIZE = 14;

  bool enabled_(SliderIndex index) const { return (this->sliders_mask_ & (1 << index)) != 0; }
  bool has_(SliderIndex index) const { return this->sliders_[index] != nullptr; }
  bool can_change_saturation_() const {
    return this->use_wheel_ || (this->sliders_mask_ &
                                (SLIDER_FLAG_SATURATION | SLIDER_FLAG_RED | SLIDER_FLAG_GREEN | SLIDER_FLAG_BLUE)) != 0;
  }
  bool can_change_brightness_() const {
    return (this->sliders_mask_ & (SLIDER_FLAG_BRIGHTNESS | SLIDER_FLAG_RED | SLIDER_FLAG_GREEN | SLIDER_FLAG_BLUE)) !=
           0;
  }

  // A slider the widget was built without keeps the value held internally.
  int32_t slider_value_(SliderIndex index, int32_t fallback) const {
    return this->has_(index) ? lv_slider_get_value(this->sliders_[index]) : fallback;
  }

  void set_slider_value_(SliderIndex index, int32_t value) {
    if (this->has_(index))
      lv_slider_set_value(this->sliders_[index], value, LV_ANIM_OFF);
  }

  // Builds a slider with a name label above it, and returns its container.
  lv_obj_t *create_slider_(lv_obj_t *parent, SliderIndex index, const char *name, bool vertical,
                           const lv_grad_dsc_t *grad, int32_t max) {
    // A column keeps the label on its own row however large the font is.
    auto *container = create_flex(parent, LV_FLEX_FLOW_COLUMN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    if (vertical) {
      // How far the knob reaches past the ends depends on the font, so follow style changes.
      update_knob_clearance(container);
      lv_obj_add_event_cb(container, knob_clearance_cb, LV_EVENT_STYLE_CHANGED, nullptr);
    } else {
      lv_obj_set_style_pad_bottom(container, KNOB_OVERHANG, LV_PART_MAIN);
      lv_obj_set_style_pad_row(container, KNOB_OVERHANG, LV_PART_MAIN);
    }
    lv_label_set_text(lv_label_create(container), name);

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

  void create_row_slider_(lv_obj_t *parent, SliderIndex index, const char *name, const lv_grad_dsc_t *grad,
                          lv_color_t text_color) {
    auto *container = this->create_slider_(parent, index, name, false, grad, 100);
    lv_obj_set_size(container, lv_pct(100), LV_SIZE_CONTENT);
    this->add_value_label_(index, text_color);
  }

#ifdef USE_LVGL_COLOR_PICKER_WHEEL
  // A full-size circle filled with a gradient.
  static lv_obj_t *create_disc(lv_obj_t *parent, const lv_grad_dsc_t *grad) {
    auto *disc = create_box(parent);
    lv_obj_set_size(disc, lv_pct(100), lv_pct(100));
    lv_obj_set_style_align(disc, LV_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_radius(disc, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(disc, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_grad(disc, grad, LV_PART_MAIN);
    lv_obj_set_flag(disc, LV_OBJ_FLAG_SCROLLABLE, false);
    return disc;
  }

  // Builds the colour wheel that replaces the hue and saturation sliders, and returns its container.
  lv_obj_t *create_wheel_(lv_obj_t *parent) {
    auto *container = create_box(parent);
    this->wheel_ = create_disc(container, &wheel_hue_grad);
    // Not clickable, so touches fall through to the wheel underneath.
    auto *overlay = create_disc(this->wheel_, &wheel_white_grad);
    lv_obj_set_flag(overlay, LV_OBJ_FLAG_CLICKABLE, false);

    this->wheel_handle_ = lv_obj_create(this->wheel_);
    lv_obj_set_size(this->wheel_handle_, WHEEL_HANDLE_SIZE, WHEEL_HANDLE_SIZE);
    lv_obj_set_style_align(this->wheel_handle_, LV_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_radius(this->wheel_handle_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(this->wheel_handle_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(this->wheel_handle_, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(this->wheel_handle_, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_flag(this->wheel_handle_, LV_OBJ_FLAG_CLICKABLE, false);
    lv_obj_set_flag(this->wheel_handle_, LV_OBJ_FLAG_SCROLLABLE, false);

    // PRESSED as well, so a tap takes effect without the finger moving.
    lv_obj_add_event_cb(this->wheel_, wheel_touch_cb, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(this->wheel_, wheel_touch_cb, LV_EVENT_PRESSING, this);
    // The handle can only be placed once the wheel has been laid out.
    lv_obj_add_event_cb(this->wheel_, wheel_size_cb, LV_EVENT_SIZE_CHANGED, this);
    return container;
  }
#endif

  void add_value_label_(SliderIndex index, lv_color_t color) {
    auto *slider = this->sliders_[index];
    lv_obj_set_flag(slider, LV_OBJ_FLAG_OVERFLOW_VISIBLE, true);
    auto *label = lv_label_create(slider);
    this->values_[index] = label;
    lv_obj_set_style_align(label, LV_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
  }

  // Leaves room at the ends of a vertical slider for the overhanging knob: half a line of text.
  static void update_knob_clearance(lv_obj_t *container) {
    const auto *font = lv_obj_get_style_text_font(container, LV_PART_MAIN);
    auto clearance = static_cast<int32_t>(lv_font_get_line_height(font)) / 2;
    if (clearance == lv_obj_get_style_pad_bottom(container, LV_PART_MAIN))
      return;
    lv_obj_set_style_pad_bottom(container, clearance, LV_PART_MAIN);
    lv_obj_set_style_pad_row(container, clearance, LV_PART_MAIN);
  }

  static void knob_clearance_cb(lv_event_t *event) { update_knob_clearance(lv_event_get_current_target_obj(event)); }

  static LvColorPickerType *from_event(lv_event_t *event) {
    return static_cast<LvColorPickerType *>(lv_event_get_user_data(event));
  }
  static void hsv_changed_cb(lv_event_t *event) { from_event(event)->update_hsv_(); }
  static void rgb_changed_cb(lv_event_t *event) { from_event(event)->update_rgb_(); }
#ifdef USE_LVGL_COLOR_PICKER_WHEEL
  static void wheel_touch_cb(lv_event_t *event) { from_event(event)->update_from_wheel_touch_(); }
  static void wheel_size_cb(lv_event_t *event) { from_event(event)->update_wheel_handle_(); }
#endif
  static void size_changed_cb(lv_event_t *event) { from_event(event)->update_bar_thickness_(); }

  // Gives the horizontal bars the thickness of the vertical ones, half a hue column, so every
  // knob is the same size. A knob is sized from the thickness of its bar.
  void update_bar_thickness_() {
    auto thickness = lv_obj_get_content_width(this->obj) * HUE_WIDTH_PCT / 200;
    for (auto index : {SLIDER_SATURATION, SLIDER_BRIGHTNESS}) {
      if (this->has_(index) && lv_obj_get_style_height(this->sliders_[index], LV_PART_MAIN) != thickness)
        lv_obj_set_style_height(this->sliders_[index], thickness, LV_PART_MAIN);
    }
  }

  // LVGL also counts this size when working out the scroll area, so a widget given a fixed
  // width must not claim it, or it would scroll whenever it is smaller.
  static void self_size_cb(lv_event_t *event) {
    auto *obj = lv_event_get_current_target_obj(event);
    if (lv_obj_get_style_width(obj, LV_PART_MAIN) != LV_SIZE_CONTENT)
      return;
    const auto *font = lv_obj_get_style_text_font(obj, LV_PART_MAIN);
    auto self_size = static_cast<int32_t>(lv_font_get_line_height(font)) * SELF_SIZE_FONT_MULTIPLE;
    auto *point = static_cast<lv_point_t *>(lv_event_get_param(event));
    point->x = self_size;
    point->y = self_size;
  }

  void update_text_() const {
    // "#" + 3x "%02X" + null
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X", static_cast<unsigned>(this->state_.r),
             static_cast<unsigned>(this->state_.g), static_cast<unsigned>(this->state_.b));
    lv_label_set_text(this->color_text_, buf);
  }

  void update_hue_bar_(SliderIndex index, lv_grad_dsc_t *grad, lv_color_t color) {
    if (!this->has_(index))
      return;
    grad->stops[1].color = color;
    lv_obj_invalidate(this->sliders_[index]);
  }

  void update_values_() const {
    for (size_t index = 0; index < SLIDER_COUNT; index++) {
      if (this->values_[index] == nullptr)
        continue;
      // up to 3 digits + null
      char buf[4];
      snprintf(buf, sizeof(buf), "%d", static_cast<int>(lv_slider_get_value(this->sliders_[index])));
      lv_label_set_text(this->values_[index], buf);
    }
  }

  void tint_knob_(SliderIndex index, lv_color_t color) const {
    if (this->has_(index))
      lv_obj_set_style_bg_color(this->sliders_[index], color, LV_PART_KNOB);
  }

  // Tints each knob with the colour of its bar at the current value. The brightness knob
  // shows the selected colour instead, since its bar leaves out saturation.
  void update_knobs_(lv_color_t full_hue) const {
    if (!this->tint_knobs_)
      return;
    this->tint_knob_(SLIDER_HUE, full_hue);
    this->tint_knob_(SLIDER_SATURATION, lv_color_hsv_to_rgb(this->hue_, this->saturation_, 100));
    this->tint_knob_(SLIDER_BRIGHTNESS, this->state_);
    this->tint_knob_(SLIDER_RED, lv_color_make(255, 255 - this->state_.r, 255 - this->state_.r));
    this->tint_knob_(SLIDER_GREEN, lv_color_make(255 - this->state_.g, 255, 255 - this->state_.g));
    this->tint_knob_(SLIDER_BLUE, lv_color_make(255 - this->state_.b, 255 - this->state_.b, 255));
  }

#ifdef USE_LVGL_COLOR_PICKER_WHEEL
  // Returns the wheel's radius, which is zero until it has been laid out.
  double wheel_radius_(lv_area_t *area) const {
    lv_obj_get_coords(this->wheel_, area);
    return static_cast<double>(area->x2 - area->x1) / 2.0;
  }

  void update_wheel_handle_() {
    if (this->wheel_handle_ == nullptr)
      return;
    lv_area_t area;
    auto radius = this->wheel_radius_(&area);
    if (radius <= 0)
      return;
    auto angle_rad = static_cast<double>(this->hue_) * (std::numbers::pi / 180.0);
    auto distance = (static_cast<double>(this->saturation_) / 100.0) * radius;
    lv_obj_set_style_translate_x(this->wheel_handle_, static_cast<int32_t>(std::lround(distance * std::cos(angle_rad))),
                                 LV_PART_MAIN);
    lv_obj_set_style_translate_y(this->wheel_handle_, static_cast<int32_t>(std::lround(distance * std::sin(angle_rad))),
                                 LV_PART_MAIN);
  }

  // The angle from the centre gives the hue, the distance the saturation. Brightness is unchanged.
  void update_from_wheel_touch_() {
    lv_indev_t *indev = lv_indev_active();
    if (indev == nullptr)
      return;
    lv_point_t point;
    lv_indev_get_point(indev, &point);
    lv_area_t area;
    auto radius = this->wheel_radius_(&area);
    if (radius <= 0)
      return;
    auto dx = point.x - (area.x1 + area.x2) / 2.0;
    auto dy = point.y - (area.y1 + area.y2) / 2.0;
    auto angle = std::atan2(dy, dx) * (180.0 / std::numbers::pi);
    if (angle < 0)
      angle += 360.0;
    this->hue_ = static_cast<uint16_t>(std::lround(angle)) % 360;
    auto distance = std::sqrt(dx * dx + dy * dy);
    this->saturation_ = static_cast<uint8_t>(std::lround(std::min(100.0, distance * 100.0 / radius)));
    this->brightness_ = static_cast<uint8_t>(this->slider_value_(SLIDER_BRIGHTNESS, this->brightness_));
    this->apply_hsv_();
    // Only sliders send this themselves; without it on_value would not fire for the wheel.
    lv_obj_send_event(this->wheel_, LV_EVENT_VALUE_CHANGED, nullptr);
  }
#endif

  // Lets events from the internal sliders reach the widget, where the triggers listen. Every
  // object in between needs the flag; the widget itself does not, so events stop there.
  static void bubble_events(lv_obj_t *obj) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) {
      auto *child = lv_obj_get_child(obj, i);
      lv_obj_set_flag(child, LV_OBJ_FLAG_EVENT_BUBBLE, true);
      bubble_events(child);
    }
  }

  void refresh_() {
    lv_obj_set_style_bg_color(this->color_indicator_, this->state_, LV_PART_MAIN);
    auto full_hue = lv_color_hsv_to_rgb(this->hue_, 100, 100);
    this->update_hue_bar_(SLIDER_SATURATION, &this->saturation_bar_, full_hue);
    this->update_hue_bar_(SLIDER_BRIGHTNESS, &this->brightness_bar_, full_hue);
    this->update_text_();
    this->update_values_();
    this->update_knobs_(full_hue);
#ifdef USE_LVGL_COLOR_PICKER_WHEEL
    this->update_wheel_handle_();
#endif
  }

  // Sets the colour from the hue, saturation and brightness, and moves the RGB sliders to match.
  void apply_hsv_() {
    lv_color32_t c32 =
        lv_color_to_32(lv_color_hsv_to_rgb(this->hue_, this->saturation_, this->brightness_), LV_OPA_COVER);
    this->state_ = Color(c32.red, c32.green, c32.blue);
    this->set_slider_value_(SLIDER_RED, c32.red);
    this->set_slider_value_(SLIDER_GREEN, c32.green);
    this->set_slider_value_(SLIDER_BLUE, c32.blue);
    this->refresh_();
  }

  void update_hsv_() {
    this->hue_ = static_cast<uint16_t>(this->slider_value_(SLIDER_HUE, this->hue_));
    this->saturation_ = static_cast<uint8_t>(this->slider_value_(SLIDER_SATURATION, this->saturation_));
    this->brightness_ = static_cast<uint8_t>(this->slider_value_(SLIDER_BRIGHTNESS, this->brightness_));
    this->apply_hsv_();
  }

  void update_rgb_() {
    this->state_ = Color(static_cast<uint8_t>(this->slider_value_(SLIDER_RED, this->state_.r)),
                         static_cast<uint8_t>(this->slider_value_(SLIDER_GREEN, this->state_.g)),
                         static_cast<uint8_t>(this->slider_value_(SLIDER_BLUE, this->state_.b)));
    auto hsv = lv_color_rgb_to_hsv(this->state_.r, this->state_.g, this->state_.b);
    this->hue_ = hsv.h;
    this->saturation_ = hsv.s;
    this->brightness_ = hsv.v;
    // A grey, white or black colour leaves a picker stuck there if nothing it has can move the
    // saturation, or the brightness, away from zero.
    bool stuck = false;
    if (this->saturation_ == 0 && !this->can_change_saturation_()) {
      this->saturation_ = 100;
      stuck = true;
    }
    if (this->brightness_ == 0 && !this->can_change_brightness_()) {
      this->brightness_ = 100;
      stuck = true;
    }
    this->set_slider_value_(SLIDER_HUE, this->hue_);
    this->set_slider_value_(SLIDER_SATURATION, this->saturation_);
    this->set_slider_value_(SLIDER_BRIGHTNESS, this->brightness_);
    if (stuck) {
      this->apply_hsv_();
    } else {
      this->refresh_();
    }
  }

  void update_color_() {
    this->set_slider_value_(SLIDER_RED, this->state_.r);
    this->set_slider_value_(SLIDER_GREEN, this->state_.g);
    this->set_slider_value_(SLIDER_BLUE, this->state_.b);
    this->update_rgb_();
    lv_obj_invalidate(this->obj);
  }

  // Shared by every colour picker; set up once by init_shared().
  inline static bool shared_ready{false};
  inline static lv_grad_dsc_t hue_bar{};
  inline static lv_grad_dsc_t red_bar{};
  inline static lv_grad_dsc_t green_bar{};
  inline static lv_grad_dsc_t blue_bar{};
#ifdef USE_LVGL_COLOR_PICKER_WHEEL
  inline static lv_grad_dsc_t wheel_hue_grad{};
  inline static lv_grad_dsc_t wheel_white_grad{};
#endif
  inline static lv_style_t plain_box{};
  inline static lv_style_t slider_vert{};
  inline static lv_style_t slider_vert_knob{};
  inline static lv_style_t slider_horz{};
  inline static lv_style_t slider_horz_knob{};

  // Per widget, since their far end follows the chosen hue.
  lv_grad_dsc_t saturation_bar_{};
  lv_grad_dsc_t brightness_bar_{};
  Color state_{0x80, 0x80, 0x80};
  std::array<lv_obj_t *, SLIDER_COUNT> sliders_{};
  std::array<lv_obj_t *, SLIDER_COUNT> values_{};
  lv_obj_t *color_indicator_{};
  lv_obj_t *color_text_{};
  // Kept alongside state_ so a slider left out of the widget still has a value.
  uint16_t hue_{0};
  uint8_t saturation_{0};
  uint8_t brightness_{50};
  const uint8_t sliders_mask_;
  const bool use_wheel_;
  bool tint_knobs_{true};
#ifdef USE_LVGL_COLOR_PICKER_WHEEL
  // Null unless use_wheel_ is set.
  lv_obj_t *wheel_{};
  lv_obj_t *wheel_handle_{};
#endif
};

}  // namespace esphome::lvgl

#endif  // USE_LVGL_COLOR_PICKER
