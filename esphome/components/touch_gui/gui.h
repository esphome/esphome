#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/display/display_buffer.h"

#include <map>
#include <set>

namespace esphome {
namespace touch_gui {

enum TouchGUIButtonType {
  TOUCH_GUI_BUTTON_TYPE_MOMENTARY,
  TOUCH_GUI_BUTTON_TYPE_TOGGLE,
  TOUCH_GUI_BUTTON_TYPE_RADIO,
  TOUCH_GUI_BUTTON_TYPE_AREA,
};

class TouchGUIButton;
using button_writer_t = std::function<void(TouchGUIButton &)>;
using ButtonPtrSet = std::set<TouchGUIButton *>;
using RadioGroupMap = std::map<int, ButtonPtrSet>;

/** Class to route touch events to virtual buttons.
 *
 */
class TouchGUIComponent : public PollingComponent {
 public:
  void set_display(display::DisplayBuffer *display) { this->display_ = display; }
  void set_button_background_color(const Color &col) { this->button_background_color_ = col; }
  void set_button_active_background_color(const Color &col) { this->button_active_background_color_ = col; }
  void set_button_foreground_color(const Color &col) { this->button_foreground_color_ = col; }
  void set_button_active_foreground_color(const Color &col) { this->button_active_foreground_color_ = col; }
  void set_button_border_color(const Color &col) { this->button_border_color_ = col; }
  void set_button_font(display::Font *fnt) { this->button_font_ = fnt; }
  void set_writer(button_writer_t &&writer) { this->writer_ = writer; }
  void add_on_update_callback(std::function<void()> &&callback);

  void register_button(TouchGUIButton *button) { this->buttons_.push_back(button); }

  void setup() override;
  /// Loops through the virtual buttons to let them process their stati
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  /// Draw all the elements visible on the current page.
  void draw();
  /// Report an user interaction.
  void touch(int x, int y, bool touched);
  /// Activate the button via an action.
  void activate(TouchGUIButton *button);

  display::DisplayBuffer *get_display() const { return display_; }

  const Color &get_button_background_color() const { return this->button_background_color_; }
  const Color &get_button_active_background_color() const { return this->button_active_background_color_; }
  const Color &get_button_foreground_color() const { return this->button_foreground_color_; }
  const Color &get_button_active_foreground_color() const { return this->button_active_foreground_color_; }
  const Color &get_button_border_color() const { return this->button_border_color_; }

  display::Font *get_button_font() const { return this->button_font_; }

  optional<button_writer_t> &get_writer() { return this->writer_; }

 protected:
  void release_();
  void check_page_changed_();

  display::DisplayBuffer *display_;
  std::vector<TouchGUIButton *> buttons_;
  RadioGroupMap groups_;
  display::Font *button_font_{nullptr};
  Color button_background_color_{display::COLOR_OFF};
  Color button_active_background_color_{display::COLOR_ON};
  Color button_foreground_color_{display::COLOR_ON};
  Color button_active_foreground_color_{display::COLOR_OFF};
  Color button_border_color_{display::COLOR_ON};
  optional<button_writer_t> writer_{};
  CallbackManager<void()> update_callback_{};
  const display::DisplayPage *last_page_displayed_{nullptr};
};

/** Class representing an area on the screen sensitive to touch.
 *
 */
class TouchGUIButton : public binary_sensor::BinarySensor, public Component {
 public:
  void set_parent(TouchGUIComponent *parent) { this->parent_ = parent; }
  void set_type(TouchGUIButtonType t) { this->type_ = t; }
  void set_area(uint16_t x_min, uint16_t x_max, uint16_t y_min, uint16_t y_max) {
    this->x_min_ = x_min;
    this->x_max_ = x_max;
    this->y_min_ = y_min;
    this->y_max_ = y_max;
  }
  void set_pages(std::vector<const display::DisplayPage *> pages) { this->pages_ = pages; }
  void set_background_color(const Color &col) { this->background_color_ = col; }
  void set_active_background_color(const Color &col) { this->active_background_color_ = col; }
  void set_foreground_color(const Color &col) { this->foreground_color_ = col; }
  void set_active_foreground_color(const Color &col) { this->active_foreground_color_ = col; }
  void set_border_color(const Color &col) { this->border_color_ = col; }
  void set_font(display::Font *fnt) { this->font_ = fnt; }
  void set_radio_group(int g) { this->radio_group_ = g; }
  void set_initial(bool val) { this->initial_ = val; }
  void set_touch_time(uint32_t t) { this->touch_time_ = t; }

  void set_writer(button_writer_t &&writer) { this->writer_ = writer; }
  template<typename V> void set_text(V txt) { this->text_ = txt; }

  /// Get the background color for the inactive state, taking the parent's one if not overparametrized in the button
  /// itself.
  const Color &get_background_color() const {
    return this->background_color_.has_value() ? this->background_color_.value()
                                               : parent_->get_button_background_color();
  }
  /// Get the background color for the active state, taking the parent's one if not overparametrized in the button
  /// itself.
  const Color &get_active_background_color() const {
    return this->active_background_color_.has_value() ? this->active_background_color_.value()
                                                      : parent_->get_button_active_background_color();
  }
  /// Get the foreground color for the inactive state, taking the parent's one if not overparametrized in the button
  /// itself.
  const Color &get_foreground_color() const {
    return this->foreground_color_.has_value() ? this->foreground_color_.value()
                                               : parent_->get_button_foreground_color();
  }
  /// Get the foreground color for the active state, taking the parent's one if not overparametrized in the button
  /// itself.
  const Color &get_active_foreground_color() const {
    return this->active_foreground_color_.has_value() ? this->active_foreground_color_.value()
                                                      : parent_->get_button_active_foreground_color();
  }
  /// Get the color of the border.
  const Color &get_border_color() const {
    return this->border_color_.has_value() ? this->border_color_.value() : parent_->get_button_border_color();
  }

  /// Get the background color to be used for the current state.
  const Color &get_background_color_to_use() const {
    return this->state ? get_active_background_color() : get_background_color();
  }
  /// Get the foreground color to be used for the current state.
  const Color &get_foreground_color_to_use() const {
    return this->state ? get_active_foreground_color() : get_foreground_color();
  }

  /// Get the font to draw the text, taking the parent's one if not overparametrized in the button itself.
  display::Font *get_font() const { return this->font_ != nullptr ? this->font_ : parent_->get_button_font(); }

  TouchGUIButtonType get_type() const { return this->type_; }
  int get_radio_group() const { return this->radio_group_; }
  int get_x_min() const { return this->x_min_; }
  int get_x_max() const { return this->x_max_; }
  int get_y_min() const { return this->y_min_; }
  int get_y_max() const { return this->y_max_; }
  /// Get the x center of the sensitive area.
  int get_x_center() const { return (this->x_min_ + this->x_max_) / 2; }
  /// Get the y center of the sensitive area.
  int get_y_center() const { return (this->y_min_ + this->y_max_) / 2; }
  std::string get_text() { return this->text_.value(); }
  bool get_initial() const { return this->initial_; }

  display::DisplayBuffer *get_display() { return parent_->get_display(); }

  void setup() override;
  /// Setup of the buttons has to take place after the parent component.
  float get_setup_priority() const override { return setup_priority::PROCESSOR - 10.0; }

  /// Handles the time-dependent actions. Periodically called from the parent component.
  void update();
  void draw();
  /// Returns true if the button is visible on the currently displayed page.
  bool is_visible() const;
  /// Returns true if the button is visible on the currently displayed page and the coordinates are inside its sensitive
  /// area/
  bool is_interacted(int x, int y) const {
    return x >= this->x_min_ && x <= this->x_max_ && y >= this->y_min_ && y <= this->y_max_ && is_visible();
  }
  /// Returns true if the visual state of the element changed and clears the flag.
  bool should_update() {
    bool x = this->update_requested_;
    this->update_requested_ = false;
    return x;
  }

  // Track the real touch state. Does not change for programmatic activation  
  void set_touch_state(bool x) { this->touch_state_ = x; }
  bool is_touched() const { return this->touch_state_; }

 protected:
  TouchGUIComponent *parent_;
  TouchGUIButtonType type_;
  int radio_group_{0};
  uint16_t x_min_, x_max_, y_min_, y_max_;
  bool touch_state_{false};
  uint32_t touch_time_;
  std::vector<const display::DisplayPage *> pages_;
  display::Font *font_{nullptr};
  optional<Color> background_color_{};
  optional<Color> active_background_color_{};
  optional<Color> foreground_color_{};
  optional<Color> active_foreground_color_{};
  optional<Color> border_color_{};
  optional<button_writer_t> writer_{};
  TemplatableStringValue<> text_;
  bool initial_{false};
  unsigned long activation_time_{0};
  bool update_requested_{false};
};

template<typename... Ts> class TouchGUITouchAction : public Action<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint16_t, x)
  TEMPLATABLE_VALUE(uint16_t, y)
  TEMPLATABLE_VALUE(bool, touched)

  void set_gui(TouchGUIComponent *gui) { this->gui_ = gui; }
  void play(Ts... x) override {
    auto x_val = this->x_.value(x...);
    auto y_val = this->y_.value(x...);
    auto t_val = this->touched_.value(x...);
    this->gui_->touch(x_val, y_val, t_val);
  }

 protected:
  TouchGUIComponent *gui_;
  TouchGUIButton *button_;
};

template<typename... Ts> class TouchGUIActivateButtonAction : public Action<Ts...> {
 public:
  TEMPLATABLE_VALUE(TouchGUIButton *, button)

  void set_gui(TouchGUIComponent *gui) { this->gui_ = gui; }
  void play(Ts... x) override {
    auto button = this->button_.value(x...);
    this->gui_->activate(button);
  }

 protected:
  TouchGUIComponent *gui_;
};

class TouchGuiUpdateTrigger : public Trigger<> {
 public:
  explicit TouchGuiUpdateTrigger(TouchGUIComponent *parent) {
    parent->add_on_update_callback([this]() { this->trigger(); });
  }
};

}  // namespace touch_gui
}  // namespace esphome
