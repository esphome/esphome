#pragma once

#include <utility>

#include "esphome/components/graphical_layout/graphical_layout.h"
#include "esphome/components/font/font.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome {
namespace graphical_layout {

/** The TextPanel is a UI item that renders a single line of text to a display */
class TextPanel : public LayoutItem {
 public:
  display::Rect measure_item_internal(display::Display *display) override;
  void render_internal(display::Display *display, display::Rect bounds) override;
  void dump_config(int indent_depth, int additional_level_depth) override;
  void setup_complete() override;

  template<typename V> void set_text(V text) { this->text_input_ = text; };
  void set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; };
  void set_text_sensor(text_sensor::TextSensor *text_sensor) { this->text_sensor_ = text_sensor; };
  void set_time(time::RealTimeClock *time) { this->time_ = time; };
  void set_use_utc_time(bool use_utc_time) { this->use_utc_time_ = use_utc_time; };
  template<typename V> void set_time_format(V time_format) { this->time_format_ = time_format; };
  template<typename V> void set_text_formatter(V text_formatter) { this->text_formatter_ = text_formatter; };
  void set_font(display::BaseFont *font) { this->font_ = font; };
  void set_foreground_color(Color foreground_color) { this->foreground_color_ = foreground_color; };
  void set_background_color(Color background_color) { this->background_color_ = background_color; };
  void set_text_align(display::TextAlign text_align) { this->text_align_ = text_align; };

 protected:
  TemplatableValue<std::string> text_{};
  sensor::Sensor *sensor_{nullptr};
  text_sensor::TextSensor *text_sensor_{nullptr};
  time::RealTimeClock *time_{nullptr};
  bool use_utc_time_{false};
  TemplatableValue<std::string> time_format_{};
  TemplatableValue<std::string, const std::string> text_formatter_{};
  TemplatableValue<std::string> text_input_{};
  display::BaseFont *font_{nullptr};
  display::TextAlign text_align_{display::TextAlign::TOP_LEFT};
  Color foreground_color_{COLOR_ON};
  Color background_color_{COLOR_OFF};
};

}  // namespace graphical_layout
}  // namespace esphome
